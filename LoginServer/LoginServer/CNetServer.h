#ifndef __CNETSERVER_H__
#define __CNETSERVER_H__
#include "CNetServerDefine.h"
#include "CLockFreeStack.h"

#pragma warning(disable:26495)

//#define dfSERVER_MODULE_BENCHMARK
//#define dfSERVER_MODULE_MONITOR_TEST

class CNetServer
{
public:
	struct st_MonitoringInfo
	{
		LONG64 alignas(64)	AcceptTPS;
		LONG64				AcceptTPSMax;
		LONG64				AcceptTPSMin;
		LONG64				AcceptTotal;

		LONG64 alignas(64)	DisconnectTPS;
		LONG64 				DisconnectTPSMax;
		LONG64				DisconnectTPSMin;
		LONG64				DisconnetTotal;

		LONG64 alignas(64)	NowSessionNum;
	};

public:
	CNetServer();
	~CNetServer();

	bool Start(int dwWorkerThradNum, DWORD dwActiveThreadNum, const WCHAR* pwchIP, DWORD dwPort, DWORD dwMaxSessionNum, 
		BYTE _byPacketCode, BYTE _byPacketKey, bool bNagleOpt = false);
	void Stop(void);			// ��Ȯ�� ������ �ϴ� �Լ����� �𸣰���. (�޽��� �ۼ��� ����?)

	bool Disconnect(ULONG64 ullSessionID);
	void SendPacket(ULONG64 ullSessionID, CPacket* pPacket);

	// ����͸�  �Լ�
	void GetMonitoringInfo(st_MonitoringInfo*);

protected:

	// ȭ��Ʈ ����Ʈ IP ���
	virtual bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort) = 0;

	// TODO: OnClientJoin �Լ��� Ŭ���̾�Ʈ ������ ������ �����ؾ� �� �� ����غ� ��
	virtual void OnClientJoin(ULONG64 ullSessionID) = 0;
	virtual void OnClientLeave(ULONG64 ullSessionID) = 0;
	
	virtual void OnRecv(ULONG64 ullSessionID, CPacket* pPacket) = 0;
	
	virtual void OnError(int errorcode, const WCHAR* comment) = 0;

private:
	void SendPost(st_NET_SESSION* pSession);
	void RecvPost(st_NET_SESSION* pSession);
	void Release(st_NET_SESSION* pSession);

	st_NET_SESSION* FindSession(ULONG64 ullSessionID);
	int FindAvailableSession(void);

	static unsigned int __stdcall AcceptThread(void* pvServerObj);
	static unsigned int __stdcall WorkerThread(void* pvServerObj);

private:
	CLockFreeStack<int> _AvailableSessionStack;	// ��밡���� ���� �迭 Index ���� ����

	st_NET_SESSION*		_pSessionArr;		// ���� ���� �迭
	DWORD			_dwMaxSession;		// �ִ� ���� ���� ���� ��

	WCHAR			_wchIP[16];			// ���� ���ε� IP(��Ʈ��ũ �����)
	DWORD			_dwPort;			// ���� ��Ʈ

	bool			_bNagleOpt;			// ���̱� �˰��� ����. true: on, false: off

	DWORD			_dwWorkderThreadNum;// IOCP ��Ŀ ������ ����
	DWORD			_dwActiveThreadNum;	// IOCP ���� ���� ������ ����

	HANDLE			_hIOCP;
	HANDLE*			_phThreads;			// ������ �ڵ� �迭
	DWORD*			_pdwThreadsID;		// ������ ID

	BYTE			_byPacketCode;		// �������� �ڵ�
	BYTE			_byPacketKey;		// ��Ŷ ���� Ű

	// ����͸� ����
	st_MonitoringInfo		_stMonitoringOnGoing;	// �ǽð� ����͸� ���� ������
	LONG64					_ldMonitorCount;		// ����͸� ���� Ƚ��
	DWORD					_dwMonitorTick;			// ����͸� �ð�
};
#endif
