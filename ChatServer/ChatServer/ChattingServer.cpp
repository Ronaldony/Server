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
	
	// Config 파일에서 설정 값 불러오기
	CParserUnicode parser;

	if (false == parser.LoadFile(L"ChatServer.cnf"))
		wprintf(L"CONFIG FILE LOAD FAILED\n");
	
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

	_dwWorkerThreadNum = dwWorkderThreadNum;

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

	wprintf(L"------------------------------------------------------------------\n");

	// 타임아웃
	_hTimeCheckThread = (HANDLE)_beginthreadex(NULL, 0, Proc_TimeCheck, (void*)this, 0, (unsigned int*)&_dwTimeCheckThreadsID);
	if (_hTimeCheckThread== 0 || _hTimeCheckThread== (HANDLE)(-1))
	{
		wprintf(L"_beginthreadex error\n");
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
	timer = time(NULL);
	struct tm t;

	(void)localtime_s(&t, &timer);
	swprintf_s(_LogFileName, sizeof(_LogFileName) / sizeof(WCHAR), L"Chat Server Log [%d_%02d_%02d]", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	// Account 맵 동기화 객체 초기화
	InitializeSRWLock(&_UserMapLock);

	for (int cntY = 0; cntY < dfSECTOR_MAX_Y; ++cntY)
	{
		for (int cntX = 0; cntX < dfSECTOR_MAX_Y; ++cntX)
		{
			InitializeSRWLock(&_SectorMapLock[cntY][cntX]);
		}
	}

	// NetServer 가동
	Start(dwWorkderThreadNum, dwActiveThreadNum, wchIP, dwPort, dwMaxSession, iPacketCode, iPacketKey, false);
	
	_RedisClient.connect();
}

ChattingServer::~ChattingServer()
{

}

////////////////////////////////////////////////////////////////////////
// 연결 허용 여부 검사. 이곳에서 화이트 리스트 설정
// 
// Parameter: (WCHAR*)연결된 IP, (DWORD)포트 번호
// return: 없음
////////////////////////////////////////////////////////////////////////
bool ChattingServer::OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort)
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
void ChattingServer::OnClientJoin(ULONG64 udlSessionID)
{
	// Nothing
}

