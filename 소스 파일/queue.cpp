#include <iostream>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include "queue.h"

static int parent(int i) { return (i - 1) / 2; }
static int left(int i) { return 2 * i + 1; }
static int right(int i) { return 2 * i + 2; }

static void swap_items(Item& a, Item& b) {
    Item tmp = a;
    a = b;
    b = tmp;
}

static void heapify_up(Queue* pq, int idx) {
    while (idx > 0) {
        int p = parent(idx);
        if (pq->heap[p].key >= pq->heap[idx].key) break;
        swap_items(pq->heap[p], pq->heap[idx]);
        idx = p;
    }
}

static void heapify_down(Queue* pq, int idx) {
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

static void resize_heap(Queue* pq, int new_capacity) {
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
    Queue* pq = new Queue;
    pq->capacity.store(16384, std::memory_order_release);
    pq->size.store(0, std::memory_order_release);
    pq->heap = new Item[pq->capacity.load()];
    return pq;
}

void release(Queue* queue) {
    if (!queue) return;
    int sz = queue->size.load(std::memory_order_acquire);
    for (int i = 0; i < sz; ++i) {
        free(queue->heap[i].value);
    }
    delete[] queue->heap;
    delete queue;
}

Reply enqueue(Queue* queue, Item item) {
    Reply reply = { false, {0, nullptr, 0} };
    if (!queue) return reply;

    // 락 순서 : resize -> enqueue
    {
        std::lock_guard<std::mutex> lock_resize(queue->resize_mtx);
        std::lock_guard<std::mutex> lock_enq(queue->enqueue_mtx);

        int sz = queue->size.load(std::memory_order_acquire);
        int cap = queue->capacity.load(std::memory_order_acquire);

        // 중복 key 검사 및 갱신
        for (int i = 0; i < sz; ++i) {
            if (queue->heap[i].key == item.key) {
                if (queue->heap[i].size != item.size) {
                    free(queue->heap[i].value);
                    queue->heap[i].value = malloc(item.size);
                    if (!queue->heap[i].value) {
                        std::cerr << "enqueue malloc 실패\n";
                        std::exit(EXIT_FAILURE);
                    }
                }
                memcpy(queue->heap[i].value, item.value, item.size);
                queue->heap[i].size = item.size;

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

        // 리사이징 필요 시
        if (sz >= cap * 9 / 10) {
            resize_heap(queue, cap * 2);
            cap = queue->capacity.load(std::memory_order_acquire);
        }

        size_t len = item.size;
        queue->heap[sz].key = item.key;
        queue->heap[sz].value = malloc(len);
        if (!queue->heap[sz].value) {
            std::cerr << "enqueue malloc 실패\n";
            std::exit(EXIT_FAILURE);
        }
        memcpy(queue->heap[sz].value, item.value, len);
        queue->heap[sz].size = len;
        queue->size.store(sz + 1, std::memory_order_release);

        heapify_up(queue, sz);

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

    std::lock_guard<std::mutex> lock_deq(queue->dequeue_mtx);

    int sz = queue->size.load(std::memory_order_acquire);
    if (sz <= 0) return reply;

    // 깊은 복사
    Item ret;
    ret.key = queue->heap[0].key;
    ret.size = queue->heap[0].size;
    ret.value = malloc(ret.size);
    if (!ret.value) {
        std::cerr << "dequeue: malloc 실패 (ret)\n";
        std::exit(EXIT_FAILURE);
    }
    memcpy(ret.value, queue->heap[0].value, ret.size);

    int new_sz = sz - 1;
    if (new_sz > 0) {
        queue->heap[0] = queue->heap[new_sz];
        heapify_down(queue, 0);
    }
    queue->size.store(new_sz, std::memory_order_release);

    reply.success = true;
    reply.item.key = ret.key;
    reply.item.size = ret.size;
    reply.item.value = malloc(ret.size);
    if (!reply.item.value) {
        std::cerr << "dequeue: malloc 실패 (reply)\n";
        std::exit(EXIT_FAILURE);
    }
    memcpy(reply.item.value, ret.value, ret.size);

    free(ret.value);
    return reply;
}

Queue* range(Queue* queue, Key start, Key end) {
    if (!queue) return nullptr;

    int sz = queue->size.load(std::memory_order_acquire);
    int* matched_indices = new int[sz];
    int count = 0;

    {
        std::lock_guard<std::mutex> lock_resize(queue->resize_mtx);
        for (int i = 0; i < sz; ++i) {
            if (queue->heap[i].key >= start && queue->heap[i].key <= end) {
                matched_indices[count++] = i;
            }
        }
    }

    Queue* new_queue = init();

    if (count > new_queue->capacity.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock_enq(new_queue->enqueue_mtx);
        std::lock_guard<std::mutex> lock_deq(new_queue->dequeue_mtx);
        std::lock_guard<std::mutex> lock_resize(new_queue->resize_mtx);
        resize_heap(new_queue, count * 2);
    }

    {
        std::lock_guard<std::mutex> lock_enq(new_queue->enqueue_mtx);
        std::lock_guard<std::mutex> lock_deq(new_queue->dequeue_mtx);
        for (int i = 0; i < count; ++i) {
            int idx = matched_indices[i];
            Item& orig_item = queue->heap[idx];
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

            new_queue->heap[i] = new_item;
        }
        new_queue->size.store(count, std::memory_order_release);

        for (int i = count / 2 - 1; i >= 0; --i) {
            heapify_down(new_queue, i);
        }
    }

    delete[] matched_indices;
    return new_queue;
}