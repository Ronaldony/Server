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

	// Config ���Ͽ��� ���� �� �ҷ�����
	CParserUnicode parser;

	if (false == parser.LoadFile(L"MonitoringClient.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");

	wprintf(L"----------------------------------------------------------------------\n");
	wprintf(L"---------------------LAN LOGIN CLIENT CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen ���� ���ε� IP
	if (parser.GetString(L"SERVER_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen ���� ��Ʈ
	if (parser.GetValue(L"SERVER_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP ��Ŀ ������ ����
	if (parser.GetValue(L"IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP ���� ���� ������ ����
	if (parser.GetValue(L"IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	wchar_t wchLogLevel[50] = { 0, };
	if (parser.GetString(L"LOG_LEVEL", wchLogLevel))
		wprintf(L"-----LOG_LEVEL: %s\n", wchLogLevel);

	wprintf(L"------------------------------------------------------------------\n");

	// �α� ���� ����
	if (!wcscmp(wchLogLevel, L"DEBUG"))
		LOG_LEVEL(CSystemLog::LEVEL_DEBUG);
	else if (!wcscmp(wchLogLevel, L"WARNING"))
		LOG_LEVEL(CSystemLog::LEVEL_ERROR);
	else
		LOG_LEVEL(CSystemLog::LEVEL_SYSTEM);

	// �α� ���� ����
	time_t timer = time(NULL);
	struct tm t;

	(void)localtime_s(&t, &timer);
	swprintf_s(_LogFileName, sizeof(_LogFileName), L"MonitorClient Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// NetServer ����
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, false);


	// Ÿ�Ӿƿ� ó�� ������
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
// �ű� ���� ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID
// return: ����
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
// ���� ���� ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnLeaveServer(void)
{
	while (!Connect())
	{
		Sleep(2000);
	}
}

////////////////////////////////////////////////////////////////////////
// ��û ��Ŷ ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ���� ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnRecv(CPacket* pPacket)
{

}

////////////////////////////////////////////////////////////////////////
// �۽� �Ϸ� �ݹ� �Լ�
// 
// Parameter: (int)�۽� �Ϸ�� ������ ũ��
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnSend(int sendsize)
{

}

////////////////////////////////////////////////////////////////////////
// ���� �߻� �ݹ� �Լ�
// 
// Parameter: (int)���� �ڵ�, (const WCHAR*)���� �ڸ�Ʈ
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitorClient::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: ���� �����忡 � ���·� ������ �� ����غ���
	if (0 != errorcode)
		LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// ����͸� ������ �α��� ��û
// 
// Parameter: (CPacket*)��Ŷ, (int)���� ��ȣ
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitorClient::mpLoginReq(CPacket* pSendPacket, int ServerNo)
{
	WORD wType = en_PACKET_SS_MONITOR_LOGIN;

	*pSendPacket << wType;
	*pSendPacket << ServerNo;
}

////////////////////////////////////////////////////////////////////////
// ����͸� ������ �α��� ��û
// 
// Parameter: (CPacket*)��Ŷ, (BYTE)���� ��ȣ, (int), (int)
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitorClient::mpUpdateMonitor(CPacket* pSendPacket, BYTE DataType, int DataValue, int TimeStamp)
{
	// ��Ŷ ����
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