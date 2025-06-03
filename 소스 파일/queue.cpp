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

    std::mutex enqueue_mtx;
    std::mutex dequeue_mtx;
    std::mutex resize_mtx;
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
    int sz = pq->size.load(std::memory_order_acquire);
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
    int sz = pq->size.load(std::memory_order_acquire);

    for (int i = 0; i < sz; ++i) {
        new_heap[i].key = pq->heap[i].key;
        size_t len = pq->heap[i].size;
        new_heap[i].value = malloc(len);
        if (!new_heap[i].value) {
            std::cerr << "resize_heap: malloc 실패\n";
            std::exit(EXIT_FAILURE);
        }
        memcpy(new_heap[i].value, pq->heap[i].value, len);
        new_heap[i].size = len;
        free(pq->heap[i].value);
    }
    delete[] pq->heap;
    pq->heap = new_heap;
    pq->capacity.store(new_capacity, std::memory_order_release);
}

Queue* init(void) {
    QueueImpl* pq = new QueueImpl;
    pq->capacity.store(16384, std::memory_order_release);
    pq->size.store(0, std::memory_order_release);
    pq->heap = new Item[pq->capacity.load()];
    return reinterpret_cast<Queue*>(pq);
}

void release(Queue* queue) {
    if (!queue) return;
    QueueImpl* pq = to_impl(queue);
    int sz = pq->size.load(std::memory_order_acquire);
    for (int i = 0; i < sz; ++i) {
        free(pq->heap[i].value);
    }
    delete[] pq->heap;
    delete pq;
}

Reply enqueue(Queue* queue, Item item) {
    Reply reply = { false, {0, nullptr, 0} };
    if (!queue) return reply;
    QueueImpl* pq = to_impl(queue);

    // 락 순서 : resize -> enqueue
    {
        std::lock_guard<std::mutex> lock_resize(pq->resize_mtx);
        std::lock_guard<std::mutex> lock_enq(pq->enqueue_mtx);

        int sz = pq->size.load(std::memory_order_acquire);
        int cap = pq->capacity.load(std::memory_order_acquire);

        // 중복 key 검사 및 갱신
        for (int i = 0; i < sz; ++i) {
            if (pq->heap[i].key == item.key) {
                if (pq->heap[i].size != item.size) {
                    free(pq->heap[i].value);
                    pq->heap[i].value = malloc(item.size);
                    if (!pq->heap[i].value) {
                        std::cerr << "enqueue malloc 실패\n";
                        std::exit(EXIT_FAILURE);
                    }
                }
                memcpy(pq->heap[i].value, item.value, item.size);
                pq->heap[i].size = item.size;

                reply.success = true;
                reply.item.key = item.key;
                reply.item.value = malloc(item.size);
                if (!reply.item.value) {
                    std::cerr << "enqueue reply malloc 실패\n";
                    std::exit(EXIT_FAILURE);
                }
                memcpy(reply.item.value, item.value, item.size);
                reply.item.size = item.size;
                return reply;
            }
        }

        // 리사이징이 필요하면 수행
        if (sz >= cap * 9 / 10) {
            resize_heap(pq, cap * 2);
            cap = pq->capacity.load(std::memory_order_acquire);
        }

        size_t len = item.size;
        pq->heap[sz].key = item.key;
        pq->heap[sz].value = malloc(len);
        if (!pq->heap[sz].value) {
            std::cerr << "enqueue malloc 실패\n";
            std::exit(EXIT_FAILURE);
        }
        memcpy(pq->heap[sz].value, item.value, len);
        pq->heap[sz].size = len;
        pq->size.store(sz + 1, std::memory_order_release);

        heapify_up(pq, sz);

        reply.success = true;
        reply.item.key = item.key;
        reply.item.value = malloc(len);
        if (!reply.item.value) {
            std::cerr << "enqueue reply malloc 실패\n";
            std::exit(EXIT_FAILURE);
        }
        memcpy(reply.item.value, item.value, len);
        reply.item.size = len;
    }
    return reply;
}

Reply dequeue(Queue* queue) {
    Reply reply = { false, {0, nullptr, 0} };
    if (!queue) return reply;
    QueueImpl* pq = to_impl(queue);

    std::lock_guard<std::mutex> lock_deq(pq->dequeue_mtx);

    int sz = pq->size.load(std::memory_order_acquire);
    if (sz <= 0) return reply;

    Item ret = pq->heap[0];
    pq->size.store(sz - 1, std::memory_order_release);

    if (pq->size.load(std::memory_order_acquire) > 0) {
        pq->heap[0] = pq->heap[pq->size.load(std::memory_order_acquire)];
        heapify_down(pq, 0);
    }

    size_t len = ret.size;
    reply.success = true;
    reply.item.key = ret.key;
    reply.item.value = malloc(len);
    if (!reply.item.value) {
        std::cerr << "dequeue malloc 실패\n";
        std::exit(EXIT_FAILURE);
    }
    memcpy(reply.item.value, ret.value, len);
    reply.item.size = len;

    return reply;
}

Queue* range(Queue* queue, Key start, Key end) {
    if (!queue) return nullptr;
    QueueImpl* pq = to_impl(queue);

    int sz = pq->size.load(std::memory_order_acquire);
    int* matched_indices = new int[sz];
    int count = 0;

    {
        std::lock_guard<std::mutex> lock_resize(pq->resize_mtx);
        for (int i = 0; i < sz; ++i) {
            if (pq->heap[i].key >= start && pq->heap[i].key <= end) {
                matched_indices[count++] = i;
            }
        }
    }

    Queue* new_queue = init();
    QueueImpl* new_pq = to_impl(new_queue);

    if (count > new_pq->capacity.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock_enq(new_pq->enqueue_mtx);
        std::lock_guard<std::mutex> lock_deq(new_pq->dequeue_mtx);
        std::lock_guard<std::mutex> lock_resize(new_pq->resize_mtx);
        resize_heap(new_pq, count * 2);
    }

    {
        std::lock_guard<std::mutex> lock_enq(new_pq->enqueue_mtx);
        std::lock_guard<std::mutex> lock_deq(new_pq->dequeue_mtx);
        for (int i = 0; i < count; ++i) {
            int idx = matched_indices[i];
            Item& orig_item = pq->heap[idx];
            Item new_item;
            size_t len = orig_item.size;
            new_item.key = orig_item.key;
            new_item.value = malloc(len);
            if (!new_item.value) {
                std::cerr << "range malloc 실패\n";
                std::exit(EXIT_FAILURE);
            }
            memcpy(new_item.value, orig_item.value, len);
            new_item.size = len;

            new_pq->heap[i] = new_item;
        }
        new_pq->size.store(count, std::memory_order_release);

        for (int i = count / 2 - 1; i >= 0; --i) {
            heapify_down(new_pq, i);
        }
    }

    delete[] matched_indices;
    return new_queue;
}
