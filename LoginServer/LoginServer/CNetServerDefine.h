#ifndef __CNETSERVER_DEFINE_H__
#define __CNETSERVER_DEFINE_H__

//#include <WinSock2.h>
//#include <windows.h>
#include "CNetProtocol.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "CPacketSingleton.h"

#pragma warning(disable:26495)

#define dfNETSEND_PAKCET_NUMBER		200			// �� ���� WSABUF ����� ���� �ִ� ����
#define dfRELEASE_FLAG_MASKING		0x100000000		// ���� Release ���� ����ŷ ��Ʈ
#define dfSESSION_INDEX_MASKING		0x7FFF			// ���� �迭�� Index ����ŷ ��Ʈ

// Ȯ�� OVERLAPPED ����ü
struct st_NET_OVERLAPPED_EX
{
	OVERLAPPED	overlapped;
	bool		bIsSend;		// true: �۽� �۾� OVERLAPPED, false: ���� �۾� OVERLAPPED
};

// ���� ����ü
struct st_NET_SESSION
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
	CPacket*				SendPacket[dfNETSEND_PAKCET_NUMBER]; // �۽� ��Ŷ ����
	CLockFreeQueue<CPacket*> SendQ;							// �۽� ��Ŷ ������ť
	CRingBuffer				RecvQ;							// ���� ��Ŷ ������ 
	st_NET_OVERLAPPED_EX		SendOverlapped;				// �۽� OVERLAPPED Ȯ�� ����ü
	st_NET_OVERLAPPED_EX		RecvOverlapped;				// ���� OVERLAPPED Ȯ�� ����ü
	bool					IsInit;							// ���� �ʱ�ȭ ����
};
#endif
