/*---------------------------------------------------------------

	procademy MemoryPool.

	�޸� Ǯ Ŭ���� (������Ʈ Ǯ / ��������Ʈ)
	Ư�� ����Ÿ(����ü,Ŭ����,����)�� ������ �Ҵ� �� ��������.

	- ����.

	64 bit ���� ����. 32bit������ ���� �۵����� ����

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData ���

	MemPool.Free(pData);


----------------------------------------------------------------*/
#ifndef  __MEMORY_POOL_TLS_H__
#define  __MEMORY_POOL_TLS_H__
//#include <windows.h>
//#include <new.h>
#include "LockfreeMemoryPool.h"

#pragma warning(disable:26495)

#define dfADDRESS_MAX_VALUE		0xFFFFFFFFFFFFFFFF

#define dfMEMORYPOOLTLS_BENCHMARK
#define dfMEMORYPOOLTLS_MONITOR

template <class DATA>
class CMemoryPoolTLS
{
public:
	template <class DATA>
	class CMemoryPool;

	enum ePOOLTLS
	{
		ePOOL_CHUNK_SIZE = 200,
		eSYSTEM_PAGE_SIZE = 4096,
		eALLOC_PAGE_RESERVE_SIZE = 65536
	};

	// ������Ʈ ��
	struct st_BLOCK_NODE
	{
		LONG64 objectCode;						// ������Ʈ �˻� �ڵ�
		DATA data;							// ������Ʈ - ���������� ����ڿ��� �Ѱ�����ϴ� �ּ�
		LONG64 overflow;						// ��� �����÷ο� �˻� �뵵
		st_BLOCK_NODE* next;				// ���� ��� ������
	};

	struct st_CHUNK_BLOCK
	{
		st_BLOCK_NODE* node;
		st_CHUNK_BLOCK* nextChunk;
	};

	struct st_POOL_INFO
	{
		int					ChunkCapacity;
		st_CHUNK_BLOCK*		PoolChunk;
		CMemoryPool<DATA>	Pool;
	};

private:
	template <class DATA>
	class CMemoryPool
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// ������, �ı���.
		//
		// Parameters:	(int) �ʱ� �� ����, (bool)�� Alloc ȣ�� �� ������Ʈ ������ ȣ�� ����
		//				(bool) Alloc �� ������ / Free �� �ı��� ȣ�� ����
		// Return:
		//////////////////////////////////////////////////////////////////////////
		CMemoryPool(bool bPlacementNew = false) : _iCapacity(0), _bPlacementNew(bPlacementNew), _pFreeNode(NULL){}

