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
	// DB 연동
	_DBWriter.Connect(L"10.0.2.2", L"KJS", L"955955", L"logdb", 3306);
	
		 
	// Config 파일에서 설정 값 불러오기
	CParserUnicode parser;

	if (false == parser.LoadFile(L"MonitoringServer.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");
	
	wprintf(L"------------------------------------------------------------------------\n");
	wprintf(L"---------------------MONITOR NET SERVER CONFIG DATA---------------------\n");

	wchar_t wchIP[50] = { 0, };	// Listen 소켓 바인드 IP
	if (parser.GetString(L"NET_BIND_IP", wchIP))
		wprintf(L"-----IP: % s\n", wchIP);

	DWORD dwPort;				// Listen 소켓 포트
	if (parser.GetValue(L"NET_BIND_PORT", (int*)&dwPort))
		wprintf(L"-----Port: %d\n", dwPort);

	DWORD dwWorkderThreadNum;	// IOCP 워커 스레드 개수
	if (parser.GetValue(L"NET_IOCP_WORKER_THREAD", (int*)&dwWorkderThreadNum))
		wprintf(L"-----IOCP_WORKER_THREAD: %d\n", dwWorkderThreadNum);

	DWORD dwActiveThreadNum;	// IOCP 동시 러닝 스레드 개수
	if (parser.GetValue(L"NET_IOCP_ACTIVE_THREAD", (int*)&dwActiveThreadNum))
		wprintf(L"-----IOCP_ACTIVE_THREAD: %d\n", dwActiveThreadNum);

	DWORD dwMaxSession;			// 최대 세션
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

	// 타임아웃
	_hTimeCheckThread = (HANDLE)_beginthreadex(NULL, 0, Proc_TimeCheck, (void*)this, 0, (unsigned int*)&_dwTimeCheckThreadsID);
	if (_hTimeCheckThread== 0 || _hTimeCheckThread== (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex error\n");
		int* pDown = NULL;
		*pDown = 0;
	}

	// DB 쓰기
	_hDBWritehThread = (HANDLE)_beginthreadex(NULL, 0, DBWriterThread, (void*)this, 0, (unsigned int*)&_dwDBWriteThreadsID);
	if (_hDBWritehThread == 0 || _hDBWritehThread == (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex Error Code: %d\n", GetLastError());
		int* pDown = NULL;
		*pDown = 0;
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
	swprintf_s(_LogFileName, _MAX_PATH, L"Monitor Net Server Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// User 맵 동기화 객체 초기화
	InitializeSRWLock(&_UserMapLock);

	_hDBWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == _hDBWriteEvent)
	{
		wprintf(L"CreateEvent Error Code: %d\n", GetLastError());
		int* pDown = NULL;
		*pDown = 0;
	}

	// 모니터링 수집 정보 초기화
	// 프로세서
	_ProcessorMonitorCollector[en_PRO_CPU_SHARE].Type = dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL;
	_ProcessorMonitorCollector[en_PRO_NON_PAGED_MEMORY].Type = dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY;
	_ProcessorMonitorCollector[en_PRO_NET_RECV].Type = dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV;
	_ProcessorMonitorCollector[en_PRO_NET_SEND].Type = dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND;
	_ProcessorMonitorCollector[en_PRO_CPU_AVA_MEMORY].Type = dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY;

	// 서버
	_ServerMonitorCollector[en_SRV_CLT_CPU_SHARE].Type = dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU;
	_ServerMonitorCollector[en_SRV_CLT_MEMORY].Type = dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM;
	_ServerMonitorCollector[en_SRV_CLT_SESSION_COUNT].Type = dfMONITOR_DATA_TYPE_CHAT_SESSION;
	_ServerMonitorCollector[en_SRV_CLT_PLAYER_COUNT].Type = dfMONITOR_DATA_TYPE_CHAT_PLAYER;
	_ServerMonitorCollector[en_SRV_CLT_MSG_TPS].Type = dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS;
	_ServerMonitorCollector[en_SRV_CLT_PACKET_POOL].Type = dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL;

	// 최소값 초기화
	for (int cnt = en_PRO_CPU_SHARE; cnt < en_PRO_TOTAL; cnt++)
	{
		_ProcessorMonitorCollector[cnt].Min = 0xFFFFFFFFFFFF;
	}

	for (int cnt = en_SRV_CLT_CPU_SHARE; cnt < en_SRV_CLT_TOTAL; cnt++)
	{
		_ServerMonitorCollector[cnt].Min = 0xFFFFFFFFFFFF;
	}

	// NetServer 가동
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, iPacketCode, iPacketKey, false);
}

MonitoringNetServer::~MonitoringNetServer()
{

}

////////////////////////////////////////////////////////////////////////
// 연결 허용 여부 검사. 이곳에서 화이트 리스트 설정
// 
// Parameter: (WCHAR*)연결된 IP, (DWORD)포트 번호
// return: 없음
////////////////////////////////////////////////////////////////////////
bool MonitoringNetServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
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
void MonitoringNetServer::OnClientJoin(ULONG64 udlSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// 유저 접속 종료 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID
// void: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::OnClientLeave(ULONG64 udlSessionID)
{
	//--------------------------------
	// 유저맵에서 로그아웃 유저 삭제
	//--------------------------------
	AcquireSRWLockExclusive(&_UserMapLock);
		
	DeleteUser(udlSessionID);

	ReleaseSRWLockExclusive(&_UserMapLock);
}

////////////////////////////////////////////////////////////////////////
// 요청 패킷 수신 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 저장 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::OnRecv(ULONG64 udlSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// 메시지 파싱 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// 메시지 타입에 따른 프로시저 호출
	switch (wType)
	{
	//case en_PACKET_TYPE::en_PACKET_SS_MONITOR_DATA_UPDATE:		// 컨텐츠 서버로부터 모니터 정보 수신
		//PacketProc_UpdateServer(pPacket);
		//break;
	case en_PACKET_TYPE::en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:	// 모니터 클라이언트 접속
		PacketProc_LoginMonitorClient(udlSessionID, pPacket);
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
void MonitoringNetServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: 로직 스레드에 어떤 형태로 전달할 지 고민해보기
	//if (0 != errorcode)
		//LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// 유저 타임아웃 검사 프로시저
// 
// Parameter: (void*)MonitoringNetServer 객체 주소
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
			
			// 프로세서 모니터링 정보 전송

			int iTimeStamp = (int)time(NULL);

			pServer->_ProcessorMonitor.UpdateMonitorInfo();

			//----------------------------------------------------------------------
			// DB 저장 서버 모니터링 정보 수집
			//----------------------------------------------------------------------
			pServer->CollectMonitorInfo();

			// 서버컴퓨터 CPU 전체 사용률
			CPacket* pPacketCPUTotal = CPacket::Alloc();
			pServer->mpUpdateData(pPacketCPUTotal, 2, dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, (int)(pServer->_ProcessorMonitor.ProcessorTotal()), iTimeStamp);

			// 서버컴퓨터 논페이지 메모리 MByte
			CPacket* pPakcetNONPaged = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetNONPaged, 2, dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, (int)(pServer->_ProcessorMonitor.ProcessorNonPagedMemory() / dfNUMBER_MEGA), iTimeStamp);

			// 서버컴퓨터 네트워크 수신량 KByte
			CPacket* pPakcetNetRecv = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetNetRecv, 2, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, (int)(pServer->_ProcessorMonitor.NetworkRecvTraffic() / dfNUMBER_KILLO), iTimeStamp);

			// 서버컴퓨터 네트워크 송신량 KByte
			CPacket* pPakcetNetSend = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetNetSend, 2, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, (int)(pServer->_ProcessorMonitor.NetworkSendTraffic() / dfNUMBER_KILLO), iTimeStamp);

			// 서버컴퓨터 사용가능 메모리
			CPacket* pPakcetAvailMem = CPacket::Alloc();
			pServer->mpUpdateData(pPakcetAvailMem, 2, dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, (int)(pServer->_ProcessorMonitor.ProcessorAvailableMemory()), iTimeStamp);

			// 클라이언트에 유저 정보 전송
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
			// 모니터링 정보 전송
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
		
		// 5분 마다 DB에 로깅
		//udlNowTick = timeGetTime();

		if ((abs((long long)(udlNowTick - pServer->_udlDBWriteTimeTick)) > en_MONITOR_COLLECT_TIME_PERIOD) && (NULL != pServer->_DBWriter.GetConnctionStatus()))
		{
			pServer->_udlDBWriteTimeTick = udlNowTick - (udlNowTick - pServer->_udlDBWriteTimeTick - en_MONITOR_COLLECT_TIME_PERIOD);

			// 프로세서 모니터 컬렉터 잡
			for (int num = en_PRO_CPU_SHARE; num < en_PRO_TOTAL; num++)
			{
				CDBServerMonitorJob* pJob = new CDBServerMonitorJob;
				pJob->_iServerNo = 0;
				pJob->_iType = pServer->_ProcessorMonitorCollector[num].Type;
				pJob->_iValueMax = (int)(pServer->_ProcessorMonitorCollector[num].Max);
				pJob->_iValueMin = (int)(pServer->_ProcessorMonitorCollector[num].Min);

				// 평균 계산
				if (pServer->_ProcessorMonitorCollector[num].Count > 0)
					pJob->_iValueAvr = (int)(pServer->_ProcessorMonitorCollector[num].Total / pServer->_ProcessorMonitorCollector[num].Count);
				else
					pJob->_iValueAvr = 0;

				pServer->_DBJobQ.Enqueue(pJob);
			}

			// 각 서버 모니터 컬렉터 잡
			for (int num = en_SRV_CLT_CPU_SHARE; num < en_SRV_CLT_TOTAL; num++)
			{
				CDBServerMonitorJob* pJob = new CDBServerMonitorJob;
				pJob->_iServerNo = 0;
				pJob->_iType = pServer->_ServerMonitorCollector[num].Type;
				pJob->_iValueMax = (int)(pServer->_ServerMonitorCollector[num].Max);
				pJob->_iValueMin = (int)(pServer->_ServerMonitorCollector[num].Min);

				// 평균 계산
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
// 프로세서 모니터링 정보 수집
// 
// Parameter: 없음
// return: 없음
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
// DB 쓰기 스레드
// 
// Parameter: (void*)MonitoringNetServer 객체 주소
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
// 각 서버 모니터 정보 패킷 처리
// 
// Parameter: (CPacket*) 패킷 직렬화버퍼
// void: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::PacketProc_UpdateServer(CPacket* pPacket)
{
	BYTE	byDataType;				// 모니터링 데이터 Type 하단 Define 됨.
	int		iDataValue;				// 해당 데이터 수치.
	int		iTimeStamp;

	*pPacket >> byDataType;
	*pPacket >> iDataValue;
	*pPacket >> iTimeStamp;

	//------------------------------------
	// 모니터링 정보 수집
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
	// 모니터링 정보 전송
	//------------------------------------
	CPacket* pSendPacket = CPacket::Alloc();
	mpUpdateData(pSendPacket, 0, byDataType, iDataValue, iTimeStamp);

	//------------------------------------
	// 모니터 서버 접속 유저 세션 ID 검색
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
	// 모니터링 정보 전송
	//------------------------------------
	for (int cnt = 0; cnt < udlSessionCount; ++cnt)
	{
		SendPacket(udlSessionArr[cnt], pSendPacket);
	}
	CPacket::Free(pSendPacket);
}

////////////////////////////////////////////////////////////////////////
// 채팅 요청 패킷 처리
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 직렬화버퍼
// void: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::PacketProc_LoginMonitorClient(ULONG64 udlSessionID, CPacket* pPacket)
{
	//------------------------------------
	// 유저 정보 참조 및 채팅 응답 메시지 생성
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
	// 로그인 응답 메시지 전송
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
// 유저 맵에 유저 추가
// 
// Parameter: (ULONG64)유저 세션 ID, (st_USER_INFO*)유저 정보 포인터
// return: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::AddUser(ULONG64 udlSessionID)
{
	_UserMap.push_back(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// 유저 맵에서 유저 찾기
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 유저 정보 구조체 주소
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
// 유저 맵에서 유저 삭제
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 없음
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
// 로그인 응답 메시지 생성
// 
// Parameter: (CPacket*)전송 데이터 저장 직렬화버퍼, (INT64)전송 대상 Account 번호, (BYTE)인증 성공 여부
// void: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::mpLoginRes(CPacket* pSendPacket, BYTE Status)
{
	WORD wType = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN;

	*pSendPacket << wType;
	*pSendPacket << Status;
}

////////////////////////////////////////////////////////////////////////
// 섹터 이동 응답 메시지 생성
// 
// Parameter: (CPacket*)전송 데이터 저장 직렬화버퍼, (INT64)전송 대상 Account 번호, (WORD)섹터 X, (WORD)섹터 Y
// void: 없음
////////////////////////////////////////////////////////////////////////
void MonitoringNetServer::mpUpdateData(CPacket* pSendPacket, BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp)
{
	// 패킷 생성
	WORD wType = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;

	*pSendPacket << wType;
	*pSendPacket << ServerNo;
	*pSendPacket << DataType;
	*pSendPacket << DataValue;
	*pSendPacket << TimeStamp;
}

////////////////////////////////////////////////////////////////////////
// 채팅 서버 모니터링
// 
// Parameter: 없음
// void: 없음
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
void MonitoringNetServer::ServerControl(void)
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