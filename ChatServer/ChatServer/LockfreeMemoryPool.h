/*---------------------------------------------------------------

	procademy MemoryPool.

	�޸� Ǯ Ŭ���� (������Ʈ Ǯ / ��������Ʈ)
	Ư�� ����Ÿ(����ü,Ŭ����,����)�� ������ �Ҵ� �� ��������.

	- ����.

	64 bit ���� ����. 32bit������ ���� �۵����� ����

	procademy::CLockFreeMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData ���

	MemPool.Free(pData);

				
----------------------------------------------------------------*/
#ifndef  __LOCKFREE_MEMORY_POOL_H__
#define  __LOCKFREE_MEMORY_POOL_H__
//#include <Windows.h>

#define dfLOCKFREE_MEMORYPOOL_MASKING_ADDR		0xFFFFFFFFFFFF
#define dfLOCKFREE_MEMORYPOOL_MASKING_COUNT		0xFFFF000000000000
#define dfLOCKFREE_MEMORYPOOL_MASKING_COUNT_ADD	0x0001000000000000

#define dfLOCKFREE_MEMORYPOOL_BENCHMARKING

template <class DATA>
class CLockFreeMemoryPool
{
public:
	enum en_POOL
	{
		eSYSTEM_PAGE_SIZE = 4096,
		eALLOC_PAGE_RESERVE_SIZE = 65536
	};

	// ������Ʈ ��
	struct alignas(64) st_BLOCK_NODE
	{
		LONG64 objectCode;						// ������Ʈ �˻� �ڵ�
		DATA data;							// ������Ʈ - ���������� ����ڿ��� �Ѱ�����ϴ� �ּ�
		LONG64 overflow;						// ��� �����÷ο� �˻� �뵵
		st_BLOCK_NODE*			next;		// ���� ��� ������
	};

	//////////////////////////////////////////////////////////////////////////
	// ������, �ı���.
	//
	// Parameters:	(int) �ʱ� �� ����, (bool)�� Alloc ȣ�� �� ������Ʈ ������ ȣ�� ����
	//				(bool) Alloc �� ������ / Free �� �ı��� ȣ�� ����
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CLockFreeMemoryPool(bool bPlacementNew = false);
	~CLockFreeMemoryPool();


	//////////////////////////////////////////////////////////////////////////
	// �� �ϳ��� �Ҵ�޴´�.  
	//
	// Parameters: ����.
	// Return: (DATA *) ����Ÿ �� ������.
	//////////////////////////////////////////////////////////////////////////
	DATA	*Alloc(void);

	//////////////////////////////////////////////////////////////////////////
	// ������̴� ���� �����Ѵ�.
	//
	// Parameters: (DATA *) �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free(DATA *pData);


	//////////////////////////////////////////////////////////////////////////
	// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
	//
	// Parameters: ����.
	// Return: (LONG64) �޸� Ǯ ���� ��ü ����
	//////////////////////////////////////////////////////////////////////////
#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	LONG64		GetCapacityCount(void) { return _ldUseSize; }
	LONG64		GetPoolSize(void) { return _TotalMemoryPool; }

	inline static LONG64		GetTotalCapacityCount(void) { return _TotalUseSize; }
	inline static LONG64		GetTotalPoolSize(void) { return _TotalPoolSize; }
#endif

	//////////////////////////////////////////////////////////////////////////
	// �ִ� �Ҵ� �� ������ ������Ų��.
	//
	// Parameters: ����
	// Return: ����
	//////////////////////////////////////////////////////////////////////////
	DATA*		Resize(void);


private:
	// ���� ������� ��ȯ�� (�̻��) ������Ʈ ���� ����.
	bool alignas(64)	_bPlacementNew;	// Alloc, Free �� �Ź� ������ �� �Ҹ��� ȣ�� ����
	int					_iAddrDistance;	// Node�� Node ���� �ּ� ���� ��(�Ÿ�)

