#ifndef __MONITOR_NET_SERVER_H_
#define __MONITOR_NET_SERVER_H_

//#include <unordered_map>
//#include <unordered_set>
//#include <strsafe.h>
//#include <Pdh.h>
#include "CNetServer.h"
#include "CProcessorMonitoring.h"
#include "LockFreeQueue.h"
#include "CDBConnector.h"
#include "CDBJob.h"

#define dfNUMBER_MEGA		1000000
#define dfNUMBER_KILLO		1000

// 채팅 서버 - 싱글 스레드
class MonitoringNetServer : public CNetServer
{
public:
	enum en_TIME_CHECK
	{
		en_MAX_MONITOR_USER = 200,
		en_SLEEP_TIME_CHECK_PERIOD = 100,			// 타임 체크 슬립 타임
		en_USER_TIME_OUT_PERIOD = 1000,				// 유저 타임 아웃 처리 주기
		en_MONITOR_COLLECT_TIME_PERIOD = 600000		// DB 로그 주기(ms)
	};

	enum en_PROCESSOR_MONITOR_COLLECTOR
	{
		en_PRO_CPU_SHARE = 0,
		en_PRO_NON_PAGED_MEMORY,
		en_PRO_NET_RECV,
		en_PRO_NET_SEND,
		en_PRO_CPU_AVA_MEMORY,
		en_PRO_TOTAL
	};

	enum en_SERVER_MONITOR_COLLECTOR
	{
		en_SRV_CLT_CPU_SHARE = 0,
		en_SRV_CLT_MEMORY,
		en_SRV_CLT_SESSION_COUNT,
		en_SRV_CLT_PLAYER_COUNT,
		en_SRV_CLT_MSG_TPS,
		en_SRV_CLT_PACKET_POOL,
		en_SRV_CLT_TOTAL
	};

	struct st_PROCESSOR_MONITOR_COLLECTOR
	{
		LONG64 Count;
		LONG64 Total;
		LONG64 Max;
		LONG64 Min;
		int Type;
	};

	struct st_SERVER_MONITOR_COLLECTOR
	{
		LONG64 alignas(64) Count;
		LONG64 alignas(64) Total;
		LONG64 Max;
		LONG64 Min;
		int Type;
	};

	MonitoringNetServer();
	~MonitoringNetServer();

	void MonitoringOutput(void);

	void PacketProc_UpdateServer(CPacket*);	// 모니터링 정보 업데이트

private:
	/* CLanServer 함수 정의 */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 udlSessionID);
	void OnClientLeave(ULONG64 udlSessionID);

	void OnRecv(ULONG64 udlSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	static unsigned int __stdcall Proc_TimeCheck(void* pvParam);	// 타임 체크
	static unsigned int __stdcall DBWriterThread(void* pvParam);	// DB 쓰기

	/* 프로시저 */
	void PacketProc_LoginMonitorClient(ULONG64 udlSessionID, CPacket*);		// 모니터링 클라이언트 로그인

	/* 패킷 생성 */
	void mpLoginRes(CPacket* pSendPacket, BYTE Status);						// 로그인 응답 메시지 생성
	void mpUpdateData(CPacket* pSendPacket, BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp);			// 섹터 이동 메시지 생성

	void AddUser(ULONG64 udlSessionID);			// 유저 정보 추가
	bool FindUser(ULONG64 udlSessionID);		// 유저 정보 검색
	void DeleteUser(ULONG64 udlSessionID);		// 유저 정보 삭제

	void CollectMonitorInfo(void);				// 프로세서 모니터링 정보 수집

	void ServerControl(void);					// 모니터링 제어 타워

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// 유저 정보 관리 컨테이너
	// Key: SessionID, Val: st_USER_INFO*
	std::list<ULONG64>	_UserMap;
	SRWLOCK				_UserMapLock;
	
	// 로그 관련
	DWORD		_dwLogLevel;				// 로그 레벨
	bool		_bSaveLog;					// 로그 파일 저장 여부
	WCHAR		_LogFileName[_MAX_PATH];

	// 모니터링 관련
	HANDLE	_hTimeCheckThread;
	DWORD	_dwTimeCheckThreadsID;
	
	ULONG64	_udlMonitorTimeTick;
	ULONG64	_udlDBWriteTimeTick;

	CProcessorMonitoring	_ProcessorMonitor;

	// DB 관련
	HANDLE	_hDBWriteEvent;
	HANDLE	_hDBWritehThread;
	DWORD	_dwDBWriteThreadsID;
	

	CDBConnector			_DBWriter;
	CLockFreeQueue<IDBJob*>	_DBJobQ;

	st_PROCESSOR_MONITOR_COLLECTOR	_ProcessorMonitorCollector[en_PRO_TOTAL];
	st_SERVER_MONITOR_COLLECTOR		_ServerMonitorCollector[en_SRV_CLT_TOTAL];
};

#endif