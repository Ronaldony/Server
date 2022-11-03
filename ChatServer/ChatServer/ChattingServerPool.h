#ifndef __CHATTING_SERVER_POOL_H__
#define __CHATTING_SERVER_POOL_H__

//#include <windows.h>
#include "ChattingServerDefine.h"
#include "CPacketSingleton.h"

//------------------------------------------------------------
// 유저 관리 정보
// 
//------------------------------------------------------------
struct st_USER_INFO
{
public:
	friend class CMemoryPoolTLS<st_USER_INFO>;

	ULONG64	SessionID;			// 세션 ID
	INT64	AccountNo;			// Account 번호

	WCHAR ID[20];				// 유저 ID
	WCHAR Nickname[20];			// 유저 닉네임

	WORD SectorX;				// 섹터 X
	WORD SectorY;				// 섹터 Y
	
	ULONG64	LastRecvMsgTime;	// 마지막 메시지 수신 시간

	static st_USER_INFO* Alloc(void) { return _PoolTLS.Alloc(); };
	static void Free(st_USER_INFO* data) { _PoolTLS.Free(data); };
	static LONG64 GetCountOfPoolAlloc(void) { return _PoolTLS.GetAllocCount(); }
	static LONG64 GetCountOfPoolTotalAlloc(void) { return _PoolTLS.GetChunkAllocCount(); }

private:
	inline static CMemoryPoolTLS<st_USER_INFO> _PoolTLS;	// CPacket 메모리풀 TLS
};
#endif