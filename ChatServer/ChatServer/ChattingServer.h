#ifndef __CHATTING_SERVER_H__
#define __CHATTING_SERVER_H__

//#include <unordered_map>
//#include <unordered_set>
#include "CNetServer.h"
#include "ChattingServerDefine.h"
#include "ChattingServerPool.h"
#include "LockFreeQueue.h"
#include "MonitorClient.h"
#include "CProcessMonitoring.h"

// 채팅 서버 - 싱글 스레드
class ChattingServer : public CNetServer
{
public:
	enum en_USER
	{
		en_CHECK_TIMEOUT = 300,
		en_CHECK_CHATUSER = 200
	};

	enum en_TIME_CHECK
	{
		en_SLEEP_TIME_CHECK_PERIOD = 100,			// 타임 체크 슬립 타임
		en_USER_TIME_OUT_PERIOD = 1000,				// 유저 타임 아웃 처리 주기
		en_MONITOR_TIME_PERIOD = 1000,				// 모니터 정보 전송 주기
	};

	struct stMonitorInfo
	{
		alignas(64) LONG64 TPS_ProcMSG;
		LONG64 TPS_ProcMSGMonitor;
		LONG64 TPSMAX_ProcMSG;
		LONG64 TPSMIN_ProcMSG;

		LONG64 Total_ProcMSG;
		LONG64 MonitorCount_ProcMSG;
	};

	ChattingServer();
	~ChattingServer();

	// 모니터링 정보
	void MonitoringOutput(void);

private:
	/* CNetServer 함수 정의 */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 udlSessionID);
	void OnClientLeave(ULONG64 udlSessionID);

	void OnRecv(ULONG64 udlSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	/* 잡 스레드 */
	static unsigned int __stdcall Proc_TimeCheck(void* pvParam);	// 유저 타임아웃 프로시저

	/* 프로시저 */
	void PacketProc_LoginUser(ULONG64 udlSessionID, CPacket*);		// 유저 로그인 요청 처리
	void PacketProc_MoveSector(ULONG64 udlSessionID, CPacket*);		// 유저 섹터 이동 처리
	void PacketProc_Chatting(ULONG64 udlSessionID, CPacket*);		// 채팅 처리
	void PacketProc_HeartBeat(ULONG64 udlSessionID);				// 유저 타임 아웃 처리

	bool AuthenticateUser(ULONG64 udlSessionID, INT64 AccountNo, char* SessionKey);			// 유저 인증
	bool AddUserInfo(ULONG64 udlSessionID, INT64 AccountNo, WCHAR* ID, WCHAR* NickName);	// 유저 리스트에 새로운 유저 정보 추가
	void CheckUserTimeout(void);		// 유저 타임아웃 검사

	void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);		// 주변 섹터 구하기

	/* 패킷 생성 */
	void mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status);													// 로그인 응답 메시지 생성
	void mpMoveSectorRes(CPacket* pSendPacket, INT64 AccountNo, WORD SectorX, WORD SectorY);								// 섹터 이동 메시지 생성
	void mpChatting(CPacket* pSendPacket, INT64 AccountNo, WCHAR* ID, WCHAR* Nickname, WORD MessageLen, WCHAR* Message);	// 채팅 응답 메시지 생성

	void AddUser(ULONG64 udlSessionID, st_USER_INFO* pUser);	// 유저 정보 추가
	st_USER_INFO* FindUser(ULONG64 udlSessionID);				// 유저 정보 검색
	void DeleteUser(ULONG64 udlSessionID);						// 유저 정보 삭제

	void AddAccount(ULONG64 udlAccount, ULONG64 udlSessionID);		// Account 번호 추가
	ULONG64 FindAccount(ULONG64 udlAccount);						// Account 번호 검색
	void DeleteAccount(ULONG64 udlAccount, ULONG64 udlSessionID);	// Account 번호 삭제

	void AddSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID);		// 섹터 정보 추가
	bool FindSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID);	// 섹터 정보 검색
	void DeleteSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID);	// 섹터 정보 삭제

	void ServerControl(void);	// 모니터링 제어 타워

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// 유저 정보 관리 컨테이너
	// Key: SessionID, Val: st_USER_INFO*
	std::unordered_map<ULONG64, st_USER_INFO*>	_UserMap;
	SRWLOCK										_UserMapLock;

	// Account 번호 관리 컨테이너
	// Key: Account 번호, Val: 세션 ID
	std::unordered_multimap<ULONG64, ULONG64>	_AccountMap;

	// 섹터 정보 관리 컨테이너
	// Key: 섹터 정보(x, y). Value: 세션 ID
	std::unordered_set<ULONG64>					_SectorPosMap[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
	SRWLOCK										_SectorMapLock[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

	LONG64 alignas(64)	_dlNowLoginUserNum;		// 현재 로그인 유저수
	DWORD				_dwMaxLoginUserNum;		// 최대 로그인 유저수
	LONG64				_dlNowSessionNum;		// 현재 세션수

	DWORD		_dwTimeoutValue;			// 유저 타임아웃 처리 최대 시간

	HANDLE		_hTimeCheckThread;			// 타임 체크 스레드 핸들
	DWORD		_dwTimeCheckThreadsID;		// 타임 체크 스레드 ID

	DWORD		_dwLogLevel;				// 로그 레벨
	bool		_bSaveLog;					// 로그 파일 저장 여부
	WCHAR		_LogFileName[_MAX_PATH];	// 로그 저장 파일 이름
	
	ULONG64			_udlUserTimeTick;		// 유저 타임아웃 경과 시간
	ULONG64			_udlMonitorTick;		// 모니터 정보 경과 시간

	DWORD		_dwWorkerThreadNum;			// IO 워커 스레드 개수
	DWORD		_dwCoreNum;					// 코어 개수

	struct tm _StartTime;					// 서버 시작 시간

	stMonitorInfo	_MonitorData;			// TPS 모니터 정보

	ULONG64		_udlMaxSessionTime;
	ULONG64		_udlMaxSessionTimeMonitor;
	ULONG64		_udlMaxSessionTimeMonitorMax;
	ULONG64		_udlDisConnCount;
	ULONG64		_udlDisConnSession[0x1FFF + 1];

	// 모니터링 클라이언트 관련
	CProcessMonitoring		_ProcessMonitor;
	MonitorClient			_MonitorClient;

	cpp_redis::client		_RedisClient;
};

#endif