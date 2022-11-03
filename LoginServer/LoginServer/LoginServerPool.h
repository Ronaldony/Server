#ifndef __LOGIN_SERVER_POOL_H__
#define __LOGIN_SERVER_POOL_H__

//#include <windows.h>
#include "MemoryPoolTLS.h"

//------------------------------------------------------------
// ���� ���� ����
// 
//------------------------------------------------------------
struct st_USER_INFO
{
public:
	friend class CMemoryPoolTLS<st_USER_INFO>;

	LONG64	LastRecvMsgTime;	// ������ �޽��� ���� �ð�

	ULONG64	SessionID;			// ���� ID
	
	static st_USER_INFO* Alloc(void) { return _PoolTLS.Alloc(); };
	static void Free(st_USER_INFO* data) { _PoolTLS.Free(data); };
	static LONG64 GetCountOfPoolAlloc(void) { return _PoolTLS.GetAllocCount(); }
	static LONG64 GetCountOfPoolTotalAlloc(void) { return _PoolTLS.GetChunkAllocCount(); }

private:
	st_USER_INFO()
	{
		SessionID = 0;
	}
	inline static CMemoryPoolTLS<st_USER_INFO> _PoolTLS;	// CPacket �޸�Ǯ TLS
};


#endif