#include <iostream>
#include <mutex>
#include <atomic>
#include <cstring> 
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

    memcpy(new_heap, pq->heap, sizeof(Item) * sz);

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
    delete[] pq->heap;
    delete pq;
}


Node* nalloc(Item item) {
	// Node 생성, item으로 초기화
	return nullptr;
}


void nfree(Node* node) {
	return;
}


Node* nclone(Node* node) {
	return nullptr;
}


Reply enqueue(Queue* queue, Item item) {
    Reply reply = { false, {0, nullptr} };
    if (!queue) return reply;
    QueueImpl* pq = to_impl(queue);

    std::lock_guard<std::mutex> lock(pq->mtx);

    int sz = pq->size.load();
    int cap = pq->capacity.load();

    if (sz >= cap) {
        resize_heap(pq, cap * 2);
        cap = pq->capacity.load();
    }

    pq->heap[sz] = item;
    pq->size.store(sz + 1);

    heapify_up(pq, sz);

    reply.success = true;
    reply.item = item;
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

    reply.success = true;
    reply.item = ret;
    return reply;
}

Queue* range(Queue* queue, Key start, Key end) {
	return nullptr;
}
