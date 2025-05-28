# OS-Good

[ 학번: *****504, 이름: 한상훈 ]

### 진행률

```
2025.05.28 | All: 기본 skeleton File 저장
           |           | ● Item* heap: 우선순위 queue 동적 배열(힙 구조). 삽입/삭제 효율적 처리
           |           | ● std::atomic<int> capacity: 할당된 heap 배열 최대 크기를 원자적으로 저장. 무결성 보장
           |  qtype.h  | ● std::atomic<int> size: queue에 저장된 item 개수를 원자적으로 관리
           |           | ● std::mutex enqueue_mtx: 삽입 작업 동기화 lock. 경쟁 상태 억제
           |           | ● std::mutex dequeue_mtx: 삭제 작업 동기화 lock. heap 구조 무결성 유지
           -------------
           |           | ● 배열 기반 heap 동적 관리, 멀티스레드 동기화는 단인 mutex 처리
           | queue.cpp | ● 원자 변수로 크기/용량 관리해 동시 접근 안정성 확보
           |           | ● 삽입/삭제 시 heap 속성 유지 위한 heap 정렬 구현
```