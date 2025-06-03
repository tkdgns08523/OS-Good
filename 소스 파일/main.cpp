#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include "queue.h"

using namespace std;

#define REQUEST_PER_CLIENT 10000
#define NUM_CLIENTS 32

atomic<unsigned long long> sum_key = 0;
atomic<unsigned long long> sum_value = 0;

typedef enum {
    GET,
    SET,
    GETRANGE
} Operation;

typedef struct {
    Operation op;
    Item item;
} Request;

void client_func(Queue* queue, Request requests[], int n_request) {
    Reply reply = { false, {0, nullptr, 0} };

    for (int i = 0; i < n_request; i++) {
        if (requests[i].op == GET) {
            reply = dequeue(queue);
        }
        else {
            reply = enqueue(queue, requests[i].item);
        }

        if (reply.success) {
            sum_key += reply.item.key;

            if (reply.item.size == sizeof(int) && reply.item.value != nullptr) {
                int val = *(int*)(reply.item.value);  // 실제 int 값 읽기
                sum_value += val;
            }

            free(reply.item.value);
        }
    }
}

int main() {
    srand((unsigned int)time(NULL));

    const int total_requests = NUM_CLIENTS * REQUEST_PER_CLIENT;
    Request* all_requests = new Request[total_requests];

    for (int c = 0; c < NUM_CLIENTS; c++) {
        int offset = c * REQUEST_PER_CLIENT;
        for (int i = 0; i < REQUEST_PER_CLIENT / 2; i++) {
            all_requests[offset + i].op = SET;
            all_requests[offset + i].item.key = i;

            int* pval = (int*)malloc(sizeof(int));
            if (!pval) {
                cerr << "malloc 실패" << endl;
                exit(EXIT_FAILURE);
            }
            *pval = rand() % 1000000;
            all_requests[offset + i].item.value = pval;
            all_requests[offset + i].item.size = sizeof(int);
        }
        for (int i = REQUEST_PER_CLIENT / 2; i < REQUEST_PER_CLIENT; i++) {
            all_requests[offset + i].op = GET;
            all_requests[offset + i].item.key = 0;
            all_requests[offset + i].item.value = nullptr;
            all_requests[offset + i].item.size = 0;
        }
    }

    Queue* queue = init();
    if (!queue) {
        cerr << "큐 초기화 실패" << endl;
        delete[] all_requests;
        return -1;
    }

    auto start_time = chrono::high_resolution_clock::now();

    thread* clients = new thread[NUM_CLIENTS];
    for (int c = 0; c < NUM_CLIENTS; c++) {
        clients[c] = thread(client_func, queue, &all_requests[c * REQUEST_PER_CLIENT], REQUEST_PER_CLIENT);
    }
    for (int c = 0; c < NUM_CLIENTS; c++) {
        clients[c].join();
    }
    delete[] clients;

    release(queue);

    for (int i = 0; i < total_requests; i++) {
        if (all_requests[i].op == SET && all_requests[i].item.value != nullptr) {
            free(all_requests[i].item.value);
            all_requests[i].item.value = nullptr;
        }
    }
    delete[] all_requests;

    chrono::duration<double> elapsed = chrono::high_resolution_clock::now() - start_time;
    double seconds = elapsed.count();
    double throughput = total_requests / seconds;

    cout << "반환된 키 값 합계 = " << sum_key << endl;
    cout << "반환된 값 합계 = " << sum_value << endl;
    cout << "총 처리 요청 수 = " << total_requests << endl;
    cout << "경과 시간(초) = " << seconds << endl;
    cout << "평균 처리율(요청/초) = " << throughput << endl;

    return 0;
}
