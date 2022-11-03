#ifndef __CLanServer_H__
#define __CLanServer_H__
#include "CLanServerDefine.h"
#include "CLockFreeStack.h"

#pragma warning(disable:26495)

#define dfSESSION_IOCOUNT_MASKING				0xFFFFFFFF
#define dfRELEASE_FLAG_MASKING					0x100000000

#define dfSESSION_INDEX_MASKING					0x7FFF

//#define dfSERVER_MODULE_BENCHMARK
//#define dfSERVER_MODULE_MONITOR_TEST

class CLanServer
{
public:
	struct st_MonitoringInfo
	{
		LONG64 alignas(64)	AcceptTPS;
		LONG64				AcceptTotal;
		LONG64 alignas(64)	NowSessionNum;
		LONG64 alignas(64)	DisconnectTPS;
		LONG64				DisconnetTotal;
		LONG64 alignas(64)	SendBPS;
		LONG64 alignas(64)	RecvBPS;
		LONG64 alignas(64)	SendTPS;
		LONG64 alignas(64)	RecvTPS;
	};

public:
	CLanServer();
	~CLanServer();

	bool Start(int dwWorkerThradNum, DWORD dwActiveThreadNum, const WCHAR* pwchIP, DWORD dwPort, DWORD dwMaxSessionNum, bool bNagleOpt = false);
	void Stop(void);			// 정확히 무엇을 하는 함수인지 모르겠음. (메시지 송수신 중지?)

	bool Disconnect(ULONG64 ullSessionID);
	void SendPacket(ULONG64 ullSessionID, CPacket* pPacket);

	// 모니터링  함수
	void GetMonitoringInfo(st_MonitoringInfo*);

protected:

	// 화이트 리스트 IP 기능
	virtual bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort) = 0;

	// TODO: OnClientJoin 함수에 클라이언트 정보로 무엇을 전달해야 할 지 고민해볼 것
	virtual void OnClientJoin(ULONG64 ullSessionID) = 0;
	virtual void OnClientLeave(ULONG64 ullSessionID) = 0;
	
	virtual void OnRecv(ULONG64 ullSessionID, CPacket* pPacket) = 0;
	
	virtual void OnError(int errorcode, const WCHAR* comment) = 0;

private:
	void SendPost(st_LAN_SESSION* pSession);
	void RecvPost(st_LAN_SESSION* pSession);
	void Release(st_LAN_SESSION* pSession);

	st_LAN_SESSION* FindSession(ULONG64 ullSessionID);
	int FindAvailableSession(void);

	static unsigned int __stdcall AcceptThread(void* pvServerObj);
	static unsigned int __stdcall WorkerThread(void* pvServerObj);

private:
	CLockFreeStack<int> _AvailableSessionStack;	// 사용가능한 세션 배열 Index 저장 공간

	st_LAN_SESSION*		_pSessionArr;		// 세션 저장 배열
	DWORD			_dwMaxSession;		// 최대 연결 제한 세션 수

	WCHAR			_wchIP[16];			// 소켓 바인드 IP(네트워크 어댑터)
	DWORD			_dwPort;			// 소켓 포트

	bool			_bNagleOpt;			// 네이글 알고리즘 여부. true: on, false: off

	DWORD			_dwWorkderThreadNum;// IOCP 워커 스레드 개수
	DWORD			_dwActiveThreadNum;	// IOCP 동시 러닝 스레드 개수

	HANDLE			_hIOCP;
	HANDLE*			_phThreads;			// 스레드 핸들 배열
	DWORD*			_pdwThreadsID;		// 스레드 ID

	// 모니터링 정보
	st_MonitoringInfo		_stMonitoringOnGoing;	// 실시간 모니터링 수집 데이터
	LONG64					_ldMonitorCount;		// 모니터링 수집 횟수
	DWORD					_dwMonitorTick;			// 모니터링 시간
};
#endif
