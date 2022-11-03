#ifndef __MONITOR_LAN_SERVER_POOL_H__
#define __MONITOR_LAN_SERVER_POOL_H__

//#include <windows.h>
#include "MemoryPoolTLS.h"

//------------------------------------------------------------
// ���� ���� ����
// 
//------------------------------------------------------------
struct st_LAN_MONITOR_USER_INFO
{
public:
	friend class CMemoryPoolTLS<st_LAN_MONITOR_USER_INFO>;

	ULONG64	SessionID;			// ���� ID
	int		ServerNo;

	static st_LAN_MONITOR_USER_INFO* Alloc(void) { return _PoolTLS.Alloc(); };
	static void Free(st_LAN_MONITOR_USER_INFO* data) { _PoolTLS.Free(data); };
	static LONG64 GetCountOfPoolAlloc(void) { return _PoolTLS.GetAllocCount(); }
	static LONG64 GetCountOfPoolTotalAlloc(void) { return _PoolTLS.GetChunkAllocCount(); }

private:
	inline static CMemoryPoolTLS<st_LAN_MONITOR_USER_INFO> _PoolTLS;	// CPacket �޸�Ǯ TLS
};

#endif