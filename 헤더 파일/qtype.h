#ifndef _QTYPE_H  // header guard
#define _QTYPE_H
#include <atomic>
#include <mutex>
// ==========이 파일은 수정 가능==========

typedef unsigned int Key;  // 값이 클수록 높은 우선순위
typedef void* Value;

typedef struct {
    Key key;
    Value value;
} Item;

typedef struct {
    bool success;   // true: 성공, false: 실패
    Item item;
    // 필드 추가 가능
} Reply;

typedef struct node_t {
    Item item;
    struct node_t* next;
    // 필드 추가 가능
} Node;

typedef struct {
    Node* head, tail;
    Item* heap;
    std::atomic<int> capacity;
    std::atomic<int> size;

    std::mutex enqueue_mtx;
    std::mutex dequeue_mtx;
    // 필드 추가 가능
} Queue;

// 이후 자유롭게 추가/수정: 새로운 자료형 정의 등

#endif
