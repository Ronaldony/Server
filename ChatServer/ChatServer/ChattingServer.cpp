#include "stdafx.h"
#include <time.h>
#include <conio.h>
#include <Pdh.h>
#include <cpp_redis/cpp_redis>
#include "ChattingServer.h"
#include "CommonProtocol.h"
#include "TextParset_Unicode.h"
#include "SystemLog.h"
#include "CCrashDump.h"

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#pragma comment(lib, "Winmm.lib")

using namespace std;

ChattingServer::ChattingServer() :
	CNetServer()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	_dwCoreNum = si.dwNumberOfProcessors;

	time_t timer = time(NULL);

	(void)localtime_s(&_StartTime, &timer);

	memset(&_MonitorData, 0, sizeof(_MonitorData));
	_MonitorData.TPSMIN_ProcMSG = 0xFFFFFFFFFFFFF;
	
	// Config ���Ͽ��� ���� �� �ҷ�����
	CParserUnicode parser;

	if (false == parser.LoadFile(L"ChatServer.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");
	
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

	_dwWorkerThreadNum = dwWorkderThreadNum;

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

	wprintf(L"------------------------------------------------------------------\n");

	// Ÿ�Ӿƿ�
	_hTimeCheckThread = (HANDLE)_beginthreadex(NULL, 0, Proc_TimeCheck, (void*)this, 0, (unsigned int*)&_dwTimeCheckThreadsID);
	if (_hTimeCheckThread== 0 || _hTimeCheckThread== (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex error\n");
		int* pDown = NULL;
		*pDown = 0;
	}

	// �α� ���� ����
	if (!wcscmp(wchLogLevel, L"DEBUG"))
		LOG_LEVEL(CSystemLog::LEVEL_DEBUG);
	else if (!wcscmp(wchLogLevel, L"WARNING"))
		LOG_LEVEL(CSystemLog::LEVEL_ERROR);
	else
		LOG_LEVEL(CSystemLog::LEVEL_SYSTEM);

	// �α� ���� ����
	timer = time(NULL);
	struct tm t;

	(void)localtime_s(&t, &timer);
	swprintf_s(_LogFileName, sizeof(_LogFileName) / sizeof(WCHAR), L"Chat Server Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// Account �� ����ȭ ��ü �ʱ�ȭ
	InitializeSRWLock(&_UserMapLock);

	for (int cntY = 0; cntY < dfSECTOR_MAX_Y; ++cntY)
	{
		for (int cntX = 0; cntX < dfSECTOR_MAX_Y; ++cntX)
		{
			InitializeSRWLock(&_SectorMapLock[cntY][cntX]);
		}
	}

	// NetServer ����
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, iPacketCode, iPacketKey, false);
	
	_RedisClient.connect();
}

ChattingServer::~ChattingServer()
{

}

////////////////////////////////////////////////////////////////////////
// ���� ��� ���� �˻�. �̰����� ȭ��Ʈ ����Ʈ ����
// 
// Parameter: (WCHAR*)����� IP, (DWORD)��Ʈ ��ȣ
// return: ����
////////////////////////////////////////////////////////////////////////
bool ChattingServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
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
void ChattingServer::OnClientJoin(ULONG64 udlSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::OnClientLeave(ULONG64 udlSessionID)
{
	//--------------------------------
	// �����ʿ��� �α׾ƿ� ���� ����
	//--------------------------------
	AcquireSRWLockExclusive(&_UserMapLock);

	st_USER_INFO* pUser = FindUser(udlSessionID);
	if (NULL == pUser)
	{
		ReleaseSRWLockExclusive(&_UserMapLock);
		LOG(L"OnClientLeave", CSystemLog::LEVEL_ERROR, L"Can't find User: %lld\n", udlSessionID);
		return;
	}
	
	DeleteUser(pUser->SessionID);
	DeleteAccount(pUser->AccountNo, pUser->SessionID);

	ReleaseSRWLockExclusive(&_UserMapLock);

	//--------------------------------
	// ���͸ʿ��� �α׾ƿ� ���� ����
	//--------------------------------
	if ((dfSECTOR_DEFAULT != pUser->SectorY) && (dfSECTOR_DEFAULT != pUser->SectorX))
	{
		WORD SectorX = pUser->SectorX;
		WORD SectorY = pUser->SectorY;

		AcquireSRWLockExclusive(&_SectorMapLock[SectorY][SectorX]);
		DeleteSector(SectorY, SectorX, pUser->SessionID);
		ReleaseSRWLockExclusive(&_SectorMapLock[SectorY][SectorX]);
	}

	InterlockedDecrement64(&_dlNowLoginUserNum);

	st_USER_INFO::Free(pUser);
}

////////////////////////////////////////////////////////////////////////
// ��û ��Ŷ ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ���� ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::OnRecv(ULONG64 udlSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// �޽��� �Ľ� 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// �޽��� Ÿ�Կ� ���� ���ν��� ȣ��
	switch (wType)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:		// ä�ü��� �α��� ��û
		PacketProc_LoginUser(udlSessionID, pPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:	// ���� �̵�
		PacketProc_MoveSector(udlSessionID, pPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:		// ä�� �Է�
		PacketProc_Chatting(udlSessionID, pPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:	// ��Ʈ��Ʈ
		PacketProc_HeartBeat(udlSessionID);
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
void ChattingServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: ���� �����忡 � ���·� ������ �� ����غ���
	//if (0 != errorcode)
		//LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// ���� Ÿ�Ӿƿ� �˻� ���ν���
// 
// Parameter: (void*)ChattingServer ��ü �ּ�
// return: 
////////////////////////////////////////////////////////////////////////
unsigned int __stdcall ChattingServer::Proc_TimeCheck(void* pvParam)
{
	ChattingServer* pServer = (ChattingServer*)pvParam;

	pServer->_udlUserTimeTick = timeGetTime();
	pServer->_udlMonitorTick = pServer->_udlUserTimeTick;

	ULONG64 udlPrevTick = pServer->_udlUserTimeTick;
	int iTimeDec;

	while (1)
	{
		ULONG64 udlNowTick = timeGetTime();
		
		//---------------------------------------------
		// ���� Ÿ�Ӿƿ� üũ
		//---------------------------------------------
		if (abs((long long)(udlNowTick - pServer->_udlUserTimeTick)) > en_USER_TIME_OUT_PERIOD)
		{
			// TPS ����
			pServer->_udlUserTimeTick = udlNowTick - (udlNowTick - pServer->_udlUserTimeTick - en_USER_TIME_OUT_PERIOD);

			pServer->CheckUserTimeout();
		}

		//---------------------------------------------
		// ����� ������ ����� ���� ����
		//---------------------------------------------
		if (abs((long long)(udlNowTick - pServer->_udlMonitorTick)) > en_MONITOR_TIME_PERIOD)
		{
			// TPS ����
			pServer->_udlMonitorTick = udlNowTick - (udlNowTick - pServer->_udlMonitorTick - en_MONITOR_TIME_PERIOD);

			pServer->_ProcessMonitor.UpdateMonitorInfo();
			int iTime = (int)time(NULL);

			// ChatServer ���� ���� ON / OFF
			CPacket* pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, true, iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// CPU ����
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)(pServer->_ProcessMonitor.ProcessTotal()), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// �޸� ��� MByte
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)(pServer->_ProcessMonitor.ProcessUserAllocMemory() / 1000000), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// ���� �� (���ؼ� ��)
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SESSION, (int)(pServer->_dlNowSessionNum), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// UPDATE ������ �ʴ� �ʸ� Ƚ��
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)(pServer->_MonitorData.TPS_ProcMSGMonitor), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// ��ŶǮ ��뷮
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)(CPacket::GetCountOfPoolAlloc()), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// �������� ����� �� (���� ������)
			ULONG64 userNum = pServer->_dlNowLoginUserNum;
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_PLAYER, (int)(pServer->_dlNowLoginUserNum), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			//---------------------------------------------
			// �ִ� �ּ� �� ���
			//---------------------------------------------
			
			pServer->_MonitorData.TPS_ProcMSGMonitor = 0;

			// �޽��� ó�� �ִ� �ּ�
			if (pServer->_MonitorData.TPS_ProcMSG != 0)
			{
				pServer->_MonitorData.TPS_ProcMSGMonitor = InterlockedExchange64(&pServer->_MonitorData.TPS_ProcMSG, 0);
				pServer->_MonitorData.MonitorCount_ProcMSG++;
				if (pServer->_MonitorData.TPS_ProcMSGMonitor > pServer->_MonitorData.TPSMAX_ProcMSG)
					pServer->_MonitorData.TPSMAX_ProcMSG = pServer->_MonitorData.TPS_ProcMSGMonitor;
				else if (pServer->_MonitorData.TPS_ProcMSGMonitor < pServer->_MonitorData.TPSMIN_ProcMSG)
					pServer->_MonitorData.TPSMIN_ProcMSG = pServer->_MonitorData.TPS_ProcMSGMonitor;
			}

			pServer->_MonitorData.Total_ProcMSG += pServer->_MonitorData.TPS_ProcMSGMonitor;
		}

		iTimeDec = (int)(udlNowTick - udlPrevTick - en_SLEEP_TIME_CHECK_PERIOD);

		if (iTimeDec > en_SLEEP_TIME_CHECK_PERIOD)
			iTimeDec = en_SLEEP_TIME_CHECK_PERIOD / 2;

		udlPrevTick = udlNowTick;

		Sleep(en_SLEEP_TIME_CHECK_PERIOD - iTimeDec);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
// �α��� ��û ��Ŷ ó��
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::PacketProc_LoginUser(ULONG64 udlSessionID, CPacket* pPacket)
{
	//------------------------------------
	// �α��� ����
	//------------------------------------
	INT64	AccountNo;
	WCHAR	ID[20];				// null ����
	WCHAR	Nickname[20];		// null ����
	char	SessionKey[64];		// ������ū

	*pPacket >> AccountNo;
	pPacket->GetData((char*)&ID, sizeof(ID));
	pPacket->GetData((char*)&Nickname, sizeof(Nickname));
	pPacket->GetData((char*)&SessionKey, sizeof(SessionKey));

	// �α��� ���� ����
	bool bRetVal = AuthenticateUser(udlSessionID, AccountNo, SessionKey);

	if (bRetVal)
		AddUserInfo(udlSessionID, AccountNo, ID, Nickname);

	InterlockedIncrement64(&_MonitorData.TPS_ProcMSG);
	// false �� ���� �ִ� ���� ���� �ʰ�
	if (true == bRetVal)
	{
		CPacket* pSendPacket = CPacket::Alloc();
		mpLoginRes(pSendPacket, AccountNo, bRetVal);
		SendPacket(udlSessionID, pSendPacket);
		CPacket::Free(pSendPacket);
	}
}

////////////////////////////////////////////////////////////////////////
// ���� �̵� ��û ��Ŷ ó��
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ����ȭ����
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::PacketProc_MoveSector(ULONG64 udlSessionID, CPacket* pPacket)
{
	INT64	AccountNo;
	*pPacket >> AccountNo;

	WORD	SectorX;
	WORD	SectorY;

	*pPacket >> SectorX;
	*pPacket >> SectorY;

	// ���� ���� �˻�
	if (SectorX < 0 || SectorX > 50)
	{
		LOG(L"PacketProc_MoveSector", CSystemLog::LEVEL_ERROR, L"Received Sector X Range exceeds\n");
		Disconnect(udlSessionID);
		return;
	}

	if (SectorY < 0 || SectorY > 50)
	{
		LOG(L"PacketProc_MoveSector", CSystemLog::LEVEL_ERROR, L"Received Sector Y Range exceeds\n");
		Disconnect(udlSessionID);
		return;
	}

	//------------------------------------
	// ������ ���� ���� ����
	//------------------------------------
	AcquireSRWLockShared(&_UserMapLock);

	st_USER_INFO* pUser = FindUser(udlSessionID);
	if (NULL == pUser)
	{
		// ���� �α��� ���� �˻�
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(udlSessionID);
		LOG(L"PacketProc_MoveSector", CSystemLog::LEVEL_ERROR, L"Not Found User ID: %lld\n", udlSessionID);
		return;
	}
	else if (AccountNo != pUser->AccountNo)
	{
		// Account ��ȣ �˻�
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(udlSessionID);
		LOG(L"PacketProc_MoveSector", CSystemLog::LEVEL_ERROR, L"AccountNo Error.Server: % lld / Recv : % lld\n", pUser->AccountNo, AccountNo);
		return;
	}

	WORD wPrevSectorY;
	WORD wPrevSectorX;

	wPrevSectorY = pUser->SectorY;
	wPrevSectorX = pUser->SectorX;
	pUser->SectorY = SectorY;
	pUser->SectorX = SectorX;

	pUser->LastRecvMsgTime = GetTickCount64();

	ReleaseSRWLockShared(&_UserMapLock);

	//------------------------------------
	// ���͸� ���� �� �߰�
	//------------------------------------
	if ((dfSECTOR_DEFAULT != wPrevSectorY) && (dfSECTOR_DEFAULT != wPrevSectorX))
	{
		AcquireSRWLockExclusive(&_SectorMapLock[wPrevSectorY][wPrevSectorX]);

		DeleteSector(wPrevSectorY, wPrevSectorX, udlSessionID);
		
		ReleaseSRWLockExclusive(&_SectorMapLock[wPrevSectorY][wPrevSectorX]);
	}

	AcquireSRWLockExclusive(&_SectorMapLock[SectorY][SectorX]);

	AddSector(SectorY, SectorX, udlSessionID);

	ReleaseSRWLockExclusive(&_SectorMapLock[SectorY][SectorX]);

	InterlockedIncrement64(&_MonitorData.TPS_ProcMSG);

	//------------------------------------
	// ���� �̵� ���� ����
	//------------------------------------
	CPacket* pSendPacket = CPacket::Alloc();
	mpMoveSectorRes(pSendPacket, AccountNo, SectorX, SectorY);
	SendPacket(udlSessionID, pSendPacket);
	CPacket::Free(pSendPacket);
}

////////////////////////////////////////////////////////////////////////
// ä�� ��û ��Ŷ ó��
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ����ȭ����
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::PacketProc_Chatting(ULONG64 udlSessionID, CPacket* pPacket)
{
	bool isError = false;

	INT64	AccountNo;
	*pPacket >> AccountNo;

	WORD	MessageLen;
	*pPacket >> MessageLen;

	if (MessageLen != pPacket->GetDataSize())
	{
		// ������ �޽����� ���̿� �޽��� ���� ������ �ٸ� ���
		LOG(L"PacketProc_Chatting", CSystemLog::LEVEL_ERROR, L"Chatting Message Lenght Error. Recv Len: %d / Real Length: %d\n",
			MessageLen, pPacket->GetDataSize());
		Disconnect(udlSessionID);
		return;
	}

	CPacket* pSendPakcet = CPacket::Alloc();
	 
	//------------------------------------
	// ���� ���� ���� �� ä�� ���� �޽��� ����
	//------------------------------------
	AcquireSRWLockShared(&_UserMapLock);
	st_USER_INFO* pUser = FindUser(udlSessionID);

	if (NULL == pUser)
	{
		// ���� �α��� ���� Ȯ��
		LOG(L"PacketProc_Chatting", CSystemLog::LEVEL_ERROR, L"Can't find the user: %lld\n", udlSessionID);
		isError = true;
	}
	else if (AccountNo != pUser->AccountNo)
	{
		// ���� Account ��ȣ �˻�
		LOG(L"PacketProc_Chatting", CSystemLog::LEVEL_ERROR, L"AccountNo Error. Server: %lld / Recv: %lld\n", pUser->AccountNo, AccountNo);
		isError = true;
	}
	else if ((dfSECTOR_DEFAULT == pUser->SectorY) || (dfSECTOR_DEFAULT == pUser->SectorX))
	{
		// ���� ��ǥ�� ���� �Էµ��� ���� ��Ȳ �˻�
		LOG(L"PacketProc_Chatting", CSystemLog::LEVEL_ERROR, L"Sector Not Input yet\n");
		isError = true;
	}

	if (isError == true)
	{
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(udlSessionID);
		CPacket::Free(pSendPakcet);
		return;
	}

	WORD	wUserSectorY;
	WORD	wUserSectorX;

	wUserSectorY = pUser->SectorY;
	wUserSectorX = pUser->SectorX;
	mpChatting(pSendPakcet, AccountNo, pUser->ID, pUser->Nickname, MessageLen, (WCHAR*)(pPacket->GetBufferReadPtr()));
	pUser->LastRecvMsgTime = GetTickCount64();

	ReleaseSRWLockShared(&_UserMapLock);

	//------------------------------------
	// ä�� ���� �޽��� ó��
	//------------------------------------
	int iAroundSessionNum = 0;
	st_SECTOR_AROUND aroundSector;

	aroundSector.Count = 0;
	// �ֺ� ���� ���� �˻�
	GetSectorAround(wUserSectorX, wUserSectorY, &aroundSector);

	//--------------------------------------------------------------
	// ��� ä�� ���۷� ���
	// 50x50 ���͸� �������� �� ���ʹ� ��� 8.76 ���͸� ����Ѵ�.
	// (���θ�(4*4) + ���ʳ�(6*4*48) + �� �� (9*2304)) / (50*50) = 8.76
	// ����: ��ġ ������ ���� ������ 1������ ��� 1���ʹ�
	// ��� 4���� ������ ��ġ�Ѵ�. 
	// ���� ��� ä�� ���۷� = 8.76 * 4 = 35.04�� �ȴ�.
	//--------------------------------------------------------------

	ULONG64 uldSessionIDArr[en_CHECK_CHATUSER];

	for (int cnt = 0; cnt < aroundSector.Count; cnt++)
	{
		int iSectorY = aroundSector.Around[cnt].iY;
		int iSectorX = aroundSector.Around[cnt].iX;

		AcquireSRWLockShared(&_SectorMapLock[iSectorY][iSectorX]);

		//std::unordered_set<ULONG64>::iterator iter;
		std::unordered_set<ULONG64>::iterator iter;
		std::unordered_set<ULONG64>::iterator iterEnd = _SectorPosMap[iSectorY][iSectorX].end();

		for (iter = _SectorPosMap[iSectorY][iSectorX].begin(); iter != iterEnd; ++iter)
		{
			uldSessionIDArr[iAroundSessionNum++] = *iter;

			if (iAroundSessionNum < en_CHECK_CHATUSER)
				continue;

			// �ֺ� ���� ä�� ���� ���� �ʰ�
			LOG(L"Proc_ChatJob", CSystemLog::LEVEL_ERROR, L"Chat User Maximum\n");
			break;
		}

		ReleaseSRWLockShared(&_SectorMapLock[iSectorY][iSectorX]);
	}

	InterlockedIncrement64(&_MonitorData.TPS_ProcMSG);

	for (int cnt = 0; cnt < iAroundSessionNum; cnt++)
		SendPacket(uldSessionIDArr[cnt], pSendPakcet);

	CPacket::Free(pSendPakcet);

	return;
}

////////////////////////////////////////////////////////////////////////
// ��Ʈ��Ʈ ��û ��Ŷ ó��
// 
// Parameter: (ULONG64)���� ���� ID
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::PacketProc_HeartBeat(ULONG64 udlSessionID)
{
	//------------------------------------
	// ���� ���� �ð� ����
	//------------------------------------
	AcquireSRWLockShared(&_UserMapLock);

	st_USER_INFO* pUser = FindUser(udlSessionID);
	if (NULL == pUser)
	{
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(udlSessionID);
		LOG(L"PacketProc_HeartBeat", CSystemLog::LEVEL_ERROR, L"Can't find User: %lld\n", udlSessionID);
		return;
	}

	pUser->LastRecvMsgTime = GetTickCount64();

	ReleaseSRWLockShared(&_UserMapLock);

	InterlockedIncrement64(&_MonitorData.TPS_ProcMSG);
}

////////////////////////////////////////////////////////////////////////
// ���� �ʿ� ���� �߰�
// 
// Parameter: (ULONG64)���� ���� ID, (st_USER_INFO*)���� ���� ������
// return: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::AddUser(ULONG64 udlSessionID, st_USER_INFO* pUser)
{
	_UserMap.insert(std::make_pair(udlSessionID, pUser));
}

////////////////////////////////////////////////////////////////////////
// ���� �ʿ��� ���� ã��
// 
// Parameter: (ULONG64)���� ���� ID
// return: ���� ���� ����ü �ּ�
////////////////////////////////////////////////////////////////////////
st_USER_INFO* ChattingServer::FindUser(ULONG64 udlSessionID)
{
	std::unordered_map<ULONG64, st_USER_INFO*>::iterator iter = _UserMap.find(udlSessionID);
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
void ChattingServer::DeleteUser(ULONG64 udlSessionID)
{
	_UserMap.erase(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// Account �ʿ� Account ��ȣ �߰�
// 
// Parameter: (ULONG64)Account ��ȣ, (st_USER_INFO*)���� ��ü �ּ�
// return: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::AddAccount(ULONG64 udlAccount, ULONG64 udlSessionID)
{
	_AccountMap.insert(std::make_pair(udlAccount, udlSessionID));
}

////////////////////////////////////////////////////////////////////////
// Account �ʿ��� Account ��ȣ �˻�
// 
// Parameter: (ULONG64)Account ��ȣ
// return: Account ��ȣ�� ����. (0 < return)����, (0)����
////////////////////////////////////////////////////////////////////////
ULONG64 ChattingServer::FindAccount(ULONG64 udlAccount)
{
	std::unordered_multimap<ULONG64, ULONG64>::iterator iter = _AccountMap.find(udlAccount);
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
void ChattingServer::DeleteAccount(ULONG64 udlAccount, ULONG64 udlSessionID)
{
	typedef std::unordered_multimap<ULONG64, ULONG64> MyMap;

	std::pair<MyMap::iterator, MyMap::iterator> pairData;
	for (pairData = _AccountMap.equal_range(udlAccount); pairData.first != pairData.second; pairData.first++)
	{
		if (udlSessionID != pairData.first->second)
			continue;

		_AccountMap.erase(pairData.first);
		break;
	}
}

////////////////////////////////////////////////////////////////////////
// Sector �ʿ� ���� ���� �߰�
// 
// Parameter: (WORD)���� ����, (ULONG64)���� ID
// return: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::AddSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID)
{
	_SectorPosMap[wSectorY][wSectorX].insert(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// Sector �ʿ��� ���� ���� �˻�
// 
// Parameter: (WORD)���� ����, (ULONG64)���� ID
// return: ���� ������ �ش��ϴ� ���� ID�� ����. (true)����, (false)����
////////////////////////////////////////////////////////////////////////
bool ChattingServer::FindSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID)
{
	if (_SectorPosMap[wSectorY][wSectorX].end() != _SectorPosMap[wSectorY][wSectorX].find(udlSessionID))
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////
// Sector �ʿ��� ���� ���� ����
// 
// Parameter: (WORD)���� ����, (ULONG64)���� ID
// return: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::DeleteSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID)
{
	_SectorPosMap[wSectorY][wSectorX].erase(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// ���� ����Ʈ�� ���ο� �α��� ���� �߰�
// 
// Parameter: (ULONG64), (INT64), (WCHAR*), (WCHAR*)
// return: (bool)���� �� ������ ���� ����
////////////////////////////////////////////////////////////////////////
bool ChattingServer::AddUserInfo(ULONG64 udlSessionID, INT64 AccountNo, WCHAR* ID, WCHAR* NickName)
{
	//--------------------------------
	// �ִ� ������ �ʰ� �� �α��� ����
	//--------------------------------
	if (_dwMaxLoginUserNum <= InterlockedIncrement64(&_dlNowLoginUserNum))
	{
		Disconnect(udlSessionID);
		LOG(L"AddUserInfo", CSystemLog::LEVEL_DEBUG, L"Max User LogIn\n", _dwMaxLoginUserNum);
		return false;
	}
	 
	//--------------------------------
	// ���� ���� �� ���� �ʿ� ���
	//--------------------------------
	st_USER_INFO* userInfo = st_USER_INFO::Alloc();
	userInfo->SessionID = udlSessionID;
	userInfo->AccountNo = AccountNo;
	userInfo->SectorX = dfSECTOR_DEFAULT;
	userInfo->SectorY = dfSECTOR_DEFAULT;
	memcpy_s(&userInfo->ID, 20 * sizeof(WCHAR), ID, 20 * sizeof(WCHAR));
	memcpy_s(&userInfo->Nickname, 20 * sizeof(WCHAR), NickName, 20 * sizeof(WCHAR));

	//--------------------------------
	// �����ʿ� �α��� ���� �߰�
	//--------------------------------
	ULONG64 udlDisSession;
	AcquireSRWLockExclusive(&_UserMapLock);

	userInfo->LastRecvMsgTime = GetTickCount64();

	udlDisSession = FindAccount(AccountNo);

	AddAccount(AccountNo, udlSessionID);
	AddUser(udlSessionID, userInfo);

	ReleaseSRWLockExclusive(&_UserMapLock);

	// �ߺ� �α����� ��� ���� ����
	if (0 != udlDisSession)
		Disconnect(udlDisSession);	// ���� ���� ���� ���� ����

	return true;
}

////////////////////////////////////////////////////////////////////////
// ���� Ÿ�� �ƿ� �˻�
// 
// Parameter: ����
// return: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::CheckUserTimeout(void)
{
	ULONG64 uldSessionIDArr[en_CHECK_TIMEOUT];
	int iSessionCount = 0;

	//-----------------------------------------------
	// �������� ��ȸ�ϸ� ������ �޽��� ���� �ð� �˻�
	//-----------------------------------------------
	_udlMaxSessionTimeMonitor = _udlMaxSessionTime;
	_udlMaxSessionTime = 0;

	AcquireSRWLockShared(&_UserMapLock);

	ULONG64 udlNowTick = GetTickCount64();

	std::unordered_map<ULONG64, st_USER_INFO*>::iterator iter;
	std::unordered_map<ULONG64, st_USER_INFO*>::iterator iterEnd = _UserMap.end();

	for (iter = _UserMap.begin(); iter != iterEnd; ++iter)
	{
		int iTime = abs((int)(udlNowTick - iter->second->LastRecvMsgTime));

		if (_udlMaxSessionTime < iTime)
			_udlMaxSessionTime = iTime;

		if (_udlMaxSessionTimeMonitorMax < iTime)
			_udlMaxSessionTimeMonitorMax = iTime;

		if (iTime < (int)_dwTimeoutValue)
			continue;
			
		uldSessionIDArr[iSessionCount++] = iter->first;
		if (iSessionCount >= en_CHECK_TIMEOUT)
			break;
	}

	ReleaseSRWLockShared(&_UserMapLock);

	for (int cnt = 0; cnt < iSessionCount; cnt++)
	{
		_udlDisConnSession[_udlDisConnCount] = uldSessionIDArr[cnt];
		_udlDisConnCount = (_udlDisConnCount + 1) & 0x1FFF;

		LOG(L"CheckUserTimeout", CSystemLog::LEVEL_DEBUG, L"Session ID: %lld / Count: %d\n", uldSessionIDArr[cnt], iSessionCount);
		Disconnect(uldSessionIDArr[cnt]);
	}
}

////////////////////////////////////////////////////////////////////////
// ���� ����
// 
// Parameter: (ULONG64)���� ���� ID, (INT64)���� Account ��ȣ, (char*)���� Ű
// return: (bool)���� ���� ����
////////////////////////////////////////////////////////////////////////
bool ChattingServer::AuthenticateUser(ULONG64 udlSessionID, INT64 AccountNo, char* SessionKey)
{
	bool bResult = false;
	char chAccount[17] = { 0, };

	// int64 to Ascii
	_i64toa_s(AccountNo, chAccount, sizeof(chAccount), 16);

	_RedisClient.get(chAccount, [&](cpp_redis::reply& reply) {
		char key[65];

		memcpy(key, SessionKey, sizeof(key) - 1);
		key[64] = 0;

		if (reply.as_string() == key)
			bResult = true;
		});
	_RedisClient.sync_commit();

	return bResult;
}

////////////////////////////////////////////////////////////////////////
// �Էµ� ���� ��ǥ�� �������� �ֺ� ���� ���ϱ�
// 
// Parameter: (int)���� ���� X, (int)���� ���� Y, (st_SECTOR_AROUND*)�ֺ� ���� ������ ������ �ּ�
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
	int iCntX, iCntY;

	//pSectorAround->Count = 0;
	//iSectorX--;
	//iSectorY--;

	for (iCntY = -1; iCntY < 2; iCntY++)
	{
		if (iSectorY + iCntY > 50)
			break;

		if (iSectorY + iCntY < 0)
			continue;

		for (iCntX = -1; iCntX < 2; iCntX++)
		{
			if (iSectorX + iCntX > 50)
				break;

			if (iSectorX + iCntX < 0)
				continue;

			pSectorAround->Around[pSectorAround->Count].iX = iSectorX + iCntX;
			pSectorAround->Around[pSectorAround->Count].iY = iSectorY + iCntY;
			pSectorAround->Count++;
		}
	}
}

////////////////////////////////////////////////////////////////////////
// �α��� ���� �޽��� ����
// 
// Parameter: (CPacket*)���� ������ ���� ����ȭ����, (INT64)���� ��� Account ��ȣ, (BYTE)���� ���� ����
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status)
{
	WORD wType = en_PACKET_CS_CHAT_RES_LOGIN;

	*pSendPacket << wType;
	*pSendPacket << Status;
	*pSendPacket << AccountNo;
}

////////////////////////////////////////////////////////////////////////
// ���� �̵� ���� �޽��� ����
// 
// Parameter: (CPacket*)���� ������ ���� ����ȭ����, (INT64)���� ��� Account ��ȣ, (WORD)���� X, (WORD)���� Y
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::mpMoveSectorRes(CPacket* pSendPacket, INT64 AccountNo, WORD SectorX, WORD SectorY)
{
	// ��Ŷ ����
	WORD wType = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;

	*pSendPacket << wType;
	*pSendPacket << AccountNo;
	*pSendPacket << SectorX;
	*pSendPacket << SectorY;
}

////////////////////////////////////////////////////////////////////////
// ä�� ���� �޽��� ����
// 
// Parameter: (CPacket*)���� ������ ���� ����ȭ����, (INT64)���� ��� Account ��ȣ, (WCHAR*)ä�� ���� ID, (WCHAR*)ä�� ���� �г���,
// (WORD)ä�� �޽��� ����(BYTE ũ��), (WCHAR*)ä�� �޽���
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::mpChatting(CPacket* pSendPacket, INT64 AccountNo, WCHAR* ID, WCHAR* Nickname, WORD MessageLen, WCHAR* Message)
{
	// ��Ŷ ����
	WORD wType = en_PACKET_CS_CHAT_RES_MESSAGE;

	*pSendPacket << wType;
	*pSendPacket << AccountNo;
	pSendPacket->PutData((char*)ID, 20 * sizeof(WCHAR));
	pSendPacket->PutData((char*)Nickname, 20 * sizeof(WCHAR));
	*pSendPacket << MessageLen;
	pSendPacket->PutData((char*)Message, MessageLen);
}

////////////////////////////////////////////////////////////////////////
// ä�� ���� ����͸�
// 
// Parameter: ����
// void: ����
////////////////////////////////////////////////////////////////////////
void ChattingServer::MonitoringOutput(void)
{
	st_MonitoringInfo mi;
	time_t timer = time(NULL);
	struct tm t;

	/* ����� ������Ʈ */

	(void)localtime_s(&t, &timer);
	GetMonitoringInfo(&mi);

	// ����� ���� ������Ʈ
	_dlNowSessionNum = mi.NowSessionNum;

	WCHAR *wchMonitorBuffer = new WCHAR[10240];
	int iBufSize = 10240;

	//---------------------------------------------------------------
	// ����͸� ���� �ܼ� ���
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s", L"==========================MONITORING==========================\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L" Press Key u or U to diplay Control Information\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s Start Time: [%d/%d/%d %2d:%2d:%2d]\n", wchMonitorBuffer, _StartTime.tm_year + 1900, _StartTime.tm_mon + 1, _StartTime.tm_mday, _StartTime.tm_hour,
		_StartTime.tm_min, _StartTime.tm_sec);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s Now Time  : [%d/%d/%d %2d:%2d:%2d]\n", wchMonitorBuffer, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s Core Number       : %d\n", wchMonitorBuffer, _dwCoreNum);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s Network IO Worker : %d\n\n", wchMonitorBuffer, _dwWorkerThreadNum);


	//---------------------------------------------------------------
	// Total
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"TOTAL::\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Accept Total                - %lld\n", wchMonitorBuffer, mi.AcceptTotal);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Diconnect Total             - %lld\n", wchMonitorBuffer, mi.DisconnetTotal);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Proc MSG                    - %lld\n", wchMonitorBuffer, _MonitorData.Total_ProcMSG);


	//---------------------------------------------------------------
	// TPS
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"TPS:: NOW / MAX / MIN\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Accept TPS                  - %4lld / %4lld / %4lld\n", wchMonitorBuffer, mi.AcceptTPS, mi.AcceptTPSMax, mi.AcceptTPSMin);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Disconnect TPS              - %4lld / %4lld / %4lld\n", wchMonitorBuffer, mi.DisconnectTPS, mi.DisconnectTPSMax, mi.DisconnectTPSMin);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Proc MSG                    - %4lld / %4lld / %4lld\n", wchMonitorBuffer,
		_MonitorData.TPS_ProcMSGMonitor, _MonitorData.TPSMAX_ProcMSG, _MonitorData.TPSMIN_ProcMSG);


	//---------------------------------------------------------------
	// Connection
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"CONNECTION::\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Session Count               - %lld\n", wchMonitorBuffer, mi.NowSessionNum);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Player Count                - %lld\n", wchMonitorBuffer, _dlNowLoginUserNum);


	//---------------------------------------------------------------
	// Pool
	//---------------------------------------------------------------
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"POOL:: USE / TOTAL\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Packet Pool                 - %4lld / %4lld\n", wchMonitorBuffer, CPacket::GetCountOfPoolAlloc(), CPacket::GetCountOfPoolTotalAlloc());
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s   Player Pool                 - %4lld / %4lld\n", wchMonitorBuffer, st_USER_INFO::GetCountOfPoolAlloc(), st_USER_INFO::GetCountOfPoolTotalAlloc());

	wprintf(L"%s", wchMonitorBuffer);

	delete [] wchMonitorBuffer;

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
void ChattingServer::ServerControl(void)
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