	// TLS ���� ���
	inline static LONG64	_bIsTlsNumAlloc;
	inline static DWORD		_dwPoolTlsNum;
	inline static DWORD		_dwAllocAddrTlsNum;
	inline static DWORD		_dwAllocSizeTlsNum;
	inline static DWORD		_dwResizeAddrTlsNum;
	inline static DWORD		_dwSpareResizeAddrTlsNum;
	int						_PageCommitCount;
	LONG64					_MemCountInPage;

	st_BLOCK_NODE* alignas(64) _pFreeNode;		// ���� Top ������Ʈ ��

#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	LONG64 alignas(64)	_ldUseSize;			// ���� ������� �� ����
	LONG64 alignas(64)	_TotalMemoryPool;	// ��ü �� ����


	inline static LONG64 alignas(64)	_TotalUseSize;	// ��ü �� ����
	inline static LONG64 alignas(64)	_TotalPoolSize;	// ��ü �� ����
#endif
};


template<class DATA>
CLockFreeMemoryPool<DATA>::CLockFreeMemoryPool(bool bPlacementNew)
	:_bPlacementNew(bPlacementNew), _pFreeNode(NULL), _iAddrDistance(0)
{
	st_BLOCK_NODE block;
	_iAddrDistance = (int)(((LONG64)(&block.data) - (LONG64)&block) / 4);

	if (0 == InterlockedExchange64(&_bIsTlsNumAlloc, true))
	{
		// TLS ��ȣ �ʱ�ȭ
		_dwPoolTlsNum = TlsAlloc();
		_dwAllocAddrTlsNum = TlsAlloc();
		_dwAllocSizeTlsNum = TlsAlloc();
		_dwResizeAddrTlsNum = TlsAlloc();
		_dwSpareResizeAddrTlsNum = TlsAlloc();
	}
	
	// ������ �� ��� ����
	_MemCountInPage = eSYSTEM_PAGE_SIZE / sizeof(st_BLOCK_NODE);
	if (0 == _MemCountInPage)
	{
		// ��尡 ���� �������� ũ�⺸�� ū ����̴�.
		// ������ ��带 ������ ũ��� ������ �Ҵ��� �������� ������ ����
		_MemCountInPage = 1 + sizeof(st_BLOCK_NODE) / eSYSTEM_PAGE_SIZE;
		_PageCommitCount = (int)_MemCountInPage;
	}
	else
	{
		// �� ���� Ŀ���� ������ ����
		_PageCommitCount = 1;
	}

#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	_ldUseSize = 0;			
	_TotalMemoryPool = 0;	
#endif
}

