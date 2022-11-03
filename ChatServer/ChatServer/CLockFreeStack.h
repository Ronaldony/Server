/*---------------------------------------------------------------

	procademy MemoryPool.

	�޸� Ǯ Ŭ���� (������Ʈ Ǯ / ��������Ʈ)
	Ư�� ����Ÿ(����ü,Ŭ����,����)�� ������ �Ҵ� �� ��������.

	- ����.

	procademy::CLockFreeStack<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData ���

	MemPool.Free(pData);


----------------------------------------------------------------*/
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
	inline static CLockFreeMemoryPool<Node> _MemmoryPool;	// ���� Ÿ���� ������ ���õ��� �޸�Ǯ�� ����
	Node*			_pFreeNode;								// ���� Top ������Ʈ ��

public:

	CLockFreeStack()
	{
		_pFreeNode = NULL;
	}

	~CLockFreeStack()
	{
		// ó�� ����
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
	// Stack ���ο� ����� data ���� ��ȯ
	//
	// Parameters: ����.
	// Return: (int) data ����
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

			// ABA ������ �ذ��ϱ� ���Ͽ� Top �ּ� �� �� ĭ�� Count ���� ���
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

			// ABA ������ �ذ��ϱ� ���Ͽ� Top �ּ� �� �� ĭ�� Count ���� ��� 
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