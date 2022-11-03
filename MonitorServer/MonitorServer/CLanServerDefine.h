#ifndef __CLANSERVER_DEFINE_H__
#define __CLANSERVER_DEFINE_H__

//#include <WinSock2.h>
//#include <windows.h>
#include "CLanProtocol.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "CPacketSingleton.h"

#define dfSEND_NET_PAKCET_SIZE	20

// Ȯ�� OVERLAPPED ����ü
struct st_LAN_OVERLAPPED_EX
{
	OVERLAPPED	overlapped;
	bool		bIsSend;		// true: �۽� �۾� OVERLAPPED, false: ���� �۾� OVERLAPPED
};

// ���� ����ü
struct st_LAN_SESSION
{
	LONG64 alignas(64)		ReleaseFlagAndIOCount;			// IO �۾��� ���� ī��Ʈ(0�� ��� ������ �����ϱ� ���� ��)
	bool					UseFlag;						// ���� ��� ����
	bool					IsDisconn;						// ���� ��� ����
	ULONG64					SessionID;						// ���� ID
	int						SessionIndex;					// ���� ���� Index
	SOCKET 					Sock;							// ���� ����
	sockaddr_in				SockAddr;						// ���� IP, Port
	LONG64 alignas(64)		SendFlag;						// true: �۽� ����, false: �۽� �Ұ���
	LONG64 					SendRqstNum;					// WSASend�� ����� ���� ����
	CPacket*				SendPacket[dfSEND_NET_PAKCET_SIZE]; // �۽� ��Ŷ ����
	CLockFreeQueue<CPacket*> SendQ;							// �۽� ��Ŷ ������ť
	CRingBuffer				RecvQ;							// ���� ��Ŷ ������ 
	st_LAN_OVERLAPPED_EX		SendOverlapped;					// �۽� OVERLAPPED Ȯ�� ����ü
	st_LAN_OVERLAPPED_EX		RecvOverlapped;					// ���� OVERLAPPED Ȯ�� ����ü
	bool					IsInit;							// ���� �ʱ�ȭ ����
};
#endif
