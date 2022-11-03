#ifndef __LOGIN_SERVER_POOL_H__
#define __LOGIN_SERVER_POOL_H__

//#include <windows.h>
#include "MemoryPoolTLS.h"

//------------------------------------------------------------
// 유저 관리 정보
// 
//------------------------------------------------------------
struct st_USER_INFO
{
public:
	friend class CMemoryPoolTLS<st_USER_INFO>;

	LONG64	LastRecvMsgTime;	// 마지막 메시지 수신 시간

	ULONG64	SessionID;			// 세션 ID
	
	static st_USER_INFO* Alloc(void) { return _PoolTLS.Alloc(); };
	static void Free(st_USER_INFO* data) { _PoolTLS.Free(data); };
	static LONG64 GetCountOfPoolAlloc(void) { return _PoolTLS.GetAllocCount(); }
	static LONG64 GetCountOfPoolTotalAlloc(void) { return _PoolTLS.GetChunkAllocCount(); }

private:
	st_USER_INFO()
	{
		SessionID = 0;
	}
	inline static CMemoryPoolTLS<st_USER_INFO> _PoolTLS;	// CPacket 메모리풀 TLS
};


#endif