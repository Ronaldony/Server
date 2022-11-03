#ifndef __LOGIN_SERVER_H__
#define __LOGIN_SERVER_H__

//#include <unordered_map>
//#include <Pdh.h>
#include "CNetServer.h"
#include "LoginServerPool.h"
#include "LockFreeQueue.h"
#include "CDBConnectorTLS.h"
#include "MonitorClient.h"
#include "CProcessMonitoring.h"

#define dfBYTES_MEGA	1000000
#define dfBYTES_KILLO	1000

// 채팅 서버 - 싱글 스레드
class LoginServer : public CNetServer 
{
public:
	enum en_USERCHECK
	{
		en_CHECK_TIMEOUT = 2000,
		en_CHECK_CHATUSER = 200
	};

	struct stMonitoringInfo
	{
		LONG64 alignas(64)	NowLoginUserNum;	// 현재 로그인 유저수
		LONG64 alignas(64)	AuthCountTPS;		// 인증성공 TPS
	};

	LoginServer();
	~LoginServer();

	// 모니터링 정보
	void MonitoringOutput(void);

private:
	/* CNetServer 함수 정의 */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 uldSessionID);
	void OnClientLeave(ULONG64 uldSessionID);

	void OnRecv(ULONG64 uldSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	static unsigned int __stdcall Proc_TimeCheck(void* pvParam);

	void PacketProc_LoginUser(ULONG64 uldSessionID, CPacket* pPacket);
	void mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status, WCHAR* ID, WCHAR* NickName,
		WCHAR* GameServerIP, USHORT GameServerPort, WCHAR* ChatServerIP, USHORT ChatServerPort);	// 로그인 응답 메시지 생성

	bool AuthenticateUser(ULONG64 uldSessionID, INT64 AccountNo, char* SessionKey);			// 유저 인증
	bool AddUserInfo(ULONG64 uldSessionID, INT64 AccountNo, WCHAR* ID, WCHAR* NickName);	// 유저 리스트에 새로운 유저 정보 추가

	bool UTF8ToUTF16(WCHAR* dstStr, int dstSize, char* srcStr, int srcSize);

	void CheckUserTimeout(void);		// 유저 타임아웃 검사
	
	void AddUser(ULONG64 uldSessionID, st_USER_INFO* pUser);	// 유저 정보 추가
	st_USER_INFO* FindUser(ULONG64 uldSessionID);				// 유저 정보 검색
	void DeleteUser(ULONG64 uldSessionID);						// 유저 정보 삭제

	void AddAccount(ULONG64 uldAccount, ULONG64 uldSessionID);		// Account 번호 추가
	ULONG64 FindAccount(ULONG64 uldAccount);						// Account 번호 검색
	void DeleteAccount(ULONG64 uldAccount, ULONG64 uldSessionID);	// Account 번호 삭제

	void ServerControl(void);	// 모니터링 제어 타워

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// 게임 서버 및 채팅 서버 IP, Port
	WCHAR		_wchGameSrvIP[16];
	DWORD		_dwGameSrvPort;
	WCHAR		_wchChatSrvIP[16];
	DWORD		_dwChatSrvPort;

	// 유저 정보 관리 컨테이너
	// Key: SessionID, Val: st_USER_INFO*
	std::unordered_map<ULONG64, st_USER_INFO*>	_UserMap;
	SRWLOCK							_UserMapLock;

	// Account 번호 관리 컨테이너
	// Key: Account 번호, Val: 세션 ID
	std::unordered_multimap<ULONG64, ULONG64>	_AccountMap;
	SRWLOCK			_AccountMapLock;

	DWORD	_dwTimeoutValue;	// 유저 타임아웃 시간
	DWORD	_dwTimeOutTick;		// 타임아웃 프로시저 Tick

	// 로그 관련
	DWORD		_dwLogLevel;		// 로그 레벨
	bool		_bSaveLog;			// 로그 파일 저장 여부
	WCHAR		_LogFileName[_MAX_PATH];

	HANDLE	_hTimeoutThread;		// 스레드 핸들 배열
	DWORD	_dwTimeoutThreadID;		// 스레드 ID

	DWORD	_dwMaxLoginUserNum;		// 최대 로그인 유저수

	stMonitoringInfo	_OngoingMonitoring;		// 계산 중인 모니터링 정보
	stMonitoringInfo	_ResultMonitoring;		// 결과 모니터링 정보

	CDBConnectorTLS*		_pDBConnector;		// DB 커넥터

	CProcessMonitoring		_ProcessMonitor;	// 프로세스 모니터 모듈
	MonitorClient			_MonitorClient;		// 모니터링 서버의 클라이언트

	cpp_redis::client		_RedisClient;		// Redis 클라이언트
};

#endif