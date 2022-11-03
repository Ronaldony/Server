#ifndef __CLANCLIENT_H__
#define __CLANCLIENT_H__
#include "CLanClientDefine.h"

//#define dfSERVER_MODULE_BENCHMARK
//#define dfSERVER_MODULE_MONITOR_TEST

class CLanClient
{
public:
	CLanClient();
	~CLanClient();

	bool Start(int dwWorkerThradNum, DWORD dwActiveThreadNum, const WCHAR* pwchIP, DWORD dwPort, bool bNagleOpt = false);
	void Stop(void);			// ��Ȯ�� ������ �ϴ� �Լ����� �𸣰���. (�޽��� �ۼ��� ����?)

	bool Connect(void);
	bool Disconnect(void);
	void SendPacket(CPacket* pPacket);

	bool IsConnected(void) { return _MySession.UseFlag; }

protected:
	virtual void OnEnterJoinServer(void) = 0;
	virtual void OnLeaveServer(void) = 0;

	virtual void OnRecv(CPacket* pPacket) = 0;
	virtual void OnSend(int sendsize) = 0;

	virtual void OnError(int errorcode, const WCHAR* comment) = 0;


private:
	void Release(void);

	void SendPost(void);
	void RecvPost(void);

	static unsigned int __stdcall WorkerThread(void* pvServerObj);

private:
	st_LAN_SESSION		_MySession;

	WCHAR			_wchServerIP[16];	// ���� IP
	DWORD			_dwServerPort;		// ���� ��Ʈ

	bool			_bNagleOpt;			// ���̱� �˰��� ����. true: on, false: off

	DWORD			_dwWorkderThreadNum;// IOCP ��Ŀ ������ ����
	DWORD			_dwActiveThreadNum;	// IOCP ���� ���� ������ ����

	HANDLE			_hIOCP;
	HANDLE*			_phThreads;			// ������ �ڵ� �迭
	DWORD*			_pdwThreadsID;		// ������ ID
};
#endif