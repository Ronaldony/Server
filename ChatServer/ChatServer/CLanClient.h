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
	void Stop(void);			// 정확히 무엇을 하는 함수인지 모르겠음. (메시지 송수신 중지?)

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

	WCHAR			_wchServerIP[16];	// 서버 IP
	DWORD			_dwServerPort;		// 서버 포트

	bool			_bNagleOpt;			// 네이글 알고리즘 여부. true: on, false: off

	DWORD			_dwWorkderThreadNum;// IOCP 워커 스레드 개수
	DWORD			_dwActiveThreadNum;	// IOCP 동시 러닝 스레드 개수

	HANDLE			_hIOCP;
	HANDLE*			_phThreads;			// 스레드 핸들 배열
	DWORD*			_pdwThreadsID;		// 스레드 ID
};
#endif