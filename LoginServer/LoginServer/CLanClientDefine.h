#ifndef __CLANCLIENT_DEFINE_H__
#define __CLANCLIENT_DEFINE_H__

//#include <WinSock2.h>
//#include <windows.h>
#include "CLanProtocol.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "CPacketSingleton.h"

#define dfLANSEND_PAKCET_NUMBER	50
#define dfRELEASE_FLAG_MASKING	0x100000000

// 확장 OVERLAPPED 구조체
struct st_LAN_OVERLAPPED_EX
{
	OVERLAPPED	overlapped;
	bool		bIsSend;		// true: 송신 작업 OVERLAPPED, false: 수신 작업 OVERLAPPED
};

// 세션 구조체
struct st_LAN_SESSION
{
	LONG64 alignas(64)		ReleaseFlagAndIOCount;			// IO 작업에 대한 카운트(0인 경우 세션을 종료하기 위한 값)
	bool					IsDisconn;						// 세션 사용 여부
	bool					UseFlag;						// 세션 사용 여부
	SOCKET 					Sock;							// 세션 소켓
	sockaddr_in				SockAddr;						// 세션 IP, Port
	LONG64 alignas(64)		SendFlag;						// true: 송신 가능, false: 송신 불가능
	LONG64 					SendRqstNum;					// WSASend를 등록한 버퍼 개수
	CPacket*				SendPacket[dfLANSEND_PAKCET_NUMBER]; // 송신 패킷 저장
	CLockFreeQueue<CPacket*> SendQ;							// 송신 패킷 락프리큐
	CRingBuffer				RecvQ;							// 수신 패킷 링버퍼 
	st_LAN_OVERLAPPED_EX	SendOverlapped;					// 송신 OVERLAPPED 확장 구조체
	st_LAN_OVERLAPPED_EX	RecvOverlapped;					// 수신 OVERLAPPED 확장 구조체
};
#endif
