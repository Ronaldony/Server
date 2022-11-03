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
	// Config ���Ͽ��� ���� �� �ҷ�����
	CParserUnicode parser;

	if (false == parser.LoadFile(L"MonitoringServer.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");
	
	wprintf(L"------------------------------------------------------------------------\n");
	wprintf(L"---------------------LAN MONITOR SERVER CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen ���� ���ε� IP
	if (parser.GetString(L"LAN_BIND_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen ���� ��Ʈ
	if (parser.GetValue(L"LAN_BIND_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP ��Ŀ ������ ����
	if (parser.GetValue(L"LAN_IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP ���� ���� ������ ����
	if (parser.GetValue(L"LAN_IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	DWORD dwMaxSession;			// �ִ� ����
	if (parser.GetValue(L"LAN_SESSION_MAX", (int*)&dwMaxSession))
		wprintf(L"-----SESSION_MAX: %d\n", dwMaxSession);

	wchar_t wchLogLevel[50] = { 0, };
	if (parser.GetString(L"LAN_LOG_LEVEL", wchLogLevel))
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
	swprintf_s(_LogFileName, sizeof(_LogFileName), L"Chat Server Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// Account �� ����ȭ ��ü �ʱ�ȭ
	InitializeSRWLock(&_UserMapLock);

	// Lan ���� ����
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, false);
}

MonitoringLanServer::~MonitoringLanServer()
{

}

////////////////////////////////////////////////////////////////////////
// ���� ��� ���� �˻�. �̰����� ȭ��Ʈ ����Ʈ ����
// 
// Parameter: (WCHAR*)����� IP, (DWORD)��Ʈ ��ȣ
// return: ����
////////////////////////////////////////////////////////////////////////
bool MonitoringLanServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
{
	// TODO: ȭ��Ʈ ����Ʈ IP �˻��Ͽ� ���� Ȯ��
	//for (int cnt = 0; cnt < _whiteListIPCount; cnt++)
	//	if (ip != _whiteListIP) return false;
	
	// TODO: ���ο� ���� ���� �޽��� EnQ
	//_ProcedureMsgQ.Enqueue(���ο� ���� ���� �޽���);

	return true;
}

////////////////////////////////////////////////////////////////////////
// �ű� ���� ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::OnClientJoin(ULONG64 udlSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::OnClientLeave(ULONG64 udlSessionID)
{
	//--------------------------------
	// �����ʿ��� �α׾ƿ� ���� ����
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
// ��û ��Ŷ ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ���� ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::OnRecv(ULONG64 udlSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// �޽��� �Ľ� 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// �޽��� Ÿ�Կ� ���� ���ν��� ȣ��
	switch (wType)
	{
	case en_PACKET_TYPE::en_PACKET_SS_MONITOR_LOGIN:			// ������ ���� �α���
		PacketProc_LoginServer(udlSessionID, pPacket);
		break;
	case en_PACKET_TYPE::en_PACKET_SS_MONITOR_DATA_UPDATE:		// ������ �����κ��� ����� ���� ����
	{
		if (_NetMonitor != NULL)
			_NetMonitor->PacketProc_UpdateServer(pPacket);
	}
		break;
	}

	return;
}

////////////////////////////////////////////////////////////////////////
// ���� �߻� �ݹ� �Լ�
// 
// Parameter: (int)���� �ڵ�, (const WCHAR*)���� �ڸ�Ʈ
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: ���� �����忡 � ���·� ������ �� ����غ���
	//if (0 != errorcode)
		//LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}


////////////////////////////////////////////////////////////////////////
// �α��� ��û ��Ŷ ó��
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::PacketProc_LoginServer(ULONG64 udlSessionID, CPacket* pPacket)
{
	//------------------------------------
	// ������ ���� �α���
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
// ���� �ʿ� ���� �߰�
// 
// Parameter: (ULONG64)���� ���� ID, (st_USER_INFO*)���� ���� ������
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::AddUser(ULONG64 udlSessionID, st_LAN_MONITOR_USER_INFO* pUser)
{
	_UserMap.insert(std::make_pair(udlSessionID, pUser));
}

////////////////////////////////////////////////////////////////////////
// ���� �ʿ��� ���� ã��
// 
// Parameter: (ULONG64)���� ���� ID
// return: ���� ���� ����ü �ּ�
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
// ���� �ʿ��� ���� ����
// 
// Parameter: (ULONG64)���� ���� ID
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringLanServer::DeleteUser(ULONG64 udlSessionID)
{
	_UserMap.erase(udlSessionID);
}