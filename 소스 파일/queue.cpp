#include <iostream>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include "queue.h"

struct QueueImpl {
    Item* heap;
    std::atomic<int> capacity;
    std::atomic<int> size;
    std::mutex mtx;
};

static QueueImpl* to_impl(Queue* q) {
    return reinterpret_cast<QueueImpl*>(q);
}

static int parent(int i) { return (i - 1) / 2; }
static int left(int i) { return 2 * i + 1; }
static int right(int i) { return 2 * i + 2; }

static void swap_items(Item& a, Item& b) {
    Item tmp = a;
    a = b;
    b = tmp;
}

static void heapify_up(QueueImpl* pq, int idx) {
    while (idx > 0) {
        int p = parent(idx);
        if (pq->heap[p].key >= pq->heap[idx].key) break;
        swap_items(pq->heap[p], pq->heap[idx]);
        idx = p;
    }
}

static void heapify_down(QueueImpl* pq, int idx) {
    int sz = pq->size.load();
    while (true) {
        int l = left(idx);
        int r = right(idx);
        int largest = idx;
        if (l < sz && pq->heap[l].key > pq->heap[largest].key) largest = l;
        if (r < sz && pq->heap[r].key > pq->heap[largest].key) largest = r;
        if (largest == idx) break;
        swap_items(pq->heap[idx], pq->heap[largest]);
        idx = largest;
    }
}

static void resize_heap(QueueImpl* pq, int new_capacity) {
    Item* new_heap = new Item[new_capacity];
    int sz = pq->size.load();
    for (int i = 0; i < sz; ++i) {
        new_heap[i].key = pq->heap[i].key;
        size_t len = strlen((char*)pq->heap[i].value) + 1;
        new_heap[i].value = malloc(len);
        if (new_heap[i].value == nullptr) {
            std::cerr << "resize_heap 함수에서 malloc 실패\n";
            std::exit(EXIT_FAILURE);
        }
        memcpy((char*)new_heap[i].value, (char*)pq->heap[i].value, len);
        free(pq->heap[i].value);
    }
    delete[] pq->heap;
    pq->heap = new_heap;
    pq->capacity.store(new_capacity);
}

Queue* init(void) {
    QueueImpl* pq = new QueueImpl;
    pq->capacity = 1024;
    pq->size = 0;
    pq->heap = new Item[pq->capacity];
    return reinterpret_cast<Queue*>(pq);
}

void release(Queue* queue) {
    if (!queue) return;
    QueueImpl* pq = to_impl(queue);
    for (int i = 0; i < pq->size.load(); ++i) {
        free(pq->heap[i].value);
    }
    delete[] pq->heap;
    delete pq;
}

Reply enqueue(Queue* queue, Item item) {
    Reply reply = { false, {0, nullptr} };
    if (!queue) return reply;
    QueueImpl* pq = to_impl(queue);

    std::lock_guard<std::mutex> lock(pq->mtx);

    int sz = pq->size.load();
    int cap = pq->capacity.load();

    // 중복 key 검사 및 갱신
    for (int i = 0; i < sz; ++i) {
        if (pq->heap[i].key == item.key) {
            free(pq->heap[i].value);
            size_t len = strlen((char*)item.value) + 1;
            pq->heap[i].value = malloc(len);
            if (pq->heap[i].value == nullptr) {
                std::cerr << "enqueue 함수에서 malloc 실패\n";
                std::exit(EXIT_FAILURE);
            }
            memcpy((char*)pq->heap[i].value, (char*)item.value, len);

            reply.success = true;
            reply.item.key = item.key;
            reply.item.value = malloc(len);
            if (reply.item.value == nullptr) {
                std::cerr << "enqueue 함수에서 reply용 malloc 실패\n";
                std::exit(EXIT_FAILURE);
            }
            memcpy((char*)reply.item.value, (char*)item.value, len);
            return reply;
        }
    }

    if (sz >= cap) {
        resize_heap(pq, cap * 2);
    }

    size_t len = strlen((char*)item.value) + 1;
    pq->heap[sz].key = item.key;
    pq->heap[sz].value = malloc(len);
    if (pq->heap[sz].value == nullptr) {
        std::cerr << "enqueue 함수에서 새로운 아이템 malloc 실패\n";
        std::exit(EXIT_FAILURE);
    }
    memcpy((char*)pq->heap[sz].value, (char*)item.value, len);
    pq->size.store(sz + 1);

    heapify_up(pq, sz);

    reply.success = true;
    reply.item.key = item.key;
    reply.item.value = malloc(len);
    if (reply.item.value == nullptr) {
        std::cerr << "enqueue 함수에서 reply용 malloc 실패\n";
        std::exit(EXIT_FAILURE);
    }
    memcpy((char*)reply.item.value, (char*)item.value, len);
    return reply;
}

Reply dequeue(Queue* queue) {
    Reply reply = { false, {0, nullptr} };
    if (!queue) return reply;
    QueueImpl* pq = to_impl(queue);

    std::lock_guard<std::mutex> lock(pq->mtx);

    int sz = pq->size.load();
    if (sz <= 0) return reply;

    Item ret = pq->heap[0];
    pq->size.store(sz - 1);

    if (pq->size.load() > 0) {
        pq->heap[0] = pq->heap[pq->size.load()];
        heapify_down(pq, 0);
    }

    size_t len = strlen((char*)ret.value) + 1;
    reply.success = true;
    reply.item.key = ret.key;
    reply.item.value = malloc(len);
    if (reply.item.value == nullptr) {
        std::cerr << "dequeue 함수에서 malloc 실패\n";
        std::exit(EXIT_FAILURE);
    }
    memcpy((char*)reply.item.value, (char*)ret.value, len);

    return reply;
}

Queue* range(Queue* queue, Key start, Key end) {
    if (!queue) return nullptr;
    QueueImpl* pq = to_impl(queue);

    // 1. 원본 큐 잠금: 범위 내 아이템 수 먼저 셈
    int sz = pq->size.load();
    int* matched_indices = new int[sz];
    int count = 0;

    {
        std::lock_guard<std::mutex> lock(pq->mtx);
        for (int i = 0; i < sz; ++i) {
            if (pq->heap[i].key >= start && pq->heap[i].key <= end) {
                matched_indices[count++] = i;
            }
        }
    }

    // 2. 새 큐 초기화 및 충분한 용량 확보
    Queue* new_queue = init();
    QueueImpl* new_pq = to_impl(new_queue);

    if (count > new_pq->capacity.load()) {
        resize_heap(new_pq, count * 2);
    }

    // 3. 새 큐 잠금 후 깊은 복사
    {
        std::lock_guard<std::mutex> lock(new_pq->mtx);
        for (int i = 0; i < count; ++i) {
            int idx = matched_indices[i];
            Item& orig_item = pq->heap[idx];
            Item new_item;
            size_t len = strlen((char*)orig_item.value) + 1;
            new_item.key = orig_item.key;
            new_item.value = malloc(len);
            if (new_item.value == nullptr) {
                std::cerr << "range 함수에서 malloc 실패\n";
                std::exit(EXIT_FAILURE);
            }
            memcpy(new_item.value, orig_item.value, len);

            new_pq->heap[i] = new_item;
        }
        new_pq->size.store(count);

        // 4. bottom-up heapify_down으로 힙 재구성
        for (int i = count / 2 - 1; i >= 0; --i) {
            heapify_down(new_pq, i);
        }
    }

    delete[] matched_indices;
    return new_queue;
}