template<class DATA>
CLockFreeMemoryPool<DATA>::~CLockFreeMemoryPool()
{
	st_BLOCK_NODE* pDeleteNode;
	st_BLOCK_NODE* pFreeNode = (st_BLOCK_NODE*)((ULONG64)_pFreeNode & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

	while (NULL != pFreeNode)
	{
		pDeleteNode = pFreeNode;
		pFreeNode = pFreeNode->next;
		free(pDeleteNode);
	}
}

template<class DATA>
DATA* CLockFreeMemoryPool<DATA>::Alloc(void)
{
	DATA* retAddr;

	while (1)
	{
		st_BLOCK_NODE* pTempNode = _pFreeNode;
		st_BLOCK_NODE* pFreeNode = (st_BLOCK_NODE*)((ULONG64)pTempNode & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);
		
		if (nullptr == pFreeNode)
		{
			retAddr = Resize();
			break;
		}

		// ABA ������ �ذ��ϱ� ���Ͽ� Top �ּ� �� �� ĭ�� Count ���� ��� 
		st_BLOCK_NODE* next = (st_BLOCK_NODE*)((((ULONG64)pTempNode + dfLOCKFREE_MEMORYPOOL_MASKING_COUNT_ADD) & dfLOCKFREE_MEMORYPOOL_MASKING_COUNT) | (ULONG64)(pFreeNode->next));

		if (pTempNode == (st_BLOCK_NODE*)InterlockedCompareExchange((unsigned long long*) & _pFreeNode, (unsigned long long)next, (unsigned long long)pTempNode))
		{
			retAddr = &pFreeNode->data;

#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
			if (_bPlacementNew == true)
				new (retAddr) DATA;
#endif
			break;
		}
	}
#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	InterlockedIncrement64(&_ldUseSize);
	InterlockedIncrement64(&_TotalUseSize);
#endif

	return retAddr;
}

template<class DATA>
bool CLockFreeMemoryPool<DATA>::Free(DATA* pData)
{
	st_BLOCK_NODE* pNode = (st_BLOCK_NODE*)((int*)pData - _iAddrDistance);

#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	// ������Ʈ �� �˻�
	if (pNode->objectCode != (LONG64)pNode)
		return false;

	if (pNode->overflow != (LONG64)pNode)
		return false;

	// �� ���� ����
	if (_bPlacementNew == true)
		pData->~DATA();
#endif

	while (1)
	{
		st_BLOCK_NODE* pTempNode = _pFreeNode;
		st_BLOCK_NODE* pFreeNode = (st_BLOCK_NODE*)((ULONG64)pTempNode & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

		pNode->next = pFreeNode;

		// ABA ������ �ذ��ϱ� ���Ͽ� Top �ּ� �� �� ĭ�� Count ���� ���
		st_BLOCK_NODE* pNewTop = (st_BLOCK_NODE*)((((ULONG64)pTempNode + dfLOCKFREE_MEMORYPOOL_MASKING_COUNT_ADD) & dfLOCKFREE_MEMORYPOOL_MASKING_COUNT) | (ULONG64)(pNode));

		if (pTempNode == (st_BLOCK_NODE*)InterlockedCompareExchange((unsigned long long*)&_pFreeNode, (unsigned long long)pNewTop, (unsigned long long)pTempNode))
		{
			break;
		}
	}
#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	InterlockedDecrement64(&_ldUseSize);
	InterlockedDecrement64(&_TotalUseSize);
#endif

	return true;
}

template<class DATA>
DATA* CLockFreeMemoryPool<DATA>::Resize(void)
{
	// ������Ʈ �� �Ҵ�
	LONG64 dlSpareResizeAddrCount = (LONG64)TlsGetValue(_dwSpareResizeAddrTlsNum);
	
	if (dlSpareResizeAddrCount > 0)
	{
		TlsSetValue(_dwSpareResizeAddrTlsNum, (void*)(dlSpareResizeAddrCount - 1));

		st_BLOCK_NODE* pNewBlock = (st_BLOCK_NODE*)TlsGetValue(_dwResizeAddrTlsNum);

		// �޸�Ǯ �߰� �Ҵ� �ּ� ������Ʈ
		LONG64 NextResizeBlock = (LONG64)(pNewBlock->next);
		TlsSetValue(_dwResizeAddrTlsNum, (void*)NextResizeBlock);

		return &pNewBlock->data;
	}

	LONG64 ldAllocSize = (LONG64)TlsGetValue(_dwAllocSizeTlsNum);
	int* pBaseAddr = (int*)TlsGetValue(_dwAllocAddrTlsNum);
	int* pBlockAddrBase = pBaseAddr;

	// ���� �޸� ������ �� Ŀ��
	if ((NULL == pBaseAddr)
		|| ((LONG64)eALLOC_PAGE_RESERVE_SIZE - ldAllocSize < (LONG64)(sizeof(st_BLOCK_NODE) * _MemCountInPage)))
	{
		// Condition 1: ���� �޸� ����
		// Condition 2: ������ 64KB ������ �Ҵ�ũ�⸸ŭ�� ������ ���� ���

		pBlockAddrBase = (int*)VirtualAlloc(NULL, eALLOC_PAGE_RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
		pBaseAddr = pBlockAddrBase;
		ldAllocSize = 0;
	}

	if ((NULL == pBlockAddrBase) || (ERROR_INVALID_ADDRESS == (LONG64)pBlockAddrBase))
	{
		// Condition 1: �޸� �Ҵ� ����
		// Condition 2: ��ȿ���� ���� ������ ���� ��û�� ���
		int* exit = NULL;
		*exit = 0;
		return NULL;
	}

	pBlockAddrBase = (int*)VirtualAlloc(pBaseAddr, sizeof(st_BLOCK_NODE) * _MemCountInPage, MEM_COMMIT, PAGE_READWRITE);

	// �Ҵ�� ������ ������ ���ϱ�
	pBaseAddr = pBaseAddr + (eSYSTEM_PAGE_SIZE / sizeof(int)) * _PageCommitCount;
	ldAllocSize += _PageCommitCount * eSYSTEM_PAGE_SIZE;

	// TLS ����
	TlsSetValue(_dwAllocAddrTlsNum, pBaseAddr);
	TlsSetValue(_dwAllocSizeTlsNum, (void*)ldAllocSize);
	TlsSetValue(_dwSpareResizeAddrTlsNum, (void*)(_MemCountInPage - 1));

	// ������Ʈ �� �Ҵ�
	st_BLOCK_NODE* pAllocBlock = (st_BLOCK_NODE*)pBlockAddrBase;
	st_BLOCK_NODE* pBlock = pAllocBlock + 1;

#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	//--------------------------------------
	// �޸� �ʱ�ȭ
	//--------------------------------------
	pAllocBlock->objectCode = (LONG64)pAllocBlock;
	pAllocBlock->next = nullptr;
	pAllocBlock->overflow = (LONG64)pAllocBlock;

	new (&pAllocBlock->data) DATA;

	if (_bPlacementNew == true)
	{
		pBlock->objectCode = (LONG64)pBlock;
		pBlock->next = nullptr;
		pBlock->overflow = (LONG64)pBlock;

		for (int cnt = 2; cnt < _MemCountInPage; cnt++)
		{
			st_BLOCK_NODE* pNewBlock;

			pNewBlock = (st_BLOCK_NODE*)pBlock + 1;
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

		for (int cnt = 2; cnt < _MemCountInPage; cnt++)
		{
			st_BLOCK_NODE* pNewBlock;

			pNewBlock = (st_BLOCK_NODE*)pBlock + 1;
			pNewBlock->objectCode = (LONG64)pNewBlock;
			pNewBlock->next = pBlock;
			pNewBlock->overflow = (LONG64)pNewBlock;

			pBlock = pNewBlock;

			new (&pBlock->data) DATA;
		}
	}
#else
	//--------------------------------------
	// �޸� �ʱ�ȭ
	//--------------------------------------

	// �Ҵ� �޸�
	pAllocBlock->objectCode = (LONG64)pAllocBlock;
	pAllocBlock->next = nullptr;
	pAllocBlock->overflow = (LONG64)pAllocBlock;

	new (&pAllocBlock->data) DATA;

	// �޸�Ǯ ����
	pBlock->objectCode = (LONG64)pBlock;
	pBlock->next = nullptr;
	pBlock->overflow = (LONG64)pBlock;

	new (&pBlock->data) DATA;

	for (int cnt = 2; cnt < _MemCountInPage; cnt++)
	{
		st_BLOCK_NODE* pNewBlock;

		pNewBlock = (st_BLOCK_NODE*)pBlock + 1;
		pNewBlock->objectCode = (LONG64)pNewBlock;
		pNewBlock->next = pBlock;
		//pNewBlock->next = nullptr;
		pNewBlock->overflow = (LONG64)pNewBlock;

		pBlock = pNewBlock;

		new (&pBlock->data) DATA;
	}
#endif
	
	// �޸�Ǯ �߰� �Ҵ� �ּ� ������Ʈ
	TlsSetValue(_dwResizeAddrTlsNum, pBlock);

#ifndef dfLOCKFREE_MEMORYPOOL_BENCHMARKING
	InterlockedIncrement64(&_TotalMemoryPool);
	InterlockedAdd64(&_TotalPoolSize, _MemCountInPage);
#endif

	return &pAllocBlock->data;
}


#endif