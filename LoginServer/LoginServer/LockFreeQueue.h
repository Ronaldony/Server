#ifndef __LOCKFREE_QUEUE_H__ 
#define __LOCKFREE_QUEUE_H__ 

#include "LockfreeMemoryPool.h"

#define dfLOCKFREE_QUEUE_MASKING_ADDR		0xFFFFFFFFFFFF
#define dfLOCKFREE_QUEUE_MASKING_COUNT		0xFFFF000000000000
#define dfLOCKFREE_QUEUE_MASKING_COUNT_ADD	0x0001000000000000
#define dfLOCKFREE_QUEUE_DEBUG_MAX  		0x7FFF

template <typename T>
class CLockFreeQueue
 {
 public:
    struct Node
    {
        T data;
        Node* next;
    };

 private:
     CLockFreeMemoryPool<Node>  _MemmoryPool;
     LONG64 alignas(64) _size;
     Node* alignas(64) _head;        // ���۳�带 ����Ʈ�Ѵ�.
     Node* alignas(64) _tail;        // ��������带 ����Ʈ�Ѵ�.

 public:
     CLockFreeQueue()
     {
         _size = 0;
         _head = _MemmoryPool.Alloc();
         _head->next = NULL;
         _tail = _head;
     }

     LONG64 GetUseSize() { return _size; }
     LONG64 GetPoolSize() { return _MemmoryPool.GetPoolSize(); }

     inline static LONG64		GetTotalCapacityCount(void) { return CLockFreeMemoryPool<Node>::GetTotalCapacityCount(); }
     inline static LONG64		GetTotalPoolSize(void) { return CLockFreeMemoryPool<Node>::GetTotalPoolSize(); }
 
     void Enqueue(T data)
     {
         Node* pNode = _MemmoryPool.Alloc();

         pNode->data = data;
         pNode->next = NULL;

         while(true)
         {
             // ABA ������ ����Ͽ� 64bit ������ �ּ� ��� ��Ʈ�� ����ŷ�Ͽ� Tail�� next ���´�.
             Node* pTempTail = _tail;
             Node* tail = (Node*)((ULONG_PTR)pTempTail & dfLOCKFREE_QUEUE_MASKING_ADDR);
             Node* next = tail->next;

             // �Ʒ� ���� �ð��� �� Interlocked ����� �ִ��� �������� �ʱ� ���� ���ǹ�
             if (NULL == next)
             {
                 // 1�� ����: tail�� next�� ���ο� ��带 �ȴ� ����
                 if (NULL == InterlockedCompareExchangePointer((PVOID*)&tail->next, pNode, next))
                 {
                     Node* pNewNode = (Node*)((((ULONG_PTR)pTempTail + dfLOCKFREE_QUEUE_MASKING_COUNT_ADD) & dfLOCKFREE_QUEUE_MASKING_COUNT) | (ULONG_PTR)(pNode));

                     // 2�� ����: tail�� ���ο� tail�� �����ϴ� ����
                     InterlockedCompareExchangePointer((PVOID*)&_tail, pNewNode, pTempTail);
                     break;
                 }
             }
             else
             {
                Node* pNewNode = (Node*)((((ULONG_PTR)pTempTail + dfLOCKFREE_QUEUE_MASKING_COUNT_ADD) & dfLOCKFREE_QUEUE_MASKING_COUNT) | (ULONG_PTR)(next));
                InterlockedCompareExchangePointer((PVOID*)&_tail, pNewNode, pTempTail);
             }
         }

         InterlockedExchangeAdd64(&_size, 1);
     }
 
     int Dequeue(T& data)
     {
         while(true)
         {
             Node* tempHead = _head;
             Node* head = (Node*)((ULONG_PTR)tempHead & dfLOCKFREE_QUEUE_MASKING_ADDR);
             Node* next = head->next;

             if (next == NULL)
             {
                 // ť�� ����ִ� ����
                 return 0;
             }
             else
             {
                 Node* tail = (Node*)((ULONG_PTR)_tail & dfLOCKFREE_QUEUE_MASKING_ADDR);
                 if (tail == head)
                 {
                     // Head�� Tail�� ���� �� Tail�� Tail->next�� �����Ͽ� Head�� Tail�� �����ϴ� ��Ȳ�� ���´�.
                     Node* tempTailNext = (Node*)((((ULONG_PTR)_tail + dfLOCKFREE_QUEUE_MASKING_COUNT_ADD) & dfLOCKFREE_QUEUE_MASKING_COUNT) | (ULONG_PTR)(tail->next));
                     if (tempHead != InterlockedCompareExchangePointer((PVOID*)&_tail, tempTailNext, tempHead))
                         continue;
                 }

                 // Head�� ��ü�ϱ� ���� data�� �̸� �����ص��� �ʰ� Head ��ü �� data�� �����ϰ� �Ǹ�
                 // �ٸ� �����忡�� next ��带 ������� DeQ, EnQ �ϴ� ��Ȳ�� �Ǿ��� �� next->data�� �ߺ� DeQ �� �� �ִ�.
                 data = next->data;

                 Node* pNewHead = (Node*)((((ULONG_PTR)tempHead + dfLOCKFREE_QUEUE_MASKING_COUNT_ADD) & dfLOCKFREE_QUEUE_MASKING_COUNT) | (ULONG_PTR)(next));

                 if (tempHead == InterlockedCompareExchangePointer((PVOID*)&_head, pNewHead, tempHead))
                 {
                     _MemmoryPool.Free(head);
                     break;
                 }
             }
         }
         InterlockedExchangeAdd64(& _size, -1);
         return 1;
     }
 };

#endif
