#include "stdafx.h"
#include <time.h>
#include <conio.h>
#include "MonitorClient.h"
#include "CommonProtocol.h"
#include "TextParset_Unicode.h"
#include "SystemLog.h"
#include "CCrashDump.h"

#pragma comment(lib, "Winmm.lib")

using namespace std;

MonitorClient::MonitorClient() :
	CLanClient()
{

	// Config 파일에서 설정 값 불러오기
	CParserUnicode parser;

	if (false == parser.LoadFile(L"MonitoringClient.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");

	wprintf(L"----------------------------------------------------------------------\n");
	wprintf(L"---------------------LAN LOGIN CLIENT CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen 소켓 바인드 IP
	if (parser.GetString(L"SERVER_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen 소켓 포트
	if (parser.GetValue(L"SERVER_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP 워커 스레드 개수
	if (parser.GetValue(L"IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP 동시 러닝 스레드 개수
	if (parser.GetValue(L"IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	wchar_t wchLogLevel[50] = { 0, };
	if (parser.GetString(L"LOG_LEVEL", wchLogLevel))
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
	swprintf_s(_LogFileName, sizeof(_LogFileName), L"MonitorClient Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// NetServer 가동
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, false);


	// 타임아웃 처리 스레드
	_hConnectThread = (HANDLE)_beginthreadex(NULL, 0, ConnectThread, (void*)this, 0, (unsigned int*)&_hConnectThreadID);
	if (_hConnectThread == 0 || _hConnectThread == (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex error\n");
		int* pDown = NULL;
		*pDown = 0;
	}
}

MonitorClient::~MonitorClient()
{

}

////////////////////////////////////////////////////////////////////////
// 신규 유저 접속 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnEnterJoinServer(void)
{
	// Nothing
	CPacket* pSendPacket = CPacket::Alloc();
	mpLoginReq(pSendPacket, 0);
	SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);
}

////////////////////////////////////////////////////////////////////////
// 유저 접속 종료 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID
// void: 없음
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnLeaveServer(void)
{
	while (!Connect())
	{
		Sleep(2000);
	}
}

////////////////////////////////////////////////////////////////////////
// 요청 패킷 수신 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 저장 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnRecv(CPacket* pPacket)
{

}

////////////////////////////////////////////////////////////////////////
// 송신 완료 콜백 함수
// 
// Parameter: (int)송신 완료된 데이터 크기
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnSend(int sendsize)
{

}

////////////////////////////////////////////////////////////////////////
// 에러 발생 콜백 함수
// 
// Parameter: (int)에러 코드, (const WCHAR*)에러 코멘트
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: 로직 스레드에 어떤 형태로 전달할 지 고민해보기
	if (0 != errorcode)
		LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// 모니터링 서버로 로그인 요청
// 
// Parameter: (CPacket*)패킷, (int)서버 번호
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitorClient::mpLoginReq(CPacket* pSendPacket, int ServerNo)
{
	WORD wType = en_PACKET_SS_MONITOR_LOGIN;

	*pSendPacket << wType;
	*pSendPacket << ServerNo;
}

////////////////////////////////////////////////////////////////////////
// 모니터링 서버로 로그인 요청
// 
// Parameter: (CPacket*)패킷, (BYTE)서버 번호, (int), (int)
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitorClient::mpUpdateMonitor(CPacket* pSendPacket, BYTE DataType, int DataValue, int TimeStamp)
{
	// 패킷 생성
	WORD wType = en_PACKET_SS_MONITOR_DATA_UPDATE;

	*pSendPacket << wType;
	*pSendPacket << DataType;
	*pSendPacket << DataValue;
	*pSendPacket << TimeStamp;
}



unsigned int __stdcall MonitorClient::ConnectThread(void* pvParam)
{
	MonitorClient* pMonitorClient = (MonitorClient*)pvParam;
	while (!pMonitorClient->Connect())
	{
		Sleep(2000);
	}

	return 0;
}