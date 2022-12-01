#ifndef  __LOCKFREE_STACK_H__
#define  __LOCKFREE_STACK_H__
//#include <Windows.h>
#include "LockfreeMemoryPool.h"

template <class T>
class CLockFreeStack
{
public:
	struct Node
	{
		T data;
		Node* next;
	};

private:
	inline static CLockFreeMemoryPool<Node> _MemmoryPool;	// 같은 타입의 락프리 스택들은 메모리풀을 공유
	Node*			_pFreeNode;								// 스택 Top 오브젝트 블럭

public:

	CLockFreeStack()
	{
		_pFreeNode = NULL;
	}

	~CLockFreeStack()
	{
		// 처리 보류
		Node* pDeleteNode;
		Node* pFreeNode = (Node*)((ULONG64)_pFreeNode & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

		while (NULL != pFreeNode)
		{
			pDeleteNode = pFreeNode;
			pFreeNode = pFreeNode->next;
			_MemmoryPool.Free(pDeleteNode);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Stack 내부에 저장된 data 개수 반환
	//
	// Parameters: 없음.
	// Return: (int) data 개수
	//////////////////////////////////////////////////////////////////////////

	void Push(T data)
	{
		Node* pNode = _MemmoryPool.Alloc();

		pNode->data = data;
		pNode->next = NULL;

		while (1)
		{
			Node* pTempNode = _pFreeNode;
			Node* pFreeNode = (Node*)((ULONG64)pTempNode & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

			pNode->next = pFreeNode;

			// ABA 문제를 해결하기 위하여 Top 주소 값 빈 칸에 Count 값을 계산
			Node* pNewTop = (Node*)((((ULONG64)pTempNode + dfLOCKFREE_MEMORYPOOL_MASKING_COUNT_ADD) & dfLOCKFREE_MEMORYPOOL_MASKING_COUNT) | (ULONG64)(pNode));

			if (pTempNode == (Node*)InterlockedCompareExchange((unsigned long long*) & _pFreeNode, (unsigned long long)pNewTop, (unsigned long long)pTempNode))
			{
				break;
			}
		}
	}

	int Pop(T& data)
	{
		while (1)
		{
			Node* pTempNode = _pFreeNode;
			Node* pFreeNode = (Node*)((ULONG64)pTempNode & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

			if (nullptr == pFreeNode)
			{
				return 0;
			}

			// ABA 문제를 해결하기 위하여 Top 주소 값 빈 칸에 Count 값을 계산 
			Node* next = (Node*)((((ULONG64)pTempNode + dfLOCKFREE_MEMORYPOOL_MASKING_COUNT_ADD) & dfLOCKFREE_MEMORYPOOL_MASKING_COUNT) | (ULONG64)(pFreeNode->next));

			if (pTempNode == (Node*)InterlockedCompareExchange((unsigned long long*) & _pFreeNode, (unsigned long long)next, (unsigned long long)pTempNode))
			{
				data = pFreeNode->data;
				_MemmoryPool.Free(pFreeNode);

				break;
			}
		}
		return 1;
	}
};

#endif
