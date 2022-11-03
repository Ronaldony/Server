#include "stdafx.h"
#include <iostream>
#include <time.h>
#include <conio.h>
#include <Pdh.h>
#include <cpp_redis/cpp_redis>
#include "LoginServer.h"
#include "CommonProtocol.h"
#include "TextParset_Unicode.h"
#include "SystemLog.h"
#include "CCrashDump.h"
#include "SystemLog.h"

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#pragma comment(lib, "Winmm.lib")

using namespace std;

LoginServer::LoginServer() :
	CNetServer()
{
	// 모니터링 데이터 초기화
	memset(&_OngoingMonitoring, 0, sizeof(_OngoingMonitoring));
	memset(&_ResultMonitoring, 0, sizeof(_ResultMonitoring));

	// 시스템 로그 디렉토리 지정
	CSystemLog::SystemLog_Directory(L"LOGIN_SERVER_LOG");

	// Config 파일에서 설정 값 불러오기
	CParserUnicode parser;

	if (false == parser.LoadFile(L"LoginServer.cnf"))
	{
		wprintf(L"CONFIG FILE LOAD FAILED\n");
		CCrashDump::Crash();
	}
	
	wprintf(L"------------------------------------------------------------------\n");
	wprintf(L"---------------------CHAT SERVER CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen 소켓 바인드 IP
	if (parser.GetString(L"BIND_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen 소켓 포트
	if (parser.GetValue(L"BIND_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP 워커 스레드 개수
	if (parser.GetValue(L"IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP 동시 러닝 스레드 개수
	if (parser.GetValue(L"IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	DWORD dwMaxSession;			// 최대 세션
	if (parser.GetValue(L"SESSION_MAX", (int*)&dwMaxSession))
		wprintf(L"-----SESSION_MAX: %d\n", dwMaxSession);

	if (parser.GetValue(L"USER_MAX", (int*)&_dwMaxLoginUserNum))
		wprintf(L"-----USER_MAX: %d\n", _dwMaxLoginUserNum);

	int iPacketCode;
	if (parser.GetValue(L"PACKET_CODE", (int*)&iPacketCode))
		wprintf(L"-----PACKET_CODE: %d\n", iPacketCode);

	int iPacketKey;
	if (parser.GetValue(L"PACKET_KEY", (int*)&iPacketKey))
		wprintf(L"-----PACKET_KEY: %d\n", iPacketKey);

	wchar_t wchLogLevel[50] = { 0, };
	if (parser.GetString(L"LOG_LEVEL", wchLogLevel))
		wprintf(L"-----LOG_LEVEL: %s\n", wchLogLevel);

	if (parser.GetValue(L"TIMEOUT_DISCONNECT", (int*)&_dwTimeoutValue))
		wprintf(L"-----TIMEOUT_DISCONNECT: %d\n", _dwTimeoutValue);

	if (parser.GetString(L"GAME_SERVER_IP", _wchGameSrvIP))
		wprintf(L"-----GAME_SERVER_IP: %s\n", _wchGameSrvIP);

	if (parser.GetValue(L"GAME_SERVER_PORT", (int*)&_dwGameSrvPort))
		wprintf(L"-----GAME_SERVER_PORT: %d\n", _dwGameSrvPort);

	if (parser.GetString(L"CHAT_SERVER_IP", _wchChatSrvIP))
		wprintf(L"-----CHAT_SERVER_IP: %s\n", _wchChatSrvIP);

	if (parser.GetValue(L"CHAT_SERVER_PORT", (int*)&_dwChatSrvPort))
		wprintf(L"-----CHAT_SERVER_PORT: %d\n", _dwChatSrvPort);


	//------------------------------------------
	// DB 관련
	//------------------------------------------
	wchar_t wchDBIP[50] = { 0, };
	if (parser.GetString(L"DB_IP", wchDBIP))
		wprintf(L"-----DB_IP: %s\n", wchDBIP);

	wchar_t wchDBAccount[50] = { 0, };
	if (parser.GetString(L"DB_ACCOUNT", wchDBAccount))
		wprintf(L"-----DB_ACCOUNT: %s\n", wchDBAccount);

	wchar_t wchDBPASS[50] = { 0, };
	if (parser.GetString(L"DB_PASS", wchDBPASS))
		wprintf(L"-----DB_PASS: %s\n", wchDBPASS);

	wchar_t wchDBTable[50] = { 0, };
	if (parser.GetString(L"DB_TABLE", wchDBTable))
		wprintf(L"-----DB_TABLE: %s\n", wchDBTable);

	int iDBPort;
	if (parser.GetValue(L"DB_PORT", (int*)&iDBPort))
		wprintf(L"-----DB_PORT: %d\n", iDBPort);

	wprintf(L"------------------------------------------------------------------\n");

	// DB 세션 생성
	_pDBConnector = new CDBConnectorTLS(wchDBIP, wchDBAccount, wchDBPASS, wchDBTable, iDBPort);

	// 타임아웃
	_hTimeoutThread = (HANDLE)_beginthreadex(NULL, 0, Proc_TimeCheck, (void*)this, 0, (unsigned int*)&_dwTimeoutThreadID);
	if (_hTimeoutThread == 0 || _hTimeoutThread == (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex error\n");
		CCrashDump::Crash();
	}

	// 로그 레벨 지정
	if (!wcscmp(wchLogLevel, L"DEBUG"))
		LOG_LEVEL(CSystemLog::LEVEL_DEBUG);
	else if (!wcscmp(wchLogLevel, L"WARNING"))
		LOG_LEVEL(CSystemLog::LEVEL_ERROR);
	else
		LOG_LEVEL(CSystemLog::LEVEL_SYSTEM);

	// 로그 파일 네임
	time_t timer = time(NULL);
	struct tm t;

	(void)localtime_s(&t, &timer);
	swprintf_s(_LogFileName, sizeof(_LogFileName), L"Chat Server Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// Account 맵 동기화 객체 초기화
	InitializeSRWLock(&_UserMapLock);
	InitializeSRWLock(&_AccountMapLock);

	// NetServer 가동
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, iPacketCode, iPacketKey, false);

	_RedisClient.connect();
}

LoginServer::~LoginServer()
{

}

////////////////////////////////////////////////////////////////////////
// 연결 허용 여부 검사. 이곳에서 화이트 리스트 설정
// 
// Parameter: (WCHAR*)연결된 IP, (DWORD)포트 번호
// return: 없음
////////////////////////////////////////////////////////////////////////
bool LoginServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
{
	// TODO: 화이트 리스트 IP 검사하여 연결 확인
	//for (int cnt = 0; cnt < _whiteListIPCount; cnt++)
	//	if (ip != _whiteListIP) return false;
	
	// TODO: 새로운 세션 접속 메시지 EnQ
	//_ProcedureMsgQ.Enqueue(새로운 세션 접속 메시지);

	return true;
}

////////////////////////////////////////////////////////////////////////
// 신규 유저 접속 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::OnClientJoin(ULONG64 uldSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// 유저 접속 종료 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID
// void: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::OnClientLeave(ULONG64 uldSessionID)
{
	//--------------------------------
	// 유저맵에서 유저 삭제
	//--------------------------------

	AcquireSRWLockExclusive(&_UserMapLock);

	st_USER_INFO* pUser = FindUser(uldSessionID);
	if (NULL == pUser)
	{
		ReleaseSRWLockExclusive(&_UserMapLock);
		LOG(L"OnClientLeave", CSystemLog::LEVEL_ERROR, L"Can't find User: %lld\n", uldSessionID);
		return;
	}
	
	DeleteUser(uldSessionID);

	ReleaseSRWLockExclusive(&_UserMapLock);

	InterlockedDecrement64(&_OngoingMonitoring.NowLoginUserNum);

	st_USER_INFO::Free(pUser);
}

////////////////////////////////////////////////////////////////////////
// 요청 패킷 수신 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 저장 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::OnRecv(ULONG64 uldSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// 메시지 파싱 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// 메시지 타입에 따른 프로시저 호출
	switch (wType)
	{
	case en_PACKET_CS_LOGIN_REQ_LOGIN:		// 채팅서버 로그인 요청
		PacketProc_LoginUser(uldSessionID, pPacket);
		break;
	}

	return;
}

////////////////////////////////////////////////////////////////////////
// 에러 발생 콜백 함수
// 
// Parameter: (int)에러 코드, (const WCHAR*)에러 코멘트
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: 로직 스레드에 어떤 형태로 전달할 지 고민해보기
	if (0 != errorcode)
		LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// 유저 타임아웃 검사 프로시저
// 
// Parameter: (void*)LoginServer 객체 주소
// return: 
////////////////////////////////////////////////////////////////////////
unsigned int __stdcall LoginServer::Proc_TimeCheck(void* pvParam)
{
	LoginServer* pServer = (LoginServer*)pvParam;

	pServer->_dwTimeOutTick = timeGetTime();

	while (1)
	{
		DWORD dwNowTick = timeGetTime();
		if (dwNowTick - pServer->_dwTimeOutTick > 1000)
		{

			// TPS 관련
			pServer->_dwTimeOutTick = dwNowTick;
			
			pServer->CheckUserTimeout();
			

		}

		Sleep(100);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
// 로그인 요청 패킷 처리
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::PacketProc_LoginUser(ULONG64 uldSessionID, CPacket* pPacket)
{
	//------------------------------------
	// 로그인 절차
	//------------------------------------
	INT64	AccountNo;
	char	SessionKey[65];		// 인증토큰

	*pPacket >> AccountNo;
	pPacket->GetData((char*)&SessionKey, sizeof(SessionKey) - 1);

	// TODO: 퍼블리셔로부터 회원 정보 조회 (실제 테스트는 회원 DB)
	MYSQL_ROW sql_result;

	//------------------------------------
	// account 테이블 조회
	//------------------------------------
	if (false == _pDBConnector->Query(L"SELECT userid, usernick FROM account WHERE accountno=%d", AccountNo))
	{
		LOG(L"PacketProc_LoginUser", CSystemLog::LEVEL_SYSTEM, L"DB Query Fail. Error number: %d, Msg: %s\n", _pDBConnector->GetLastError(), _pDBConnector->GetLastErrorMsg());
		return;
	}

	WCHAR	ID[20];				// null 포함
	WCHAR	Nickname[20];		// null 포함

	sql_result = _pDBConnector->FetchRow();
	if (false == UTF8ToUTF16(ID, sizeof(ID), sql_result[0], (int)strlen(sql_result[0])))
		return;

	if (false == UTF8ToUTF16(Nickname, sizeof(Nickname), sql_result[1], (int)strlen(sql_result[1])))
		return;

	_pDBConnector->FreeResult();


	//------------------------------------
	// sessionkey 테이블 조회
	//------------------------------------
	//CHAR chSessionKey[64];

	if (false == _pDBConnector->Query(L"SELECT sessionkey FROM sessionkey WHERE accountno=%d", AccountNo))
	{
		LOG(L"PacketProc_LoginUser", CSystemLog::LEVEL_SYSTEM, L"DB Query Fail. Error number: %d, Msg: %s\n", _pDBConnector->GetLastError(), _pDBConnector->GetLastErrorMsg());
		return;
	}

	// DB로부터 세션키 조회
	//sql_result = _pDBConnector->FetchRow();
	//memcpy_s(chSessionKey, sizeof(chSessionKey), sql_result[0], strlen(sql_result[0]));
	
	_pDBConnector->FreeResult();

	//------------------------------------
	// 세션키 대조
	//------------------------------------


	//------------------------------------
	// status 테이블 조회
	//------------------------------------
	BYTE byStatus;

	if (false == _pDBConnector->Query(L"SELECT status FROM status WHERE accountno=%d", AccountNo))
	{
		LOG(L"PacketProc_LoginUser", CSystemLog::LEVEL_SYSTEM, L"DB Query Fail. Error number: %d, Msg: %s\n", _pDBConnector->GetLastError(), _pDBConnector->GetLastErrorMsg());
		return;
	}

	sql_result = _pDBConnector->FetchRow();
	byStatus = atoi(sql_result[0]);
	
	_pDBConnector->FreeResult();

	//------------------------------------
	// 회원 상태 기록
	//------------------------------------
	if (false == _pDBConnector->Query_Save(L"UPDATE status SET status=1 WHERE accountno=%d", AccountNo))
	{
		LOG(L"PacketProc_LoginUser", CSystemLog::LEVEL_SYSTEM, L"DB Query Fail. Error number: %d, Msg: %s\n", _pDBConnector->GetLastError(), _pDBConnector->GetLastErrorMsg());
		return;
	}

	//------------------------------------
	// Redis에 인증 토큰 저장(토큰 서버에 Account, 인증 토큰 전송)
	//------------------------------------

	char chAccount[17] = {0, };
	// INT64 -> 문자로 변환
	_i64toa_s(AccountNo, chAccount, sizeof(chAccount), 16);
	// 문자열 형태로 Redis에 쓰기 위함
	SessionKey[64] = 0;				

	_RedisClient.set(chAccount, SessionKey);
	_RedisClient.expire(chAccount, 5);			// 5초 후 키 만료
	_RedisClient.sync_commit();

	//------------------------------------
	// UserMap 추가
	//------------------------------------
	st_USER_INFO* pUser = st_USER_INFO::Alloc();

	pUser->LastRecvMsgTime = GetTickCount64();
	pUser->SessionID = uldSessionID;

	AcquireSRWLockExclusive(&_UserMapLock);
	AddUser(uldSessionID, pUser);
	ReleaseSRWLockExclusive(&_UserMapLock);

	//------------------------------------
	// 메시지 전송
	//------------------------------------
	CPacket* pSendPacket = CPacket::Alloc();
	mpLoginRes(pSendPacket, AccountNo, 1, ID, Nickname, _wchGameSrvIP, (USHORT)_dwGameSrvPort, _wchChatSrvIP, (USHORT)_dwChatSrvPort);
	SendPacket(uldSessionID, pSendPacket);
	CPacket::Free(pSendPacket);

	InterlockedIncrement64(&_OngoingMonitoring.NowLoginUserNum);
	InterlockedIncrement64(&_OngoingMonitoring.AuthCountTPS);
}


////////////////////////////////////////////////////////////////////////
// 로그인 응답 메시지 생성
// 
// Parameter: (CPacket*)전송 데이터 저장 직렬화버퍼, (INT64)전송 대상 Account 번호, (BYTE)인증 성공 여부
// void: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status, WCHAR* ID, WCHAR* NickName, WCHAR* GameServerIP, USHORT GameServerPort, WCHAR* ChatServerIP, USHORT ChatServerPort)
{
	//------------------------------------------------------------
	// 로그인 서버에서 클라이언트로 로그인 응답
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		BYTE	Status				// 0 (세션오류) / 1 (성공) ...  하단 defines 사용
	//
	//		WCHAR	ID[20]				// 사용자 ID		. null 포함
	//		WCHAR	Nickname[20]		// 사용자 닉네임	. null 포함
	//
	//		WCHAR	GameServerIP[16]	// 접속대상 게임,채팅 서버 정보
	//		USHORT	GameServerPort
	//		WCHAR	ChatServerIP[16]
	//		USHORT	ChatServerPort
	//	}
	//
	//------------------------------------------------------------
	WORD wType = en_PACKET_CS_LOGIN_RES_LOGIN;

	*pSendPacket << wType;
	*pSendPacket << AccountNo;
	*pSendPacket << Status;
	pSendPacket->PutData((char*)ID, 20 * sizeof(WCHAR));
	pSendPacket->PutData((char*)NickName, 20 * sizeof(WCHAR));
	pSendPacket->PutData((char*)GameServerIP, 16 * sizeof(WCHAR));
	*pSendPacket << GameServerPort;
	pSendPacket->PutData((char*)ChatServerIP, 16 * sizeof(WCHAR));
	*pSendPacket << ChatServerPort;
}


////////////////////////////////////////////////////////////////////////
// 유저 인증
// 
// Parameter: (ULONG64)유저 세션 ID, (INT64)유저 Account 번호, (char*)세션 키
// return: (bool)인증 성공 여부
////////////////////////////////////////////////////////////////////////
bool LoginServer::AuthenticateUser(ULONG64 uldSessionID, INT64 AccountNo, char* SessionKey)
{
	// TODO: 유저 인증 절차
	return true;
}

////////////////////////////////////////////////////////////////////////
// UTF8 인코딩 문자를 UTF16 인코딩 문자로 변환
// 
// Parameter: (WCHAR*)출력버퍼, (int)출력 버퍼 크기, (char*)입력버퍼, (int)입력 버퍼 크기
// return: (true)성공, (false)실패
////////////////////////////////////////////////////////////////////////
bool LoginServer::UTF8ToUTF16(WCHAR* dstStr, int dstSize, char* srcStr, int srcSize)
{
	int iLen = MultiByteToWideChar(CP_ACP, 0, srcStr, srcSize, NULL, NULL);
	if (iLen > dstSize)
	{
		LOG(L"UTF8ToUTF16", CSystemLog::LEVEL_DEBUG, L"Required size is larger than Output Buffer size\n");
		return false;
	}

	MultiByteToWideChar(CP_ACP, 0, srcStr, srcSize, dstStr, iLen);

	return true;
}


////////////////////////////////////////////////////////////////////////
// 유저 타임 아웃 검사
// 
// Parameter: 없음
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::CheckUserTimeout(void)
{
	ULONG64 uldSessionIDArr[en_CHECK_TIMEOUT];
	int iSessionCount = 0;

	//-----------------------------------------------
	// 유저맵을 순회하며 마지막 메시지 수신 시간 검사
	//-----------------------------------------------

	AcquireSRWLockShared(&_UserMapLock);

	LONG64 dwNowTick = GetTickCount64();

	std::unordered_map<ULONG64, st_USER_INFO*>::iterator iter;
	for (iter = _UserMap.begin(); iter != _UserMap.end(); ++iter)
	{
		if (abs((long long)(dwNowTick - iter->second->LastRecvMsgTime)) < _dwTimeoutValue)
			continue;

		uldSessionIDArr[iSessionCount++] = iter->first;
		if (iSessionCount >= en_CHECK_TIMEOUT)
			break;
	}

	ReleaseSRWLockShared(&_UserMapLock);

	for (int cnt = 0; cnt < iSessionCount; cnt++)
	{
		LOG(L"CheckUserTimeout", CSystemLog::LEVEL_ERROR, L"Session ID: %lld / Count: %d\n", uldSessionIDArr[cnt], iSessionCount);
		Disconnect(uldSessionIDArr[cnt]);
	}
}


////////////////////////////////////////////////////////////////////////
// 유저 맵에 유저 추가
// 
// Parameter: (ULONG64)유저 세션 ID, (st_USER_INFO*)유저 정보 포인터
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::AddUser(ULONG64 uldSessionID, st_USER_INFO* pUser)
{
	_UserMap.insert(std::make_pair(uldSessionID, pUser));
}

////////////////////////////////////////////////////////////////////////
// 유저 맵에서 유저 찾기
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 유저 정보 구조체 주소
////////////////////////////////////////////////////////////////////////
st_USER_INFO* LoginServer::FindUser(ULONG64 uldSessionID)
{
	std::unordered_map<ULONG64, st_USER_INFO*>::iterator iter = _UserMap.find(uldSessionID);
	if (iter == _UserMap.end())
	{
		LOG(L"FindUser", CSystemLog::LEVEL_DEBUG, L"User Map End. Session ID: %lld\n", uldSessionID);
		return NULL;
	}

	return iter->second;
}

////////////////////////////////////////////////////////////////////////
// 유저 맵에서 유저 삭제
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::DeleteUser(ULONG64 uldSessionID)
{
	_UserMap.erase(uldSessionID);
}

////////////////////////////////////////////////////////////////////////
// Account 맵에 Account 번호 추가
// 
// Parameter: (ULONG64)Account 번호, (st_USER_INFO*)유저 객체 주소
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::AddAccount(ULONG64 uldAccount, ULONG64 uldSessionID)
{
	_AccountMap.insert(std::make_pair(uldAccount, uldSessionID));
}

////////////////////////////////////////////////////////////////////////
// Account 맵에서 Account 번호 검색
// 
// Parameter: (ULONG64)Account 번호
// return: Account 번호의 유무. (0 < return)있음, (0)없음
////////////////////////////////////////////////////////////////////////
ULONG64 LoginServer::FindAccount(ULONG64 uldAccount)
{
	std::unordered_multimap<ULONG64, ULONG64>::iterator iter = _AccountMap.find(uldAccount);
	if (iter != _AccountMap.end())
		return iter->second;

	return 0;
}

////////////////////////////////////////////////////////////////////////
// Account 맵에서 Account 번호 삭제
// 
// Parameter: (ULONG64)Account 번호
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::DeleteAccount(ULONG64 uldAccount, ULONG64 uldSessionID)
{
	typedef std::unordered_multimap<ULONG64, ULONG64> MyMap;

	std::pair<MyMap::iterator, MyMap::iterator> pairData;
	for (pairData = _AccountMap.equal_range(uldAccount); pairData.first != pairData.second; pairData.first++)
	{
		if (uldSessionID != pairData.first->second)
			continue;

		_AccountMap.erase(pairData.first);
		break;
	}
}

////////////////////////////////////////////////////////////////////////
// 채팅 서버 모니터링
// 
// Parameter: 없음
// void: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::MonitoringOutput(void)
{
	st_MonitoringInfo mi;
	time_t timer = time(NULL);
	struct tm t;

	(void)localtime_s(&t, &timer);

	GetMonitoringInfo(&mi);

	WCHAR* wchMonitorBuffer = new WCHAR[10240];
	int iBufSize = 10240;

	GetMonitoringInfo(&mi);

	// 모니터 정보
	_ResultMonitoring.AuthCountTPS = InterlockedExchange64(&_OngoingMonitoring.AuthCountTPS, 0);

	// 모니터링 서버로 전송
	_ProcessMonitor.UpdateMonitorInfo();

	int iTime = (int)time(NULL);

	// 로그인서버 CPU 사용률
	CPacket* pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU, (int)(_ProcessMonitor.ProcessTotal()), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// 로그인서버 메모리 사용 MByte
	pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM, (int)(_ProcessMonitor.ProcessUserAllocMemory() / dfBYTES_MEGA), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// 로그인서버 세션 수 (컨넥션 수)
	pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_SESSION, (int)(mi.NowSessionNum), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// 로그인서버 인증 처리 초당 횟수
	pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS, (int)(_ResultMonitoring.AuthCountTPS), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// 로그인서버 패킷풀 사용량
	pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL, (int)(CPacket::GetCountOfPoolAlloc()), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	swprintf_s(wchMonitorBuffer, iBufSize, L"%s",L"==========================LOGIN SERVER==========================\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L" Press Key u or U to diplay Control Information\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s [%d/%d/%d %2d:%2d:%2d]\n\n", wchMonitorBuffer, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

	//---------------------------------------------------------------
	// Count
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"COUNT\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Session                     - %4lld\n", wchMonitorBuffer, mi.NowSessionNum);

	//---------------------------------------------------------------
	// TPS
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"TPS\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Auth Pass                   - %4lld\n", wchMonitorBuffer, _ResultMonitoring.AuthCountTPS);

	//---------------------------------------------------------------
	// Pool
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"POOL:: USE / TOTAL\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Packet Pool                 - %4lld / %4lld\n", wchMonitorBuffer, CPacket::GetCountOfPoolAlloc(), CPacket::GetCountOfPoolTotalAlloc());
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Player Pool                 - %4lld / %4lld\n", wchMonitorBuffer, st_USER_INFO::GetCountOfPoolAlloc(), st_USER_INFO::GetCountOfPoolTotalAlloc());
	
	wprintf(L"%s", wchMonitorBuffer);

	delete wchMonitorBuffer;

	ServerControl();

	if (true == _bSaveLog)
	{
		FILE* fp;
		if (_wfopen_s(&fp, _LogFileName, L"ab") != NULL)
		{
			DWORD err = GetLastError();
			int* exit = NULL;
			*exit = 1;
		}

		if (fp == NULL)
		{
			int* exit = NULL;
			*exit = 1;
		}

		// 프로파일링 프레임 파일 출력
		size_t size = fwrite(wchMonitorBuffer, 1, wcslen(wchMonitorBuffer) * sizeof(WCHAR), fp);
		if (size != wcslen(wchMonitorBuffer) * sizeof(WCHAR))
		{
			int* exit = NULL;
			*exit = 1;
		}

		fclose(fp);
	}
};

////////////////////////////////////////////////////////////////////////
// 서버 컨트롤 타워
// 
// Parameter: 없음
// return: 없음
////////////////////////////////////////////////////////////////////////
void LoginServer::ServerControl(void)
{
	// 키보드 컨트롤 잠금, 풀림 변수
	static bool bControlMode = false;

	//---------------------------------------------
	// L: 컨트롤 Lock  / U: 컨트롤 Unlock  / Q: 서버 종료 / O: 로그 레벨 변경
	//---------------------------------------------
	if (_kbhit())
	{
		WCHAR ControlKey = _getwch();

		// 키보드 제어 허용
		if ((L'u' == ControlKey) || (L'U' == ControlKey))
		{
			bControlMode = true;

			// 관련 키 도움말 출력
			wprintf(L"Control Mode..! Press Q - Quit \n");
			wprintf(L"Control Mode..! Press L - Control Lock \n");
			wprintf(L"Control Mode..! Press O - Change Log Level \n");
			wprintf(L"Control Mode..! Press S - Save / Not Save Log \n");
		}

		// 키보드 제어 잠금
		if ((L'l' == ControlKey) || (L'L' == ControlKey))
		{
			bControlMode = false;

			// 관련 키 도움말 출력
			wprintf(L"Control Mode..! Press U - Control Unlock \n");
		}

		// 서버 종료
		if ((L'q' == ControlKey && bControlMode) || (L'Q' == ControlKey))
		{
			//g_bShutdown = true;
		}

		// 로그 레벨 변경
		if ((L'o' == ControlKey || L'O' == ControlKey) && bControlMode)
		{
			_dwLogLevel = (_dwLogLevel + 1) % (dfSYSTEM_LOG_LEVEL_SYSTEM + 1);
			LOG_LEVEL(_dwLogLevel);

			WCHAR szLogLevel[3][10] =
			{
				L"DEBUG",
				L"WARNING",
				L"ERROR"
			};
			wprintf(L"Control Mode..! Now Log Level %s!\n", szLogLevel[_dwLogLevel]);
		}

		// 키보드제어 풀림 상태에서 특정 기능
		if ((L's' == ControlKey || L'S' == ControlKey) && bControlMode)
		{
			if (true == _bSaveLog)
			{
				wprintf(L"Change to Not Save Log \n");
				_bSaveLog = false;
			}
			else
			{
				wprintf(L"Change to Save Log \n");
				_bSaveLog = true;
			}
		}
	}
}