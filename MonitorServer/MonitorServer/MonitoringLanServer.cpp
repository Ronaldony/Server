#include <windows.h>
#include <strsafe.h>
#include <time.h>
#include <process.h>
#include <conio.h>
#include <unordered_set>
#include <unordered_map>
#include <crtdbg.h>
#include <DbgHelp.h>
#include <Pdh.h>
#include "MonitoringLanServer.h"
#include "TextParset_Unicode.h"
#include "SystemLog.h"
#include "CCrashDump.h"
#include "CommonProtocol.h"

#pragma comment(lib, "Winmm.lib")

using namespace std;

MonitoringLanServer::MonitoringLanServer() :
	CLanServer(), _NetMonitor(NULL)
{
	// Config 파일에서 설정 값 불러오기
	CParserUnicode parser;

	if (false == parser.LoadFile(L"MonitoringServer.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");
	
	wprintf(L"------------------------------------------------------------------------\n");
	wprintf(L"---------------------LAN MONITOR SERVER CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen 소켓 바인드 IP
	if (parser.GetString(L"LAN_BIND_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen 소켓 포트
	if (parser.GetValue(L"LAN_BIND_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP 워커 스레드 개수
	if (parser.GetValue(L"LAN_IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP 동시 러닝 스레드 개수
	if (parser.GetValue(L"LAN_IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	DWORD dwMaxSession;			// 최대 세션
	if (parser.GetValue(L"LAN_SESSION_MAX", (int*)&dwMaxSession))
		wprintf(L"-----SESSION_MAX: %d\n", dwMaxSession);

	wchar_t wchLogLevel[50] = { 0, };
	if (parser.GetString(L"LAN_LOG_LEVEL", wchLogLevel))
		wprintf(L"-----LOG_LEVEL: %s\n", wchLogLevel);

	wprintf(L"------------------------------------------------------------------\n");

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

	// Lan 서버 가동
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, false);
}

MonitoringLanServer::~MonitoringLanServer()
{

}

////////////////////////////////////////////////////////////////////////
// 연결 허용 여부 검사. 이곳에서 화이트 리스트 설정
// 
// Parameter: (WCHAR*)연결된 IP, (DWORD)포트 번호
// return: 없음
////////////////////////////////////////////////////////////////////////
bool MonitoringLanServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
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
void MonitoringLanServer::OnClientJoin(ULONG64 udlSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// 유저 접속 종료 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID
// void: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::OnClientLeave(ULONG64 udlSessionID)
{
	//--------------------------------
	// 유저맵에서 로그아웃 유저 삭제
	//--------------------------------
	AcquireSRWLockExclusive(&_UserMapLock);

	st_LAN_MONITOR_USER_INFO* pUser = FindUser(udlSessionID);
	if (NULL == pUser)
	{
		ReleaseSRWLockExclusive(&_UserMapLock);
		LOG(L"OnClientLeave", CSystemLog::LEVEL_ERROR, L"Can't find User: %lld\n", udlSessionID);
		return;
	}
	
	DeleteUser(pUser->SessionID);

	ReleaseSRWLockExclusive(&_UserMapLock);

	st_LAN_MONITOR_USER_INFO::Free(pUser);
}

////////////////////////////////////////////////////////////////////////
// 요청 패킷 수신 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 저장 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::OnRecv(ULONG64 udlSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// 메시지 파싱 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// 메시지 타입에 따른 프로시저 호출
	switch (wType)
	{
	case en_PACKET_TYPE::en_PACKET_SS_MONITOR_LOGIN:			// 컨텐츠 서버 로그인
		PacketProc_LoginServer(udlSessionID, pPacket);
		break;
	case en_PACKET_TYPE::en_PACKET_SS_MONITOR_DATA_UPDATE:		// 컨텐츠 서버로부터 모니터 정보 수신
	{
		if (_NetMonitor != NULL)
			_NetMonitor->PacketProc_UpdateServer(pPacket);
	}
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
void MonitoringLanServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: 로직 스레드에 어떤 형태로 전달할 지 고민해보기
	//if (0 != errorcode)
		//LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}


////////////////////////////////////////////////////////////////////////
// 로그인 요청 패킷 처리
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::PacketProc_LoginServer(ULONG64 udlSessionID, CPacket* pPacket)
{
	//------------------------------------
	// 컨텐츠 서버 로그인
	//------------------------------------
	int iServerNo;
	st_LAN_MONITOR_USER_INFO* pUser = st_LAN_MONITOR_USER_INFO::Alloc();

	*pPacket >> iServerNo;
	pUser->SessionID = udlSessionID;
	pUser->ServerNo = iServerNo;

	AcquireSRWLockExclusive(&_UserMapLock);
	AddUser(udlSessionID, pUser);
	ReleaseSRWLockExclusive(&_UserMapLock);
}

////////////////////////////////////////////////////////////////////////
// 유저 맵에 유저 추가
// 
// Parameter: (ULONG64)유저 세션 ID, (st_USER_INFO*)유저 정보 포인터
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::AddUser(ULONG64 udlSessionID, st_LAN_MONITOR_USER_INFO* pUser)
{
	_UserMap.insert(std::make_pair(udlSessionID, pUser));
}

////////////////////////////////////////////////////////////////////////
// 유저 맵에서 유저 찾기
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 유저 정보 구조체 주소
////////////////////////////////////////////////////////////////////////
st_LAN_MONITOR_USER_INFO* MonitoringLanServer::FindUser(ULONG64 udlSessionID)
{
	std::unordered_map<ULONG64, st_LAN_MONITOR_USER_INFO*>::iterator iter = _UserMap.find(udlSessionID);
	if (iter == _UserMap.end())
	{
		LOG(L"FindUser", CSystemLog::LEVEL_DEBUG, L"User Map End. Session ID: %lld\n", udlSessionID);
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
void MonitoringLanServer::DeleteUser(ULONG64 udlSessionID)
{
	_UserMap.erase(udlSessionID);
}