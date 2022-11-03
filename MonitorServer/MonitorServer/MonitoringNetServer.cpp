#include <windows.h>
#include <strsafe.h>
#include <time.h>
#include <process.h>
#include <conio.h>
#include <unordered_set>
#include <unordered_map>
#include <strsafe.h>
#include <crtdbg.h>
#include <DbgHelp.h>
#include <Pdh.h>
#include "MonitoringNetServer.h"
#include "TextParset_Unicode.h"
#include "SystemLog.h"
#include "CCrashDump.h"
#include "CommonProtocol.h"
#include "CDBConnector.h"

#pragma comment(lib, "Winmm.lib")

using namespace std;

MonitoringNetServer::MonitoringNetServer() :
	CNetServer()
{
	// DB ����
	_DBWriter.Connect(L"10.0.2.2", L"KJS", L"955955", L"logdb", 3306);
	
		 
	// Config ���Ͽ��� ���� �� �ҷ�����
	CParserUnicode parser;

	if (false == parser.LoadFile(L"MonitoringServer.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");
	
	wprintf(L"------------------------------------------------------------------------\n");
	wprintf(L"---------------------MONITOR NET SERVER CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen ���� ���ε� IP
	if (parser.GetString(L"NET_BIND_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen ���� ��Ʈ
	if (parser.GetValue(L"NET_BIND_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP ��Ŀ ������ ����
	if (parser.GetValue(L"NET_IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP ���� ���� ������ ����
	if (parser.GetValue(L"NET_IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	DWORD dwMaxSession;			// �ִ� ����
	if (parser.GetValue(L"NET_SESSION_MAX", (int*)&dwMaxSession))
		wprintf(L"-----SESSION_MAX: %d\n", dwMaxSession);

	int iPacketCode;
	if (parser.GetValue(L"NET_PACKET_CODE", (int*)&iPacketCode))
		wprintf(L"-----PACKET_CODE: %d\n", iPacketCode);

	int iPacketKey;
	if (parser.GetValue(L"NET_PACKET_KEY", (int*)&iPacketKey))
		wprintf(L"-----PACKET_KEY: %d\n", iPacketKey);

	wchar_t wchLogLevel[50] = { 0, };
	if (parser.GetString(L"NET_LOG_LEVEL", wchLogLevel))
		wprintf(L"-----LOG_LEVEL: %s\n", wchLogLevel);

	wprintf(L"------------------------------------------------------------------\n");

	// Ÿ�Ӿƿ�
	_hTimeCheckThread = (HANDLE)_beginthreadex(NULL, 0, Proc_TimeCheck, (void*)this, 0, (unsigned int*)&_dwTimeCheckThreadsID);
	if (_hTimeCheckThread== 0 || _hTimeCheckThread== (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex error\n");
		int* pDown = NULL;
		*pDown = 0;
	}

	// DB ����
	_hDBWritehThread = (HANDLE)_beginthreadex(NULL, 0, DBWriterThread, (void*)this, 0, (unsigned int*)&_dwDBWriteThreadsID);
	if (_hDBWritehThread == 0 || _hDBWritehThread == (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex Error Code: %d\n", GetLastError());
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
	time_t timer = time(NULL);
	struct tm t;

	(void)localtime_s(&t, &timer);
	swprintf_s(_LogFileName, _MAX_PATH, L"Monitor Net Server Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// User �� ����ȭ ��ü �ʱ�ȭ
	InitializeSRWLock(&_UserMapLock);

	_hDBWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == _hDBWriteEvent)
	{
		wprintf(L"CreateEvent Error Code: %d\n", GetLastError());
		int* pDown = NULL;
		*pDown = 0;
	}

	// ����͸� ���� ���� �ʱ�ȭ
	// ���μ���
	_ProcessorMonitorCollector[en_PRO_CPU_SHARE].Type = dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL;
	_ProcessorMonitorCollector[en_PRO_NON_PAGED_MEMORY].Type = dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY;
	_ProcessorMonitorCollector[en_PRO_NET_RECV].Type = dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV;
	_ProcessorMonitorCollector[en_PRO_NET_SEND].Type = dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND;
	_ProcessorMonitorCollector[en_PRO_CPU_AVA_MEMORY].Type = dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY;

	// ����
	_ServerMonitorCollector[en_SRV_CLT_CPU_SHARE].Type = dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU;
	_ServerMonitorCollector[en_SRV_CLT_MEMORY].Type = dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM;
	_ServerMonitorCollector[en_SRV_CLT_SESSION_COUNT].Type = dfMONITOR_DATA_TYPE_CHAT_SESSION;
	_ServerMonitorCollector[en_SRV_CLT_PLAYER_COUNT].Type = dfMONITOR_DATA_TYPE_CHAT_PLAYER;
	_ServerMonitorCollector[en_SRV_CLT_MSG_TPS].Type = dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS;
	_ServerMonitorCollector[en_SRV_CLT_PACKET_POOL].Type = dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL;

	// �ּҰ� �ʱ�ȭ
	for (int cnt = en_PRO_CPU_SHARE; cnt < en_PRO_TOTAL; cnt++)
	{
		_ProcessorMonitorCollector[cnt].Min = 0xFFFFFFFFFFFF;
	}

	for (int cnt = en_SRV_CLT_CPU_SHARE; cnt < en_SRV_CLT_TOTAL; cnt++)
	{
		_ServerMonitorCollector[cnt].Min = 0xFFFFFFFFFFFF;
	}

	// NetServer ����
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, iPacketCode, iPacketKey, false);
}

MonitoringNetServer::~MonitoringNetServer()
{

}

////////////////////////////////////////////////////////////////////////
// ���� ��� ���� �˻�. �̰����� ȭ��Ʈ ����Ʈ ����
// 
// Parameter: (WCHAR*)����� IP, (DWORD)��Ʈ ��ȣ
// return: ����
////////////////////////////////////////////////////////////////////////
bool MonitoringNetServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
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
void MonitoringNetServer::OnClientJoin(ULONG64 udlSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::OnClientLeave(ULONG64 udlSessionID)
{
	//--------------------------------
	// �����ʿ��� �α׾ƿ� ���� ����
	//--------------------------------
	AcquireSRWLockExclusive(&_UserMapLock);
		
	DeleteUser(udlSessionID);

	ReleaseSRWLockExclusive(&_UserMapLock);
}

////////////////////////////////////////////////////////////////////////
// ��û ��Ŷ ���� �ݹ� �Լ�
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ���� ����ȭ����
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::OnRecv(ULONG64 udlSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// �޽��� �Ľ� 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// �޽��� Ÿ�Կ� ���� ���ν��� ȣ��
	switch (wType)
	{
	//case en_PACKET_TYPE::en_PACKET_SS_MONITOR_DATA_UPDATE:		// ������ �����κ��� ����� ���� ����
		//PacketProc_UpdateServer(pPacket);
		//break;
	case en_PACKET_TYPE::en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:	// ����� Ŭ���̾�Ʈ ����
		PacketProc_LoginMonitorClient(udlSessionID, pPacket);
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
void MonitoringNetServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: ���� �����忡 � ���·� ������ �� ����غ���
	//if (0 != errorcode)
		//LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// ���� Ÿ�Ӿƿ� �˻� ���ν���
// 
// Parameter: (void*)MonitoringNetServer ��ü �ּ�
// return: 
////////////////////////////////////////////////////////////////////////
unsigned int __stdcall MonitoringNetServer::Proc_TimeCheck(void* pvParam)
{
	MonitoringNetServer* pServer = (MonitoringNetServer*)pvParam;

	pServer->_udlMonitorTimeTick = timeGetTime();
	pServer->_udlDBWriteTimeTick = pServer->_udlMonitorTimeTick;

	ULONG64 udlPrevTick = pServer->_udlMonitorTimeTick;
	int iTimeDec;

	while (1)
	{
		ULONG64 udlNowTick = timeGetTime();

		if (abs((long long)(udlNowTick - pServer->_udlMonitorTimeTick)) > en_USER_TIME_OUT_PERIOD)
		{
			pServer->_udlMonitorTimeTick = udlNowTick - (udlNowTick - pServer->_udlMonitorTimeTick - en_USER_TIME_OUT_PERIOD);
			
			// ���μ��� ����͸� ���� ����

			int iTimeStamp = (int)time(NULL);

			pServer->_ProcessorMonitor.UpdateMonitorInfo();

			//----------------------------------------------------------------------
			// DB ���� ���� ����͸� ���� ����
			//----------------------------------------------------------------------
			pServer->CollectMonitorInfo();

			// ������ǻ�� CPU ��ü ����
			CPacket* pPacketCPUTotal = CPacket::Alloc();
			pServer->mpUpdateData(pPacketCPUTotal, 2, dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, (int)(pServer->_ProcessorMonitor.ProcessorTotal()), iTimeStamp);

			// ������ǻ�� �������� �޸� MByte
			CPacket* pPakcetNONPaged = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetNONPaged, 2, dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, (int)(pServer->_ProcessorMonitor.ProcessorNonPagedMemory() / dfNUMBER_MEGA), iTimeStamp);

			// ������ǻ�� ��Ʈ��ũ ���ŷ� KByte
			CPacket* pPakcetNetRecv = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetNetRecv, 2, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, (int)(pServer->_ProcessorMonitor.NetworkRecvTraffic() / dfNUMBER_KILLO), iTimeStamp);

			// ������ǻ�� ��Ʈ��ũ �۽ŷ� KByte
			CPacket* pPakcetNetSend = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetNetSend, 2, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, (int)(pServer->_ProcessorMonitor.NetworkSendTraffic() / dfNUMBER_KILLO), iTimeStamp);

			// ������ǻ�� ��밡�� �޸�
			CPacket* pPakcetAvailMem = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetAvailMem, 2, dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, (int)(pServer->_ProcessorMonitor.ProcessorAvailableMemory()), iTimeStamp);

			// Ŭ���̾�Ʈ�� ���� ���� ����
			ULONG64 udlSessionArr[en_MAX_MONITOR_USER];
			ULONG64 udlSessionCount = 0;

			AcquireSRWLockShared(&pServer->_UserMapLock);

			std::list<ULONG64>::iterator iter;

			for (iter = pServer->_UserMap.begin(); iter != pServer->_UserMap.end(); ++iter)
			{
				udlSessionArr[udlSessionCount++] = *iter;

				if (udlSessionCount >= en_MAX_MONITOR_USER)
					break;
			}

			ReleaseSRWLockShared(&pServer->_UserMapLock);

			//------------------------------------
			// ����͸� ���� ����
			//------------------------------------
			for (int cnt = 0; cnt < udlSessionCount; ++cnt)
			{
				pServer->SendPacket(udlSessionArr[cnt], pPacketCPUTotal);
				pServer->SendPacket(udlSessionArr[cnt], pPakcetNONPaged);
				pServer->SendPacket(udlSessionArr[cnt], pPakcetNetRecv);
				pServer->SendPacket(udlSessionArr[cnt], pPakcetNetSend);
				pServer->SendPacket(udlSessionArr[cnt], pPakcetAvailMem);
			}

			CPacket::Free(pPacketCPUTotal);
			CPacket::Free(pPakcetNONPaged);
			CPacket::Free(pPakcetNetRecv);
			CPacket::Free(pPakcetNetSend);
			CPacket::Free(pPakcetAvailMem);
		}
		
		// 5�� ���� DB�� �α�
		//udlNowTick = timeGetTime();

		if ((abs((long long)(udlNowTick - pServer->_udlDBWriteTimeTick)) > en_MONITOR_COLLECT_TIME_PERIOD) && (NULL != pServer->_DBWriter.GetConnctionStatus()))
		{
			pServer->_udlDBWriteTimeTick = udlNowTick - (udlNowTick - pServer->_udlDBWriteTimeTick - en_MONITOR_COLLECT_TIME_PERIOD);

			// ���μ��� ����� �÷��� ��
			for (int num = en_PRO_CPU_SHARE; num < en_PRO_TOTAL; num++)
			{
				CDBServerMonitorJob* pJob = new CDBServerMonitorJob;
				pJob->_iServerNo = 0;
				pJob->_iType = pServer->_ProcessorMonitorCollector[num].Type;
				pJob->_iValueMax = (int)(pServer->_ProcessorMonitorCollector[num].Max);
				pJob->_iValueMin = (int)(pServer->_ProcessorMonitorCollector[num].Min);

				// ��� ���
				if (pServer->_ProcessorMonitorCollector[num].Count > 0)
					pJob->_iValueAvr = (int)(pServer->_ProcessorMonitorCollector[num].Total / pServer->_ProcessorMonitorCollector[num].Count);
				else
					pJob->_iValueAvr = 0;

				pServer->_DBJobQ.Enqueue(pJob);
			}

			// �� ���� ����� �÷��� ��
			for (int num = en_SRV_CLT_CPU_SHARE; num < en_SRV_CLT_TOTAL; num++)
			{
				CDBServerMonitorJob* pJob = new CDBServerMonitorJob;
				pJob->_iServerNo = 0;
				pJob->_iType = pServer->_ServerMonitorCollector[num].Type;
				pJob->_iValueMax = (int)(pServer->_ServerMonitorCollector[num].Max);
				pJob->_iValueMin = (int)(pServer->_ServerMonitorCollector[num].Min);

				// ��� ���
				if (pServer->_ServerMonitorCollector[num].Count > 0)
					pJob->_iValueAvr = (int)(pServer->_ServerMonitorCollector[num].Total / pServer->_ServerMonitorCollector[num].Count);
				else
					pJob->_iValueAvr = 0;

				pServer->_DBJobQ.Enqueue(pJob);
			}

			SetEvent(pServer->_hDBWriteEvent);
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
// ���μ��� ����͸� ���� ����
// 
// Parameter: ����
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::CollectMonitorInfo(void)
{
	for (int num = en_PRO_CPU_SHARE; num < en_PRO_TOTAL; num++)
	{
		double dblMonitorVal;
		switch (num)
		{
		case en_PRO_CPU_SHARE:
			dblMonitorVal = _ProcessorMonitor.ProcessorTotal();
			break;
		case en_PRO_NON_PAGED_MEMORY:
			dblMonitorVal = _ProcessorMonitor.ProcessorNonPagedMemory() / dfNUMBER_MEGA;
			break;
		case en_PRO_NET_SEND:
			dblMonitorVal = _ProcessorMonitor.NetworkSendTraffic() / dfNUMBER_KILLO;
			break;
		case en_PRO_NET_RECV:
			dblMonitorVal = _ProcessorMonitor.NetworkRecvTraffic() / dfNUMBER_KILLO;
			break;
		case en_PRO_CPU_AVA_MEMORY:
			dblMonitorVal = _ProcessorMonitor.ProcessorAvailableMemory();
			break;
		default:
			return;
		}
		_ProcessorMonitorCollector[num].Count++;
		_ProcessorMonitorCollector[num].Total += (LONG64)(dblMonitorVal);

		if ((LONG64)(dblMonitorVal) > _ProcessorMonitorCollector[num].Max)
			_ProcessorMonitorCollector[num].Max = (LONG64)(dblMonitorVal);
		else if ((LONG64)(dblMonitorVal) < _ProcessorMonitorCollector[num].Min)
			_ProcessorMonitorCollector[num].Min = (LONG64)(dblMonitorVal);
	}
}

////////////////////////////////////////////////////////////////////////
// DB ���� ������
// 
// Parameter: (void*)MonitoringNetServer ��ü �ּ�
// return: 
////////////////////////////////////////////////////////////////////////
unsigned int __stdcall MonitoringNetServer::DBWriterThread(void* pvParam)
{
	MonitoringNetServer* pServer = (MonitoringNetServer*)pvParam;
	IDBJob* pJob;

	while (1)
	{
		if (0 == pServer->_DBJobQ.Dequeue(pJob))
		{
			WaitForSingleObject(pServer->_hDBWriteEvent, INFINITE);
			continue;
		}

		pJob->Exec(&pServer->_DBWriter);

		delete pJob;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
// �� ���� ����� ���� ��Ŷ ó��
// 
// Parameter: (CPacket*) ��Ŷ ����ȭ����
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::PacketProc_UpdateServer(CPacket* pPacket)
{
	BYTE	byDataType;				// ����͸� ������ Type �ϴ� Define ��.
	int		iDataValue;				// �ش� ������ ��ġ.
	int		iTimeStamp;

	*pPacket >> byDataType;
	*pPacket >> iDataValue;
	*pPacket >> iTimeStamp;

	//------------------------------------
	// ����͸� ���� ����
	//------------------------------------
	bool bMonitorCollect = true;
	int iCollectNum;

	switch (byDataType)
	{
	case dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU:
		iCollectNum = en_SRV_CLT_CPU_SHARE;
		break;
	case dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM:
		iCollectNum = en_SRV_CLT_MEMORY;
		break;
	case dfMONITOR_DATA_TYPE_CHAT_SESSION:
		iCollectNum = en_SRV_CLT_SESSION_COUNT;
		break;
	case dfMONITOR_DATA_TYPE_CHAT_PLAYER:
		iCollectNum = en_SRV_CLT_PLAYER_COUNT;
		break;
	case dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS:
		iCollectNum = en_SRV_CLT_MSG_TPS;
		break;
	case dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL:
		iCollectNum = en_SRV_CLT_PACKET_POOL;
		break;
	default:
		bMonitorCollect = false;
	}
	
	if (true == bMonitorCollect)
	{
		InterlockedIncrement64(&_ServerMonitorCollector[iCollectNum].Count);
		InterlockedAdd64(&_ServerMonitorCollector[iCollectNum].Total, iDataValue);

		if (iDataValue > _ServerMonitorCollector[iCollectNum].Max)
			InterlockedExchange64(&_ServerMonitorCollector[iCollectNum].Max, iDataValue);
		else if (iDataValue < _ServerMonitorCollector[iCollectNum].Min)
			InterlockedExchange64(&_ServerMonitorCollector[iCollectNum].Min, iDataValue);
	}

	//------------------------------------
	// ����͸� ���� ����
	//------------------------------------
	CPacket* pSendPacket = CPacket::Alloc();
	mpUpdateData(pSendPacket, 0, byDataType, iDataValue, iTimeStamp);

	//------------------------------------
	// ����� ���� ���� ���� ���� ID �˻�
	//------------------------------------
	ULONG64 udlSessionArr[en_MAX_MONITOR_USER];
	ULONG64 udlSessionCount = 0;

	AcquireSRWLockShared(&_UserMapLock);

	std::list<ULONG64>::iterator iter;

	for (iter = _UserMap.begin(); iter != _UserMap.end(); ++iter)
	{
		udlSessionArr[udlSessionCount++] = *iter;

		if (udlSessionCount >= en_MAX_MONITOR_USER)
			break;
	}

	ReleaseSRWLockShared(&_UserMapLock);

	//------------------------------------
	// ����͸� ���� ����
	//------------------------------------
	for (int cnt = 0; cnt < udlSessionCount; ++cnt)
	{
		SendPacket(udlSessionArr[cnt], pSendPacket);
	}
	CPacket::Free(pSendPacket);
}

////////////////////////////////////////////////////////////////////////
// ä�� ��û ��Ŷ ó��
// 
// Parameter: (ULONG64)���� ���� ID, (CPacket*)��û ��Ŷ ����ȭ����
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::PacketProc_LoginMonitorClient(ULONG64 udlSessionID, CPacket* pPacket)
{
	//------------------------------------
	// ���� ���� ���� �� ä�� ���� �޽��� ����
	//------------------------------------
	
	BYTE byStatus;
	if (0 == strncmp("ajfw@!cv980dSZ[fje#@fdj123948djf", pPacket->GetBufferReadPtr(), pPacket->GetDataSize()))
	{
		byStatus = dfMONITOR_TOOL_LOGIN_OK;

		AcquireSRWLockExclusive(&_UserMapLock);
		AddUser(udlSessionID);
		ReleaseSRWLockExclusive(&_UserMapLock);
	}
	else
	{
		byStatus = dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY;
	}

	//------------------------------------
	// �α��� ���� �޽��� ����
	//------------------------------------
	CPacket* pSendPakcet = CPacket::Alloc();

	mpLoginRes(pSendPakcet, byStatus);
	SendPacket(udlSessionID, pSendPakcet);

	CPacket::Free(pSendPakcet);

	if (dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY == byStatus)
		Disconnect(udlSessionID);

	return;
}

////////////////////////////////////////////////////////////////////////
// ���� �ʿ� ���� �߰�
// 
// Parameter: (ULONG64)���� ���� ID, (st_USER_INFO*)���� ���� ������
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::AddUser(ULONG64 udlSessionID)
{
	_UserMap.push_back(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// ���� �ʿ��� ���� ã��
// 
// Parameter: (ULONG64)���� ���� ID
// return: ���� ���� ����ü �ּ�
////////////////////////////////////////////////////////////////////////
bool MonitoringNetServer::FindUser(ULONG64 udlSessionID)
{
	std::list<ULONG64>::iterator iter;
	for (iter = _UserMap.begin(); iter != _UserMap.end(); ++iter)
	{
		if (udlSessionID == *iter)
			return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////
// ���� �ʿ��� ���� ����
// 
// Parameter: (ULONG64)���� ���� ID
// return: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::DeleteUser(ULONG64 udlSessionID)
{
	std::list<ULONG64>::iterator iter;
	for (iter = _UserMap.begin(); iter != _UserMap.end(); iter)
	{
		if (udlSessionID == *iter)
		{
			iter = _UserMap.erase(iter);
		}
		else
		{
			++iter;
		}
	}

}

////////////////////////////////////////////////////////////////////////
// �α��� ���� �޽��� ����
// 
// Parameter: (CPacket*)���� ������ ���� ����ȭ����, (INT64)���� ��� Account ��ȣ, (BYTE)���� ���� ����
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::mpLoginRes(CPacket* pSendPacket, BYTE Status)
{
	WORD wType = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN;

	*pSendPacket << wType;
	*pSendPacket << Status;
}

////////////////////////////////////////////////////////////////////////
// ���� �̵� ���� �޽��� ����
// 
// Parameter: (CPacket*)���� ������ ���� ����ȭ����, (INT64)���� ��� Account ��ȣ, (WORD)���� X, (WORD)���� Y
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::mpUpdateData(CPacket* pSendPacket, BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp)
{
	// ��Ŷ ����
	WORD wType = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;

	*pSendPacket << wType;
	*pSendPacket << ServerNo;
	*pSendPacket << DataType;
	*pSendPacket << DataValue;
	*pSendPacket << TimeStamp;
}

////////////////////////////////////////////////////////////////////////
// ä�� ���� ����͸�
// 
// Parameter: ����
// void: ����
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::MonitoringOutput(void)
{
	st_MonitoringInfo mi;
	time_t timer = time(NULL);
	struct tm t;

	(void)localtime_s(&t, &timer);

	WCHAR wchMonitorBuffer[4096] = { 0, };
	int iBufSize = 4096;

	GetMonitoringInfo(&mi);
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L"==========================MONITOR SERVER==========================\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s %s", wchMonitorBuffer, L" Press Key u or U to diplay Control Information\n");
	swprintf_s(wchMonitorBuffer, iBufSize, L"%s [%d/%d/%d %2d:%2d:%2d]\n\n", wchMonitorBuffer, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

	wprintf(L"%s", wchMonitorBuffer);

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
void MonitoringNetServer::ServerControl(void)
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