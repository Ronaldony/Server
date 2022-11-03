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
	void Stop(void);

	bool Disconnect(ULONG64 ullSessionID);
	void SendPacket(ULONG64 ullSessionID, CPacket* pPacket);

	// 모니터링  함수
	void GetMonitoringInfo(st_MonitoringInfo*);

protected:

	// 화이트 리스트 IP 기능
	virtual bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort) = 0;

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
	CLockFreeStack<int> _AvailableSessionStack;	// 사용가능한 세션 배열 Index 저장 공간

	st_NET_SESSION*		_pSessionArr;		// 세션 저장 배열
	DWORD				_dwMaxSession;		// 최대 연결 제한 세션 수

	WCHAR			_wchIP[16];			// 소켓 바인드 IP(네트워크 어댑터)
	DWORD			_dwPort;			// 소켓 포트

	bool			_bNagleOpt;			// 네이글 알고리즘 여부. true: on, false: off

	DWORD			_dwWorkderThreadNum;// IOCP 워커 스레드 개수
	DWORD			_dwActiveThreadNum;	// IOCP 동시 러닝 스레드 개수

	HANDLE			_hIOCP;
	HANDLE*			_phThreads;			// 스레드 핸들 배열
	DWORD*			_pdwThreadsID;		// 스레드 ID

	BYTE			_byPacketCode;		// 프로토콜 코드
	BYTE			_byPacketKey;		// 패킷 고정 키

	// 모니터링 정보
	st_MonitoringInfo		_stMonitoringOnGoing;	// 실시간 모니터링 수집 데이터
};
#endif