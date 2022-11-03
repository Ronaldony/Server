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
	// ����͸� ������ �ʱ�ȭ
	memset(&_OngoingMonitoring, 0, sizeof(_OngoingMonitoring));
	memset(&_ResultMonitoring, 0, sizeof(_ResultMonitoring));

	// �ý��� �α� ���丮 ����
	CSystemLog::SystemLog_Directory(L"LOGIN_SERVER_LOG");

	// Config ���Ͽ��� ���� �� �ҷ�����
	CParserUnicode parser;

	if (false == parser.LoadFile(L"LoginServer.cnf"))
	{
		wprintf(L"CONFIG FILE LOAD FAILED\n");
		CCrashDump::Crash();
	}
	
	wprintf(L"------------------------------------------------------------------\n");
	wprintf(L"---------------------CHAT SERVER CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen ���� ���ε� IP
	if (parser.GetString(L"BIND_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen ���� ��Ʈ
	if (parser.GetValue(L"BIND_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP ��Ŀ ������ ����
	if (parser.GetValue(L"IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP ���� ���� ������ ����
	if (parser.GetValue(L"IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	DWORD dwMaxSession;			// �ִ� ����
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
	// DB ����
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

	// DB ���� ����
	_pDBConnector = new CDBConnectorTLS(wchDBIP, wchDBAccount, wchDBPASS, wchDBTable, iDBPort);

	// Ÿ�Ӿƿ�
	_hTimeoutThread = (HANDLE)_beginthreadex(NULL, 0, Proc_TimeCheck, (void*)this, 0, (unsigned int*)&_dwTimeoutThreadID);
	if (_hTimeoutThread == 0 || _hTimeoutThread == (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex error\n");
		CCrashDump::Crash();
	}

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
	InitializeSRWLock(&_AccountMapLock);

	// NetServer ����
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, iPacketCode, iPacketKey, false);

	_RedisClient.connect();
}

LoginServer::~LoginServer()
{

}

////////////////////////////////////////////////////////////////////////
// ���� ��� ���� �˻�. �̰����� ȭ��Ʈ ����Ʈ ����
// 
// Parameter: (WCHAR*)����� IP, (DWORD)��Ʈ ��ȣ
// return: ����
////////////////////////////////////////////////////////////////////////
bool LoginServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
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
void LoginServer::OnClientJoin(ULONG64 uldSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID
// void: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::OnClientLeave(ULONG64 uldSessionID)
{
	//--------------------------------
	// �����ʿ��� ���� ����
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
// ��û ��Ŷ ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ���� ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::OnRecv(ULONG64 uldSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// �޽��� �Ľ� 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// �޽��� Ÿ�Կ� ���� ���ν��� ȣ��
	switch (wType)
	{
	case en_PACKET_CS_LOGIN_REQ_LOGIN:		// ä�ü��� �α��� ��û
		PacketProc_LoginUser(uldSessionID, pPacket);
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
void LoginServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: ���� �����忡 � ���·� ������ �� ����غ���
	if (0 != errorcode)
		LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// ���� Ÿ�Ӿƿ� �˻� ���ν���
// 
// Parameter: (void*)LoginServer ��ü �ּ�
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

			// TPS ����
			pServer->_dwTimeOutTick = dwNowTick;
			
			pServer->CheckUserTimeout();
			

		}

		Sleep(100);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
// �α��� ��û ��Ŷ ó��
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::PacketProc_LoginUser(ULONG64 uldSessionID, CPacket* pPacket)
{
	//------------------------------------
	// �α��� ����
	//------------------------------------
	INT64	AccountNo;
	char	SessionKey[65];		// ������ū

	*pPacket >> AccountNo;
	pPacket->GetData((char*)&SessionKey, sizeof(SessionKey) - 1);

	// TODO: �ۺ��ŷκ��� ȸ�� ���� ��ȸ (���� �׽�Ʈ�� ȸ�� DB)
	MYSQL_ROW sql_result;

	//------------------------------------
	// account ���̺� ��ȸ
	//------------------------------------
	if (false == _pDBConnector->Query(L"SELECT userid, usernick FROM account WHERE accountno=%d", AccountNo))
	{
		LOG(L"PacketProc_LoginUser", CSystemLog::LEVEL_SYSTEM, L"DB Query Fail. Error number: %d, Msg: %s\n", _pDBConnector->GetLastError(), _pDBConnector->GetLastErrorMsg());
		return;
	}

	WCHAR	ID[20];				// null ����
	WCHAR	Nickname[20];		// null ����

	sql_result = _pDBConnector->FetchRow();
	if (false == UTF8ToUTF16(ID, sizeof(ID), sql_result[0], (int)strlen(sql_result[0])))
		return;

	if (false == UTF8ToUTF16(Nickname, sizeof(Nickname), sql_result[1], (int)strlen(sql_result[1])))
		return;

	_pDBConnector->FreeResult();


	//------------------------------------
	// sessionkey ���̺� ��ȸ
	//------------------------------------
	//CHAR chSessionKey[64];

	if (false == _pDBConnector->Query(L"SELECT sessionkey FROM sessionkey WHERE accountno=%d", AccountNo))
	{
		LOG(L"PacketProc_LoginUser", CSystemLog::LEVEL_SYSTEM, L"DB Query Fail. Error number: %d, Msg: %s\n", _pDBConnector->GetLastError(), _pDBConnector->GetLastErrorMsg());
		return;
	}

	// DB�κ��� ����Ű ��ȸ
	//sql_result = _pDBConnector->FetchRow();
	//memcpy_s(chSessionKey, sizeof(chSessionKey), sql_result[0], strlen(sql_result[0]));
	
	_pDBConnector->FreeResult();

	//------------------------------------
	// ����Ű ����
	//------------------------------------


	//------------------------------------
	// status ���̺� ��ȸ
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
	// ȸ�� ���� ���
	//------------------------------------
	if (false == _pDBConnector->Query_Save(L"UPDATE status SET status=1 WHERE accountno=%d", AccountNo))
	{
		LOG(L"PacketProc_LoginUser", CSystemLog::LEVEL_SYSTEM, L"DB Query Fail. Error number: %d, Msg: %s\n", _pDBConnector->GetLastError(), _pDBConnector->GetLastErrorMsg());
		return;
	}

	//------------------------------------
	// Redis�� ���� ��ū ����(��ū ������ Account, ���� ��ū ����)
	//------------------------------------

	char chAccount[17] = {0, };
	// INT64 -> ���ڷ� ��ȯ
	_i64toa_s(AccountNo, chAccount, sizeof(chAccount), 16);
	// ���ڿ� ���·� Redis�� ���� ����
	SessionKey[64] = 0;				

	_RedisClient.set(chAccount, SessionKey);
	_RedisClient.expire(chAccount, 5);			// 5�� �� Ű ����
	_RedisClient.sync_commit();

	//------------------------------------
	// UserMap �߰�
	//------------------------------------
	st_USER_INFO* pUser = st_USER_INFO::Alloc();

	pUser->LastRecvMsgTime = GetTickCount64();
	pUser->SessionID = uldSessionID;

	AcquireSRWLockExclusive(&_UserMapLock);
	AddUser(uldSessionID, pUser);
	ReleaseSRWLockExclusive(&_UserMapLock);

	//------------------------------------
	// �޽��� ����
	//------------------------------------
	CPacket* pSendPacket = CPacket::Alloc();
	mpLoginRes(pSendPacket, AccountNo, 1, ID, Nickname, _wchGameSrvIP, (USHORT)_dwGameSrvPort, _wchChatSrvIP, (USHORT)_dwChatSrvPort);
	SendPacket(uldSessionID, pSendPacket);
	CPacket::Free(pSendPacket);

	InterlockedIncrement64(&_OngoingMonitoring.NowLoginUserNum);
	InterlockedIncrement64(&_OngoingMonitoring.AuthCountTPS);
}


////////////////////////////////////////////////////////////////////////
// �α��� ���� �޽��� ����
// 
// Parameter: (CPacket*)���� ������ ���� ����ȭ����, (INT64)���� ��� Account ��ȣ, (BYTE)���� ���� ����
// void: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status, WCHAR* ID, WCHAR* NickName, WCHAR* GameServerIP, USHORT GameServerPort, WCHAR* ChatServerIP, USHORT ChatServerPort)
{
	//------------------------------------------------------------
	// �α��� �������� Ŭ���̾�Ʈ�� �α��� ����
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		BYTE	Status				// 0 (���ǿ���) / 1 (����) ...  �ϴ� defines ���
	//
	//		WCHAR	ID[20]				// ����� ID		. null ����
	//		WCHAR	Nickname[20]		// ����� �г���	. null ����
	//
	//		WCHAR	GameServerIP[16]	// ���Ӵ�� ����,ä�� ���� ����
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
// ���� ����
// 
// Parameter: (ULONG64)���� ���� ID, (INT64)���� Account ��ȣ, (char*)���� Ű
// return: (bool)���� ���� ����
////////////////////////////////////////////////////////////////////////
bool LoginServer::AuthenticateUser(ULONG64 uldSessionID, INT64 AccountNo, char* SessionKey)
{
	// TODO: ���� ���� ����
	return true;
}

////////////////////////////////////////////////////////////////////////
// UTF8 ���ڵ� ���ڸ� UTF16 ���ڵ� ���ڷ� ��ȯ
// 
// Parameter: (WCHAR*)��¹���, (int)��� ���� ũ��, (char*)�Է¹���, (int)�Է� ���� ũ��
// return: (true)����, (false)����
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
// ���� Ÿ�� �ƿ� �˻�
// 
// Parameter: ����
// return: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::CheckUserTimeout(void)
{
	ULONG64 uldSessionIDArr[en_CHECK_TIMEOUT];
	int iSessionCount = 0;

	//-----------------------------------------------
	// �������� ��ȸ�ϸ� ������ �޽��� ���� �ð� �˻�
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
// ���� �ʿ� ���� �߰�
// 
// Parameter: (ULONG64)���� ���� ID, (st_USER_INFO*)���� ���� ������
// return: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::AddUser(ULONG64 uldSessionID, st_USER_INFO* pUser)
{
	_UserMap.insert(std::make_pair(uldSessionID, pUser));
}

////////////////////////////////////////////////////////////////////////
// ���� �ʿ��� ���� ã��
// 
// Parameter: (ULONG64)���� ���� ID
// return: ���� ���� ����ü �ּ�
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
// ���� �ʿ��� ���� ����
// 
// Parameter: (ULONG64)���� ���� ID
// return: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::DeleteUser(ULONG64 uldSessionID)
{
	_UserMap.erase(uldSessionID);
}

////////////////////////////////////////////////////////////////////////
// Account �ʿ� Account ��ȣ �߰�
// 
// Parameter: (ULONG64)Account ��ȣ, (st_USER_INFO*)���� ��ü �ּ�
// return: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::AddAccount(ULONG64 uldAccount, ULONG64 uldSessionID)
{
	_AccountMap.insert(std::make_pair(uldAccount, uldSessionID));
}

////////////////////////////////////////////////////////////////////////
// Account �ʿ��� Account ��ȣ �˻�
// 
// Parameter: (ULONG64)Account ��ȣ
// return: Account ��ȣ�� ����. (0 < return)����, (0)����
////////////////////////////////////////////////////////////////////////
ULONG64 LoginServer::FindAccount(ULONG64 uldAccount)
{
	std::unordered_multimap<ULONG64, ULONG64>::iterator iter = _AccountMap.find(uldAccount);
	if (iter != _AccountMap.end())
		return iter->second;

	return 0;
}

////////////////////////////////////////////////////////////////////////
// Account �ʿ��� Account ��ȣ ����
// 
// Parameter: (ULONG64)Account ��ȣ
// return: ����
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
// ä�� ���� ����͸�
// 
// Parameter: ����
// void: ����
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

	// ����� ����
	_ResultMonitoring.AuthCountTPS = InterlockedExchange64(&_OngoingMonitoring.AuthCountTPS, 0);

	// ����͸� ������ ����
	_ProcessMonitor.UpdateMonitorInfo();

	int iTime = (int)time(NULL);

	// �α��μ��� CPU ����
	CPacket* pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU, (int)(_ProcessMonitor.ProcessTotal()), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// �α��μ��� �޸� ��� MByte
	pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM, (int)(_ProcessMonitor.ProcessUserAllocMemory() / dfBYTES_MEGA), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// �α��μ��� ���� �� (���ؼ� ��)
	pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_SESSION, (int)(mi.NowSessionNum), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// �α��μ��� ���� ó�� �ʴ� Ƚ��
	pSendPacket = CPacket::Alloc();
	_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS, (int)(_ResultMonitoring.AuthCountTPS), iTime);
	_MonitorClient.SendPacket(pSendPacket);
	CPacket::Free(pSendPacket);

	// �α��μ��� ��ŶǮ ��뷮
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

		// �������ϸ� ������ ���� ���
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
// ���� ��Ʈ�� Ÿ��
// 
// Parameter: ����
// return: ����
////////////////////////////////////////////////////////////////////////
void LoginServer::ServerControl(void)
{
	// Ű���� ��Ʈ�� ���, Ǯ�� ����
	static bool bControlMode = false;

	//---------------------------------------------
	// L: ��Ʈ�� Lock  / U: ��Ʈ�� Unlock  / Q: ���� ���� / O: �α� ���� ����
	//---------------------------------------------
	if (_kbhit())
	{
		WCHAR ControlKey = _getwch();

		// Ű���� ���� ���
		if ((L'u' == ControlKey) || (L'U' == ControlKey))
		{
			bControlMode = true;

			// ���� Ű ���� ���
			wprintf(L"Control Mode..! Press Q - Quit \n");
			wprintf(L"Control Mode..! Press L - Control Lock \n");
			wprintf(L"Control Mode..! Press O - Change Log Level \n");
			wprintf(L"Control Mode..! Press S - Save / Not Save Log \n");
		}

		// Ű���� ���� ���
		if ((L'l' == ControlKey) || (L'L' == ControlKey))
		{
			bControlMode = false;

			// ���� Ű ���� ���
			wprintf(L"Control Mode..! Press U - Control Unlock \n");
		}

		// ���� ����
		if ((L'q' == ControlKey && bControlMode) || (L'Q' == ControlKey))
		{
			//g_bShutdown = true;
		}

		// �α� ���� ����
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

		// Ű�������� Ǯ�� ���¿��� Ư�� ���
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