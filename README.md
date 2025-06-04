# OS-Good

[ 학번: *****504, 이름: 한상훈 ]

### 진행률

```
2025.05.28 | All: 기본 skeleton File 저장
           |           | ● Item* heap: 우선순위 queue 동적 배열(힙 구조). 삽입/삭제 효율적 처리
           |           | ● std::atomic<int> capacity: 할당된 heap 배열 최대 크기를 원자적으로 저장. 무결성 보장
           |  qtype.h  | ● std::atomic<int> size: queue에 저장된 item 개수를 원자적으로 관리
           |           | ● std::mutex enqueue_mtx: 삽입 작업 동기화 lock. 경쟁 상태 억제 / queue.cpp 단일 mutex 적용(임시)
           |           | ● std::mutex dequeue_mtx: 삭제 작업 동기화 lock. heap 구조 무결성 유지 / queue.cpp 단일 mutex 적용(임시)
           -------------
           |           | ● 배열 기반 heap 동적 관리, 멀티스레드 동기화는 단일 mutex 처리
           | queue.cpp | ● 원자 변수로 크기/용량 관리해 동시 접근 안정성 확보
           |           | ● 삽입/삭제 시 heap 속성 유지 위한 heap 정렬 구현
```

```
2025.06.03 |           | ● 단일 mutex에서 다중 mutex 사용으로 처리율 향상 및 병목 현상 감소
           |           | ● enqueue, dequeue, range 깊은 복사 추가/수정, range 추가
           | queue.cpp | ● 중복 key에 대한 유일무이 가정 추가
           |           | ● lock 순서 지정으로 메모리 참조 오류 예방
           |           | ● atomic load/store에 memory_order 지정(메모리 가시성 및 스레드 간 동기화 명확화)
           -------------
           |  qtype.h  | ● heap 배열의 크기 확장 작업을 thread-safe하게 보호하기 위해 resize_mtx를 추가
           |           | ● Item 구조체에 value 데이터 크기 저장용 size 필드를 추가하여 정확한 깊은 복사 및 메모리 관리를 구현
```
```
2025.06.04 | queue.cpp | ● heap[0]의 shallow copy를 deep copy로 수정하여 dequeue()의 메모리 오류 방지
```