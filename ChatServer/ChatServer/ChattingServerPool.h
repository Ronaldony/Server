#ifndef __CHATTING_SERVER_POOL_H__
#define __CHATTING_SERVER_POOL_H__

//#include <windows.h>
#include "ChattingServerDefine.h"
#include "CPacketSingleton.h"

//------------------------------------------------------------
// ���� ���� ����
// 
//------------------------------------------------------------
struct st_USER_INFO
{
public:
	friend class CMemoryPoolTLS<st_USER_INFO>;

	ULONG64	SessionID;			// ���� ID
	INT64	AccountNo;			// Account ��ȣ

	WCHAR ID[20];				// ���� ID
	WCHAR Nickname[20];			// ���� �г���

	WORD SectorX;				// ���� X
	WORD SectorY;				// ���� Y
	
	ULONG64	LastRecvMsgTime;	// ������ �޽��� ���� �ð�

	static st_USER_INFO* Alloc(void) { return _PoolTLS.Alloc(); };
	static void Free(st_USER_INFO* data) { _PoolTLS.Free(data); };
	static LONG64 GetCountOfPoolAlloc(void) { return _PoolTLS.GetAllocCount(); }
	static LONG64 GetCountOfPoolTotalAlloc(void) { return _PoolTLS.GetChunkAllocCount(); }

private:
	inline static CMemoryPoolTLS<st_USER_INFO> _PoolTLS;	// CPacket �޸�Ǯ TLS
};
#endif