////////////////////////////////////////////////////////////////////////
// 유저 접속 종료 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID
// void: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::OnClientLeave(ULONG64 udlSessionID)
{
	//--------------------------------
	// 유저맵에서 로그아웃 유저 삭제
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
	// 섹터맵에서 로그아웃 유저 삭제
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
// 요청 패킷 수신 콜백 함수
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 저장 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::OnRecv(ULONG64 udlSessionID, CPacket* pPacket)
{
	//-----------------------------------------------------------
	// 메시지 파싱 
	//-----------------------------------------------------------
	WORD wType;
	*pPacket >> wType;

	// 메시지 타입에 따른 프로시저 호출
	switch (wType)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:		// 채팅서버 로그인 요청
		PacketProc_LoginUser(udlSessionID, pPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:	// 섹터 이동
		PacketProc_MoveSector(udlSessionID, pPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:		// 채팅 입력
		PacketProc_Chatting(udlSessionID, pPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:	// 하트비트
		PacketProc_HeartBeat(udlSessionID);
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
void ChattingServer::OnError(int errorcode, const WCHAR* comment)
{
	// TODO: 로직 스레드에 어떤 형태로 전달할 지 고민해보기
	//if (0 != errorcode)
		//LOG(L"ON_ERROR", CSystemLog::LEVEL_ERROR, L"[OnError] Error code: %d / Comment: %s\n", errorcode, comment);
}

////////////////////////////////////////////////////////////////////////
// 유저 타임아웃 검사 프로시저
// 
// Parameter: (void*)ChattingServer 객체 주소
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
		// 유저 타임아웃 체크
		//---------------------------------------------
		if (abs((long long)(udlNowTick - pServer->_udlUserTimeTick)) > en_USER_TIME_OUT_PERIOD)
		{
			// TPS 관련
			pServer->_udlUserTimeTick = udlNowTick - (udlNowTick - pServer->_udlUserTimeTick - en_USER_TIME_OUT_PERIOD);

			pServer->CheckUserTimeout();
		}

		//---------------------------------------------
		// 모니터 서버로 모니터 정보 전송
		//---------------------------------------------
		if (abs((long long)(udlNowTick - pServer->_udlMonitorTick)) > en_MONITOR_TIME_PERIOD)
		{
			// TPS 관련
			pServer->_udlMonitorTick = udlNowTick - (udlNowTick - pServer->_udlMonitorTick - en_MONITOR_TIME_PERIOD);

			pServer->_ProcessMonitor.UpdateMonitorInfo();
			int iTime = (int)time(NULL);

			// ChatServer 실행 여부 ON / OFF
			CPacket* pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, true, iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// CPU 사용률
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)(pServer->_ProcessMonitor.ProcessTotal()), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// 메모리 사용 MByte
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)(pServer->_ProcessMonitor.ProcessUserAllocMemory() / 1000000), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// 세션 수 (컨넥션 수)
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_SESSION, (int)(pServer->_dlNowSessionNum), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// UPDATE 스레드 초당 초리 횟수
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)(pServer->_MonitorData.TPS_ProcMSGMonitor), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// 패킷풀 사용량
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)(CPacket::GetCountOfPoolAlloc()), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			// 인증성공 사용자 수 (실제 접속자)
			ULONG64 userNum = pServer->_dlNowLoginUserNum;
			pSendPacket = CPacket::Alloc();
			pServer->_MonitorClient.mpUpdateMonitor(pSendPacket, dfMONITOR_DATA_TYPE_CHAT_PLAYER, (int)(pServer->_dlNowLoginUserNum), iTime);
			pServer->_MonitorClient.SendPacket(pSendPacket);
			CPacket::Free(pSendPacket);

			//---------------------------------------------
			// 최대 최소 값 계산
			//---------------------------------------------
			
			pServer->_MonitorData.TPS_ProcMSGMonitor = 0;

			// 메시지 처리 최대 최소
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
// 로그인 요청 패킷 처리
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 직렬화버퍼
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::PacketProc_LoginUser(ULONG64 udlSessionID, CPacket* pPacket)
{
	//------------------------------------
	// 로그인 절차
	//------------------------------------
	INT64	AccountNo;
	WCHAR	ID[20];				// null 포함
	WCHAR	Nickname[20];		// null 포함
	char	SessionKey[64];		// 인증토큰

	*pPacket >> AccountNo;
	pPacket->GetData((char*)&ID, sizeof(ID));
	pPacket->GetData((char*)&Nickname, sizeof(Nickname));
	pPacket->GetData((char*)&SessionKey, sizeof(SessionKey));

	// 로그인 인증 절차
	bool bRetVal = AuthenticateUser(udlSessionID, AccountNo, SessionKey);

	if (bRetVal)
		AddUserInfo(udlSessionID, AccountNo, ID, Nickname);

	InterlockedIncrement64(&_MonitorData.TPS_ProcMSG);
	// false 시 서버 최대 수용 유저 초과
	if (true == bRetVal)
	{
		CPacket* pSendPacket = CPacket::Alloc();
		mpLoginRes(pSendPacket, AccountNo, bRetVal);
		SendPacket(udlSessionID, pSendPacket);
		CPacket::Free(pSendPacket);
	}
}

////////////////////////////////////////////////////////////////////////
// 섹터 이동 요청 패킷 처리
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 직렬화버퍼
// void: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::PacketProc_MoveSector(ULONG64 udlSessionID, CPacket* pPacket)
{
	INT64	AccountNo;
	*pPacket >> AccountNo;

	WORD	SectorX;
	WORD	SectorY;

	*pPacket >> SectorX;
	*pPacket >> SectorY;

	// 섹터 범위 검사
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
	// 유저맵 섹터 정보 변경
	//------------------------------------
	AcquireSRWLockShared(&_UserMapLock);

	st_USER_INFO* pUser = FindUser(udlSessionID);
	if (NULL == pUser)
	{
		// 유저 로그인 여부 검사
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(udlSessionID);
		LOG(L"PacketProc_MoveSector", CSystemLog::LEVEL_ERROR, L"Not Found User ID: %lld\n", udlSessionID);
		return;
	}
	else if (AccountNo != pUser->AccountNo)
	{
		// Account 번호 검사
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
	// 섹터맵 삭제 및 추가
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
	// 섹터 이동 응답 전송
	//------------------------------------
	CPacket* pSendPacket = CPacket::Alloc();
	mpMoveSectorRes(pSendPacket, AccountNo, SectorX, SectorY);
	SendPacket(udlSessionID, pSendPacket);
	CPacket::Free(pSendPacket);
}

////////////////////////////////////////////////////////////////////////
// 채팅 요청 패킷 처리
// 
// Parameter: (ULONG64)유저 세션 ID, (CPacket*)요청 패킷 직렬화버퍼
// void: 없음
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
		// 실제로 메시지의 길이와 메시지 길이 정보가 다른 경우
		LOG(L"PacketProc_Chatting", CSystemLog::LEVEL_ERROR, L"Chatting Message Lenght Error. Recv Len: %d / Real Length: %d\n",
			MessageLen, pPacket->GetDataSize());
		Disconnect(udlSessionID);
		return;
	}

	CPacket* pSendPakcet = CPacket::Alloc();
	 
	//------------------------------------
	// 유저 정보 참조 및 채팅 응답 메시지 생성
	//------------------------------------
	AcquireSRWLockShared(&_UserMapLock);
	st_USER_INFO* pUser = FindUser(udlSessionID);

	if (NULL == pUser)
	{
		// 유저 로그인 여부 확인
		LOG(L"PacketProc_Chatting", CSystemLog::LEVEL_ERROR, L"Can't find the user: %lld\n", udlSessionID);
		isError = true;
	}
	else if (AccountNo != pUser->AccountNo)
	{
		// 유저 Account 번호 검사
		LOG(L"PacketProc_Chatting", CSystemLog::LEVEL_ERROR, L"AccountNo Error. Server: %lld / Recv: %lld\n", pUser->AccountNo, AccountNo);
		isError = true;
	}
	else if ((dfSECTOR_DEFAULT == pUser->SectorY) || (dfSECTOR_DEFAULT == pUser->SectorX))
	{
		// 섹터 좌표가 아직 입력되지 않은 상황 검사
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
	// 채팅 응답 메시지 처리
	//------------------------------------
	int iAroundSessionNum = 0;
	st_SECTOR_AROUND aroundSector;

	aroundSector.Count = 0;
	// 주변 섹터 정보 검색
	GetSectorAround(wUserSectorX, wUserSectorY, &aroundSector);

	//--------------------------------------------------------------
	// 평균 채팅 전송량 계산
	// 50x50 섹터를 기준으로 한 섹터당 평균 8.76 섹터를 담당한다.
	// (변두리(4*4) + 한쪽끝(6*4*48) + 그 외 (9*2304)) / (50*50) = 8.76
	// 예시: 위치 정보를 가진 유저가 1만명인 경우 1섹터당
	// 평균 4명의 유저가 위치한다. 
	// 따라서 평균 채팅 전송량 = 8.76 * 4 = 35.04가 된다.
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

			// 주변 섹터 채팅 유저 제한 초과
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
// 하트비트 요청 패킷 처리
// 
// Parameter: (ULONG64)유저 세션 ID
// void: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::PacketProc_HeartBeat(ULONG64 udlSessionID)
{
	//------------------------------------
	// 유저 수신 시간 갱신
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
// 유저 맵에 유저 추가
// 
// Parameter: (ULONG64)유저 세션 ID, (st_USER_INFO*)유저 정보 포인터
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::AddUser(ULONG64 udlSessionID, st_USER_INFO* pUser)
{
	_UserMap.insert(std::make_pair(udlSessionID, pUser));
}

////////////////////////////////////////////////////////////////////////
// 유저 맵에서 유저 찾기
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 유저 정보 구조체 주소
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
// 유저 맵에서 유저 삭제
// 
// Parameter: (ULONG64)유저 세션 ID
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::DeleteUser(ULONG64 udlSessionID)
{
	_UserMap.erase(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// Account 맵에 Account 번호 추가
// 
// Parameter: (ULONG64)Account 번호, (st_USER_INFO*)유저 객체 주소
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::AddAccount(ULONG64 udlAccount, ULONG64 udlSessionID)
{
	_AccountMap.insert(std::make_pair(udlAccount, udlSessionID));
}

////////////////////////////////////////////////////////////////////////
// Account 맵에서 Account 번호 검색
// 
// Parameter: (ULONG64)Account 번호
// return: Account 번호의 유무. (0 < return)있음, (0)없음
////////////////////////////////////////////////////////////////////////
ULONG64 ChattingServer::FindAccount(ULONG64 udlAccount)
{
	std::unordered_multimap<ULONG64, ULONG64>::iterator iter = _AccountMap.find(udlAccount);
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
// Sector 맵에 섹터 정보 추가
// 
// Parameter: (WORD)섹터 정보, (ULONG64)세션 ID
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::AddSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID)
{
	_SectorPosMap[wSectorY][wSectorX].insert(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// Sector 맵에서 섹터 정보 검색
// 
// Parameter: (WORD)섹터 정보, (ULONG64)세션 ID
// return: 섹터 정보에 해당하는 세션 ID의 유무. (true)있음, (false)없음
////////////////////////////////////////////////////////////////////////
bool ChattingServer::FindSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID)
{
	if (_SectorPosMap[wSectorY][wSectorX].end() != _SectorPosMap[wSectorY][wSectorX].find(udlSessionID))
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////
// Sector 맵에서 섹터 정보 삭제
// 
// Parameter: (WORD)섹터 정보, (ULONG64)세션 ID
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::DeleteSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID)
{
	_SectorPosMap[wSectorY][wSectorX].erase(udlSessionID);
}

////////////////////////////////////////////////////////////////////////
// 유저 리스트에 새로운 로그인 유저 추가
// 
// Parameter: (ULONG64), (INT64), (WCHAR*), (WCHAR*)
// return: (bool)소켓 및 링버퍼 에러 여부
////////////////////////////////////////////////////////////////////////
bool ChattingServer::AddUserInfo(ULONG64 udlSessionID, INT64 AccountNo, WCHAR* ID, WCHAR* NickName)
{
	//--------------------------------
	// 최대 동접자 초과 시 로그인 실패
	//--------------------------------
	if (_dwMaxLoginUserNum <= InterlockedIncrement64(&_dlNowLoginUserNum))
	{
		Disconnect(udlSessionID);
		LOG(L"AddUserInfo", CSystemLog::LEVEL_DEBUG, L"Max User LogIn\n", _dwMaxLoginUserNum);
		return false;
	}
	 
	//--------------------------------
	// 유저 생성 및 유저 맵에 등록
	//--------------------------------
	st_USER_INFO* userInfo = st_USER_INFO::Alloc();
	userInfo->SessionID = udlSessionID;
	userInfo->AccountNo = AccountNo;
	userInfo->SectorX = dfSECTOR_DEFAULT;
	userInfo->SectorY = dfSECTOR_DEFAULT;
	memcpy_s(&userInfo->ID, 20 * sizeof(WCHAR), ID, 20 * sizeof(WCHAR));
	memcpy_s(&userInfo->Nickname, 20 * sizeof(WCHAR), NickName, 20 * sizeof(WCHAR));

	//--------------------------------
	// 유저맵에 로그인 유저 추가
	//--------------------------------
	ULONG64 udlDisSession;
	AcquireSRWLockExclusive(&_UserMapLock);

	userInfo->LastRecvMsgTime = GetTickCount64();

	udlDisSession = FindAccount(AccountNo);

	AddAccount(AccountNo, udlSessionID);
	AddUser(udlSessionID, userInfo);

	ReleaseSRWLockExclusive(&_UserMapLock);

	// 중복 로그인인 경우 세션 종료
	if (0 != udlDisSession)
		Disconnect(udlDisSession);	// 기존 접속 유저 연결 종료

	return true;
}

////////////////////////////////////////////////////////////////////////
// 유저 타임 아웃 검사
// 
// Parameter: 없음
// return: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::CheckUserTimeout(void)
{
	ULONG64 uldSessionIDArr[en_CHECK_TIMEOUT];
	int iSessionCount = 0;

	//-----------------------------------------------
	// 유저맵을 순회하며 마지막 메시지 수신 시간 검사
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
// 유저 인증
// 
// Parameter: (ULONG64)유저 세션 ID, (INT64)유저 Account 번호, (char*)세션 키
// return: (bool)인증 성공 여부
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
// 입력된 섹터 좌표를 기준으로 주변 섹터 구하기
// 
// Parameter: (int)기준 섹터 X, (int)기준 섹터 Y, (st_SECTOR_AROUND*)주변 섹터 정보를 저장할 주소
// void: 없음
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
// 로그인 응답 메시지 생성
// 
// Parameter: (CPacket*)전송 데이터 저장 직렬화버퍼, (INT64)전송 대상 Account 번호, (BYTE)인증 성공 여부
// void: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status)
{
	WORD wType = en_PACKET_CS_CHAT_RES_LOGIN;

	*pSendPacket << wType;
	*pSendPacket << Status;
	*pSendPacket << AccountNo;
}

////////////////////////////////////////////////////////////////////////
// 섹터 이동 응답 메시지 생성
// 
// Parameter: (CPacket*)전송 데이터 저장 직렬화버퍼, (INT64)전송 대상 Account 번호, (WORD)섹터 X, (WORD)섹터 Y
// void: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::mpMoveSectorRes(CPacket* pSendPacket, INT64 AccountNo, WORD SectorX, WORD SectorY)
{
	// 패킷 생성
	WORD wType = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;

	*pSendPacket << wType;
	*pSendPacket << AccountNo;
	*pSendPacket << SectorX;
	*pSendPacket << SectorY;
}

////////////////////////////////////////////////////////////////////////
// 채팅 응답 메시지 생성
// 
// Parameter: (CPacket*)전송 데이터 저장 직렬화버퍼, (INT64)전송 대상 Account 번호, (WCHAR*)채팅 유저 ID, (WCHAR*)채팅 유저 닉네임,
// (WORD)채팅 메시지 길이(BYTE 크기), (WCHAR*)채팅 메시지
// void: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::mpChatting(CPacket* pSendPacket, INT64 AccountNo, WCHAR* ID, WCHAR* Nickname, WORD MessageLen, WCHAR* Message)
{
	// 패킷 생성
	WORD wType = en_PACKET_CS_CHAT_RES_MESSAGE;

	*pSendPacket << wType;
	*pSendPacket << AccountNo;
	pSendPacket->PutData((char*)ID, 20 * sizeof(WCHAR));
	pSendPacket->PutData((char*)Nickname, 20 * sizeof(WCHAR));
	*pSendPacket << MessageLen;
	pSendPacket->PutData((char*)Message, MessageLen);
}

////////////////////////////////////////////////////////////////////////
// 채팅 서버 모니터링
// 
// Parameter: 없음
// void: 없음
////////////////////////////////////////////////////////////////////////
void ChattingServer::MonitoringOutput(void)
{
	st_MonitoringInfo mi;
	time_t timer = time(NULL);
	struct tm t;

	/* 모니터 업데이트 */

	(void)localtime_s(&t, &timer);
	GetMonitoringInfo(&mi);

	// 모니터 정보 업데이트
	_dlNowSessionNum = mi.NowSessionNum;

	WCHAR *wchMonitorBuffer = new WCHAR[10240];
	int iBufSize = 10240;

	//---------------------------------------------------------------
	// 모니터링 정보 콘솔 출력
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
void ChattingServer::ServerControl(void)
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