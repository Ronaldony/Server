/*---------------------------------------------------------------

	procademy MemoryPool.

	메모리 풀 클래스 (오브젝트 풀 / 프리리스트)
	특정 데이타(구조체,클래스,변수)를 일정량 할당 후 나눠쓴다.

	- 사용법.

	64 bit 기준 설계. 32bit에서는 정상 작동하지 않음

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData 사용

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

	// 오브젝트 블럭
	struct st_BLOCK_NODE
	{
		LONG64 objectCode;						// 오브젝트 검사 코드
		DATA data;							// 오브젝트 - 실제적으로 사용자에게 넘겨줘야하는 주소
		LONG64 overflow;						// 노드 오버플로우 검사 용도
		st_BLOCK_NODE* next;				// 다음 노드 포인터
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
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 초기 블럭 개수, (bool)매 Alloc 호출 시 오브젝트 생성자 호출 여부
		//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
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
		// 블럭 하나를 할당받는다.  
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
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
		// 사용중이던 블럭을 해제한다.
		//
		// Parameters: (DATA *) 블럭 포인터.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		//void	Free(DATA *pData);
		void Free(void* pNode)
		{

#ifndef dfMEMORYPOOLTLS_BENCHMARK
			// 블럭 스택 정리
			if (_bPlacementNew == true)
				((st_BLOCK_NODE*)pNode)->data.~DATA();
#endif

			((st_BLOCK_NODE*)pNode)->next = _pFreeNode;
			_iCapacity++;
			_pFreeNode = (st_BLOCK_NODE*)pNode;
		}


		//////////////////////////////////////////////////////////////////////////
		// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
		//
		// Parameters: 없음.
		// Return: (int) 메모리 풀 내부 전체 개수
		//////////////////////////////////////////////////////////////////////////
		int		GetCapacityCount(void) { return _iCapacity; }


		//////////////////////////////////////////////////////////////////////////
		// 최대 할당 블럭 개수를 증가시킨다.
		//
		// Parameters: 없음
		// Return: 없음
		//////////////////////////////////////////////////////////////////////////
		//void		Resize(void);

		//////////////////////////////////////////////////////////////////////////
		// 메모리 청크를 전달 받는다.
		//
		// Parameters: 없음.
		// Return: (int) 메모리 풀 내부 전체 개수
		//////////////////////////////////////////////////////////////////////////
		void GetBlockChunk(void* pBlockChunk, int iChunkSize)
		{
			_iCapacity += iChunkSize;
			_pFreeNode = (st_BLOCK_NODE*)pBlockChunk;
		}


	private:
		// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리.

		int				_iCapacity;		// 현재 확보된 블럭 개수
		bool			_bPlacementNew;	// Alloc, Free 시 매번 생성자 및 소멸자 호출 여부
		st_BLOCK_NODE* _pFreeNode;		// 스택 Top 오브젝트 블럭
	};

public:
	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Parameters:	(int) 초기 블럭 개수, (bool)매 Alloc 호출 시 오브젝트 생성자 호출 여부
	//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CMemoryPoolTLS(bool bPlacementNew = false);
	virtual	~CMemoryPoolTLS();

	//////////////////////////////////////////////////////////////////////////
	// 블럭 하나를 할당받는다.  
	//
	// Parameters: 없음.
	// Return: (DATA *) 데이타 블럭 포인터.
	//////////////////////////////////////////////////////////////////////////
	DATA* Alloc(void);

	//////////////////////////////////////////////////////////////////////////
	// 사용중이던 블럭을 해제한다.
	//
	// Parameters: (DATA *) 블럭 포인터.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free(DATA* pData);

	//////////////////////////////////////////////////////////////////////////
	// 메모리 청크를 재할당한다.
	//
	// Parameters: (DATA *) 블럭 포인터.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	void	Resize(st_POOL_INFO*);

	//////////////////////////////////////////////////////////////////////////
	// 현재 풀에서 외부로 할당된 노드 개수
	//
	// Parameters: 없음
	// Return: (LONG64)할당된 개수
	//////////////////////////////////////////////////////////////////////////
	LONG64	GetAllocCount(void) { return _llCountOfAlloc; }
	LONG64	GetChunkAllocCount(void) { return _llCountOfAllocChunk; }

private:
	// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리.
	CLockFreeMemoryPool<st_CHUNK_BLOCK>		_ChunkPool;	// 청크 풀

	st_CHUNK_BLOCK* alignas(64) _pFreeChunk;

	bool alignas(64)		_bPlacementNew;		// Alloc, Free 시 매번 생성자 및 소멸자 호출 여부
	inline static DWORD		_dwPoolTlsNum;
	inline static DWORD		_dwAllocAddrTlsNum;
	inline static DWORD		_dwAllocSizeTlsNum;
	int						_PageCommitCount;
	int						_iAddrDistance;		// 노드 블럭의 DATA 위치와 노드 블럭의 주소 차이
	
	int						_iChunkPoolAllocSize;

#ifdef dfMEMORYPOOLTLS_MONITOR
	// 모니터링 정보
	LONG64 alignas(64)	_llCountOfAlloc;
	LONG64				_llCountOfAllocChunk;
#endif
};

template<class DATA>
CMemoryPoolTLS<DATA>::CMemoryPoolTLS(bool bPlacementNew)
	:_bPlacementNew(bPlacementNew), _pFreeChunk(NULL)
{
	// 메모리 블럭에서 실제 데이터까지의 주소 거리
	st_BLOCK_NODE block;
	_iAddrDistance = (int)(((LONG64)(&block.data) - (LONG64)&block) / 4);
	_iAddrDistance = (int)(dfADDRESS_MAX_VALUE - (LONG64)((_iAddrDistance - 1)));

	// TLS 번호 초기화
	_dwPoolTlsNum = TlsAlloc();
	_dwAllocAddrTlsNum = TlsAlloc();
	_dwAllocSizeTlsNum = TlsAlloc();

	// 할당 밸런스
	_iChunkPoolAllocSize = (eSYSTEM_PAGE_SIZE * 4) / sizeof(st_BLOCK_NODE);

	// 위 예약되는 크기의 페이지 개수
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
	// 처리 보류
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
// 블럭 하나를 할당받는다.
//
// Parameters: 없음.
// Return: (DATA *) 데이타 블럭 포인터.
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
		// Pool 최초 생성
		pThreadPool = (st_POOL_INFO*)_aligned_malloc(sizeof(st_POOL_INFO), 64);
		new (pThreadPool) st_POOL_INFO;
		if (!TlsSetValue(_dwPoolTlsNum, pThreadPool))
			return NULL;

		// 스레드 풀 요소 초기화
		pThreadPool->ChunkCapacity = 0;
		pThreadPool->PoolChunk = _ChunkPool.Alloc();
	}

	DATA* retAddr;

	// Pool Chunk Capacity 검사 및 할당
	// 만약 0 이상이면 Chunk에서 메모리 할당
	if (pThreadPool->ChunkCapacity)
	{
		retAddr = &pThreadPool->PoolChunk->node->data;
		pThreadPool->ChunkCapacity--;
		pThreadPool->PoolChunk->node = pThreadPool->PoolChunk->node->next;

		return retAddr;
	}

	// Pool Capacity 검사 및 할당
	// 만약 0 이상이면 Pool에서 메모리 할당
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

		// ABA 문제를 해결하기 위하여 Top 주소 값 빈 칸에 Count 값을 계산 
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
// 사용중이던 블럭을 해제한다.
//
// Parameters: (DATA *) 블럭 포인터.
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
	// 오브젝트 블럭 검사
	if (pNode->objectCode != (int)pNode)
		return false;

	if (pNode->overflow != (int)pNode)
		return false;
#endif

	st_POOL_INFO* pThreadPool = (st_POOL_INFO*)TlsGetValue(_dwPoolTlsNum);

	if (0 == pThreadPool)
	{
		// Pool 최초 생성
		pThreadPool = (st_POOL_INFO*)_aligned_malloc(sizeof(st_POOL_INFO), 64);
		new (pThreadPool) st_POOL_INFO;
		if (!TlsSetValue(_dwPoolTlsNum, pThreadPool))
			return NULL;

		// 스레드 풀 요소 초기화
		pThreadPool->ChunkCapacity = 0;
		pThreadPool->PoolChunk = _ChunkPool.Alloc();
	}

	// Pool Capacity 검사 및 할당
	// 만약 0 이상이면 Pool에서 메모리 할당
	if (pThreadPool->Pool.GetCapacityCount() < ePOOL_CHUNK_SIZE)
	{
		pThreadPool->Pool.Free(pNode);

		return true;
	}

	// Pool Chunk Capacity 검사 및 할당
	// 만약 0 이상이면 Chunk에서 메모리 할당
	if (pThreadPool->ChunkCapacity < ePOOL_CHUNK_SIZE)
	{
		pThreadPool->ChunkCapacity++;
		pNode->next = pThreadPool->PoolChunk->node;
		pThreadPool->PoolChunk->node = pNode;

		return true;
	}

	// 청크 풀 EnQ
	st_CHUNK_BLOCK* pChunk = _ChunkPool.Alloc();
	pChunk->node = pThreadPool->PoolChunk->node;
	pChunk->nextChunk = NULL;

	while (1)
	{
		st_CHUNK_BLOCK* pTempChunk = _pFreeChunk;
		st_CHUNK_BLOCK* pFreeChunk = (st_CHUNK_BLOCK*)((ULONG64)pTempChunk & dfLOCKFREE_MEMORYPOOL_MASKING_ADDR);

		pChunk->nextChunk = pFreeChunk;

		// ABA 문제를 해결하기 위하여 Top 주소 값 빈 칸에 Count 값을 계산
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

	// 동적 메모리 리저브 및 커밋
	if ((NULL == pBaseAddr)	
		|| ((LONG64)eALLOC_PAGE_RESERVE_SIZE - ldAllocSize < (LONG64)(sizeof(st_BLOCK_NODE) * _iChunkPoolAllocSize)))
	{
		// Condition 1: 최초 메모리 예약
		// Condition 2: 예약한 64KB 공간이 할당크기만큼의 공간이 없는 경우

		pBlockAddrBase = (int*)VirtualAlloc(NULL, eALLOC_PAGE_RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
		pBaseAddr = pBlockAddrBase;
		ldAllocSize = 0;
	}

	if ((NULL == pBlockAddrBase) || (ERROR_INVALID_ADDRESS == (LONG64)pBlockAddrBase))
	{
		// 동적 메모리 할당 실패
		int* exit = NULL;
		*exit = 0;
		return;
	}

	pBlockAddrBase = (int*)VirtualAlloc(pBaseAddr, sizeof(st_BLOCK_NODE) * _iChunkPoolAllocSize, MEM_COMMIT, PAGE_READWRITE);

	// 할당된 페이지 사이즈 구하기
	pBaseAddr = pBaseAddr + (eSYSTEM_PAGE_SIZE / sizeof(int)) * _PageCommitCount;
	ldAllocSize += _PageCommitCount * eSYSTEM_PAGE_SIZE;

	// TLS 갱신
	TlsSetValue(_dwAllocAddrTlsNum, pBaseAddr);
	TlsSetValue(_dwAllocSizeTlsNum, (void*)ldAllocSize);

	// 오브젝트 블럭 할당
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

		// placement new로 생성자 호출
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

	// 해당 Pool에게 메모리 청크 전달
	pPool->Pool.GetBlockChunk(pBlock, _iChunkPoolAllocSize);
#ifdef dfMEMORYPOOLTLS_MONITOR
	InterlockedAdd64(&_llCountOfAllocChunk, _iChunkPoolAllocSize);
#endif
}

#endif