		~CMemoryPool() 
		{
			st_BLOCK_NODE* pDeleteNode;

			while (NULL != _pFreeNode)
			{
				pDeleteNode = _pFreeNode;
				_pFreeNode = _pFreeNode->next;
				free(pDeleteNode);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// �� �ϳ��� �Ҵ�޴´�.  
		//
		// Parameters: ����.
		// Return: (DATA *) ����Ÿ �� ������.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc(void) 
		{
			DATA* retAddr = (DATA*)&_pFreeNode->data;
			_iCapacity--;

#ifndef dfMEMORYPOOLTLS_BENCHMARK
			if (_bPlacementNew == true)
				new (retAddr) DATA;
#endif
			_pFreeNode = _pFreeNode->next;

			return retAddr;
		}

		//////////////////////////////////////////////////////////////////////////
		// ������̴� ���� �����Ѵ�.
		//
		// Parameters: (DATA *) �� ������.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		//void	Free(DATA *pData);
		void Free(void* pNode)
		{

#ifndef dfMEMORYPOOLTLS_BENCHMARK
			// �� ���� ����
			if (_bPlacementNew == true)
				((st_BLOCK_NODE*)pNode)->data.~DATA();
#endif

			((st_BLOCK_NODE*)pNode)->next = _pFreeNode;
			_iCapacity++;
			_pFreeNode = (st_BLOCK_NODE*)pNode;
		}


		//////////////////////////////////////////////////////////////////////////
		// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
		//
		// Parameters: ����.
		// Return: (int) �޸� Ǯ ���� ��ü ����
		//////////////////////////////////////////////////////////////////////////
		int		GetCapacityCount(void) { return _iCapacity; }


		//////////////////////////////////////////////////////////////////////////
		// �ִ� �Ҵ� �� ������ ������Ų��.
		//
		// Parameters: ����
		// Return: ����
		//////////////////////////////////////////////////////////////////////////
		//void		Resize(void);

		//////////////////////////////////////////////////////////////////////////
		// �޸� ûũ�� ���� �޴´�.
		//
		// Parameters: ����.
		// Return: (int) �޸� Ǯ ���� ��ü ����
		//////////////////////////////////////////////////////////////////////////
		void GetBlockChunk(void* pBlockChunk, int iChunkSize)
		{
			_iCapacity += iChunkSize;
			_pFreeNode = (st_BLOCK_NODE*)pBlockChunk;
		}


	private:
		// ���� ������� ��ȯ�� (�̻��) ������Ʈ ���� ����.

		int				_iCapacity;		// ���� Ȯ���� �� ����
		bool			_bPlacementNew;	// Alloc, Free �� �Ź� ������ �� �Ҹ��� ȣ�� ����
		st_BLOCK_NODE* _pFreeNode;		// ���� Top ������Ʈ ��
	};

public:
	//////////////////////////////////////////////////////////////////////////
	// ������, �ı���.
	//
	// Parameters:	(int) �ʱ� �� ����, (bool)�� Alloc ȣ�� �� ������Ʈ ������ ȣ�� ����
	//				(bool) Alloc �� ������ / Free �� �ı��� ȣ�� ����
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CMemoryPoolTLS(bool bPlacementNew = false);
	virtual	~CMemoryPoolTLS();

	//////////////////////////////////////////////////////////////////////////
	// �� �ϳ��� �Ҵ�޴´�.  
	//
	// Parameters: ����.
	// Return: (DATA *) ����Ÿ �� ������.
	//////////////////////////////////////////////////////////////////////////
	DATA* Alloc(void);

	//////////////////////////////////////////////////////////////////////////
	// ������̴� ���� �����Ѵ�.
	//
	// Parameters: (DATA *) �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free(DATA* pData);

	//////////////////////////////////////////////////////////////////////////
	// �޸� ûũ�� ���Ҵ��Ѵ�.
	//
	// Parameters: (DATA *) �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	void	Resize(st_POOL_INFO*);

	//////////////////////////////////////////////////////////////////////////
	// ���� Ǯ���� �ܺη� �Ҵ�� ��� ����
	//
	// Parameters: ����
	// Return: (LONG64)�Ҵ�� ����
	//////////////////////////////////////////////////////////////////////////
	LONG64	GetAllocCount(void) { return _llCountOfAlloc; }
	LONG64	GetChunkAllocCount(void) { return _llCountOfAllocChunk; }

private:
	// ���� ������� ��ȯ�� (�̻��) ������Ʈ ���� ����.
	CLockFreeMemoryPool<st_CHUNK_BLOCK>		_ChunkPool;	// ûũ Ǯ

	st_CHUNK_BLOCK* alignas(64) _pFreeChunk;

	bool alignas(64)		_bPlacementNew;		// Alloc, Free �� �Ź� ������ �� �Ҹ��� ȣ�� ����
	inline static DWORD		_dwPoolTlsNum;
	inline static DWORD		_dwAllocAddrTlsNum;
	inline static DWORD		_dwAllocSizeTlsNum;
	int						_PageCommitCount;
	int						_iAddrDistance;		// ��� ���� DATA ��ġ�� ��� ���� �ּ� ����
	
	int						_iChunkPoolAllocSize;

#ifdef dfMEMORYPOOLTLS_MONITOR
	// ����͸� ����
	LONG64 alignas(64)	_llCountOfAlloc;
	LONG64				_llCountOfAllocChunk;
#endif
};

template<class DATA>
CMemoryPoolTLS<DATA>::CMemoryPoolTLS(bool bPlacementNew)
	:_bPlacementNew(bPlacementNew), _pFreeChunk(NULL)
{
	// �޸� ������ ���� �����ͱ����� �ּ� �Ÿ�
	st_BLOCK_NODE block;
	_iAddrDistance = (int)(((LONG64)(&block.data) - (LONG64)&block) / 4);
	_iAddrDistance = (int)(dfADDRESS_MAX_VALUE - (LONG64)((_iAddrDistance - 1)));

	// TLS ��ȣ �ʱ�ȭ
	_dwPoolTlsNum = TlsAlloc();
	_dwAllocAddrTlsNum = TlsAlloc();
	_dwAllocSizeTlsNum = TlsAlloc();

	// �Ҵ� �뷱��
	_iChunkPoolAllocSize = (eSYSTEM_PAGE_SIZE * 4) / sizeof(st_BLOCK_NODE);

	// �� ����Ǵ� ũ���� ������ ����
	_PageCommitCount = (sizeof(st_BLOCK_NODE) * _iChunkPoolAllocSize) / eSYSTEM_PAGE_SIZE;
	if (sizeof(st_BLOCK_NODE) * _iChunkPoolAllocSize != eSYSTEM_PAGE_SIZE)
		_PageCommitCount++;

#ifdef dfMEMORYPOOLTLS_MONITOR
	_llCountOfAlloc = 0;
	_llCountOfAllocChunk = 0;
#endif

}

template<class DATA>
CMemoryPoolTLS<DATA>::~CMemoryPoolTLS()
{
	// ó�� ����
	//st_BLOCK_NODE* pDeleteNode;
	//st_BLOCK_NODE* pFreeChunk = (st_BLOCK_NODE*)((ULONG64)_pFreeChunk & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

	//while (NULL != pFreeChunk)
	//{
	//	pDeleteNode = pFreeChunk;
	//	pFreeChunk = pFreeChunk->next;
	//	free(pDeleteNode);
	//}
}

//////////////////////////////////////////////////////////////////////////
// �� �ϳ��� �Ҵ�޴´�.
//
// Parameters: ����.
// Return: (DATA *) ����Ÿ �� ������.
//////////////////////////////////////////////////////////////////////////
template<class DATA>
DATA* CMemoryPoolTLS<DATA>::Alloc(void)
{
#ifdef dfMEMORYPOOLTLS_MONITOR
	InterlockedIncrement64(&_llCountOfAlloc);
#endif

	st_POOL_INFO* pThreadPool = (st_POOL_INFO*)TlsGetValue(_dwPoolTlsNum);

	if (0 == pThreadPool)
	{
		// Pool ���� ����
		pThreadPool = (st_POOL_INFO*)_aligned_malloc(sizeof(st_POOL_INFO), 64);
		new (pThreadPool) st_POOL_INFO;
		if (!TlsSetValue(_dwPoolTlsNum, pThreadPool))
			return NULL;

		// ������ Ǯ ��� �ʱ�ȭ
		pThreadPool->ChunkCapacity = 0;
		pThreadPool->PoolChunk = _ChunkPool.Alloc();
	}

	DATA* retAddr;

	// Pool Chunk Capacity �˻� �� �Ҵ�
	// ���� 0 �̻��̸� Chunk���� �޸� �Ҵ�
	if (pThreadPool->ChunkCapacity)
	{
		retAddr = &pThreadPool->PoolChunk->node->data;
		pThreadPool->ChunkCapacity--;
		pThreadPool->PoolChunk->node = pThreadPool->PoolChunk->node->next;

		return retAddr;
	}

	// Pool Capacity �˻� �� �Ҵ�
	// ���� 0 �̻��̸� Pool���� �޸� �Ҵ�
	if (pThreadPool->Pool.GetCapacityCount())
	{
		//retAddr = pThreadPool->Pool.Alloc();

		return pThreadPool->Pool.Alloc();
	}


	while (1)
	{
		st_CHUNK_BLOCK* pTempChunk = _pFreeChunk;
		st_CHUNK_BLOCK* pFreeChunk = (st_CHUNK_BLOCK*)((ULONG64)pTempChunk & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

		if (nullptr == pFreeChunk)
		{
			Resize(pThreadPool);

			break;
		}

		// ABA ������ �ذ��ϱ� ���Ͽ� Top �ּ� �� �� ĭ�� Count ���� ��� 
		st_CHUNK_BLOCK* next = (st_CHUNK_BLOCK*)((((ULONG64)pTempChunk + dfLOCKFREE_MEMORYPOOL_MASKING_COUNT_ADD) & dfLOCKFREE_MEMORYPOOL_MASKING_COUNT) | (ULONG64)(pFreeChunk->nextChunk));

		if (pTempChunk == (st_CHUNK_BLOCK*)InterlockedCompareExchange((unsigned long long*) & _pFreeChunk, (unsigned long long)next, (unsigned long long)pTempChunk))
		{
			pThreadPool->Pool.GetBlockChunk(pFreeChunk->node, ePOOL_CHUNK_SIZE);
			_ChunkPool.Free(pFreeChunk);

			break;
		}
	}

	return pThreadPool->Pool.Alloc();
}

//////////////////////////////////////////////////////////////////////////
// ������̴� ���� �����Ѵ�.
//
// Parameters: (DATA *) �� ������.
// Return: (BOOL) TRUE, FALSE.
//////////////////////////////////////////////////////////////////////////
template<class DATA>
bool CMemoryPoolTLS<DATA>::Free(DATA* pData)
{
#ifdef dfMEMORYPOOLTLS_MONITOR
	InterlockedDecrement64(&_llCountOfAlloc);
#endif

	//st_BLOCK_NODE* pNode = (st_BLOCK_NODE*)((int*)pData - _iAddrDistance);
	st_BLOCK_NODE* pNode = (st_BLOCK_NODE*)((int*)pData + _iAddrDistance);

#ifndef dfMEMORYPOOLTLS_BENCHMARK
	// ������Ʈ �� �˻�
	if (pNode->objectCode != (int)pNode)
		return false;

	if (pNode->overflow != (int)pNode)
		return false;
#endif

	st_POOL_INFO* pThreadPool = (st_POOL_INFO*)TlsGetValue(_dwPoolTlsNum);

	if (0 == pThreadPool)
	{
		// Pool ���� ����
		pThreadPool = (st_POOL_INFO*)_aligned_malloc(sizeof(st_POOL_INFO), 64);
		new (pThreadPool) st_POOL_INFO;
		if (!TlsSetValue(_dwPoolTlsNum, pThreadPool))
			return NULL;

		// ������ Ǯ ��� �ʱ�ȭ
		pThreadPool->ChunkCapacity = 0;
		pThreadPool->PoolChunk = _ChunkPool.Alloc();
	}

	// Pool Capacity �˻� �� �Ҵ�
	// ���� 0 �̻��̸� Pool���� �޸� �Ҵ�
	if (pThreadPool->Pool.GetCapacityCount() < ePOOL_CHUNK_SIZE)
	{
		pThreadPool->Pool.Free(pNode);

		return true;
	}

	// Pool Chunk Capacity �˻� �� �Ҵ�
	// ���� 0 �̻��̸� Chunk���� �޸� �Ҵ�
	if (pThreadPool->ChunkCapacity < ePOOL_CHUNK_SIZE)
	{
		pThreadPool->ChunkCapacity++;
		pNode->next = pThreadPool->PoolChunk->node;
		pThreadPool->PoolChunk->node = pNode;

		return true;
	}

	// ûũ Ǯ EnQ
	st_CHUNK_BLOCK* pChunk = _ChunkPool.Alloc();
	pChunk->node = pThreadPool->PoolChunk->node;
	pChunk->nextChunk = NULL;

	while (1)
	{
		st_CHUNK_BLOCK* pTempChunk = _pFreeChunk;
		st_CHUNK_BLOCK* pFreeChunk = (st_CHUNK_BLOCK*)((ULONG64)pTempChunk & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

		pChunk->nextChunk = pFreeChunk;

		// ABA ������ �ذ��ϱ� ���Ͽ� Top �ּ� �� �� ĭ�� Count ���� ���
		st_CHUNK_BLOCK* pNewTop = (st_CHUNK_BLOCK*)((((ULONG64)pTempChunk + dfLOCKFREE_MEMORYPOOL_MASKING_COUNT_ADD) & dfLOCKFREE_MEMORYPOOL_MASKING_COUNT) | (ULONG64)(pChunk));

		if (pTempChunk == (st_CHUNK_BLOCK*)InterlockedCompareExchange((unsigned long long*) & _pFreeChunk, (unsigned long long)pNewTop, (unsigned long long)pTempChunk))
		{
			pThreadPool->ChunkCapacity = 1;
			pThreadPool->PoolChunk->node = pNode;
			pNode->next = NULL;

			break;
		}
	}

	return true;
}


template<class DATA>
void CMemoryPoolTLS<DATA>::Resize(st_POOL_INFO* pPool)
{
	LONG64 ldAllocSize = (LONG64)TlsGetValue(_dwAllocSizeTlsNum);
	int* pBaseAddr = (int*)TlsGetValue(_dwAllocAddrTlsNum);
	int* pBlockAddrBase = pBaseAddr;

	// ���� �޸� ������ �� Ŀ��
	if ((NULL == pBaseAddr)	
		|| ((LONG64)eALLOC_PAGE_RESERVE_SIZE - ldAllocSize < (LONG64)(sizeof(st_BLOCK_NODE) * _iChunkPoolAllocSize)))
	{
		// Condition 1: ���� �޸� ����
		// Condition 2: ������ 64KB ������ �Ҵ�ũ�⸸ŭ�� ������ ���� ���

		pBlockAddrBase = (int*)VirtualAlloc(NULL, eALLOC_PAGE_RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
		pBaseAddr = pBlockAddrBase;
		ldAllocSize = 0;
	}

	if ((NULL == pBlockAddrBase) || (ERROR_INVALID_ADDRESS == (LONG64)pBlockAddrBase))
	{
		// ���� �޸� �Ҵ� ����
		int* exit = NULL;
		*exit = 0;
		return;
	}

	pBlockAddrBase = (int*)VirtualAlloc(pBaseAddr, sizeof(st_BLOCK_NODE) * _iChunkPoolAllocSize, MEM_COMMIT, PAGE_READWRITE);

	// �Ҵ�� ������ ������ ���ϱ�
	pBaseAddr = pBaseAddr + (eSYSTEM_PAGE_SIZE / sizeof(int)) * _PageCommitCount;
	ldAllocSize += _PageCommitCount * eSYSTEM_PAGE_SIZE;

	// TLS ����
	TlsSetValue(_dwAllocAddrTlsNum, pBaseAddr);
	TlsSetValue(_dwAllocSizeTlsNum, (void*)ldAllocSize);

	// ������Ʈ �� �Ҵ�
	st_BLOCK_NODE* pBlock = (st_BLOCK_NODE*)pBlockAddrBase;
	st_BLOCK_NODE* pNewBlock;

	if (_bPlacementNew == true)
	{
		pBlock->objectCode = (LONG64)pBlock;
		pBlock->next = nullptr;
		pBlock->overflow = (LONG64)pBlock;

		for (int cnt = 1; cnt < _iChunkPoolAllocSize; cnt++)
		{
			pNewBlock = (st_BLOCK_NODE*)pBlockAddrBase + cnt;
			pNewBlock->objectCode = (LONG64)pNewBlock;
			pNewBlock->next = pBlock;
			pNewBlock->overflow = (LONG64)pNewBlock;

			pBlock = pNewBlock;
		}
	}
	else // if (bPlacementNew == false)
	{
		pBlock->objectCode = (LONG64)pBlock;
		pBlock->next = nullptr;
		pBlock->overflow = (LONG64)pBlock;

		// placement new�� ������ ȣ��
		new (&pBlock->data) DATA;

		for (int cnt = 1; cnt < _iChunkPoolAllocSize; cnt++)
		{
			pNewBlock = (st_BLOCK_NODE*)pBlockAddrBase + cnt;
			pNewBlock->objectCode = (LONG64)pNewBlock;
			pNewBlock->next = pBlock;
			pNewBlock->overflow = (LONG64)pNewBlock;

			pBlock = pNewBlock;

			new (&pBlock->data) DATA;
		}
	}

	// �ش� Pool���� �޸� ûũ ����
	pPool->Pool.GetBlockChunk(pBlock, _iChunkPoolAllocSize);
#ifdef dfMEMORYPOOLTLS_MONITOR
	InterlockedAdd64(&_llCountOfAllocChunk, _iChunkPoolAllocSize);
#endif
}

#endif
