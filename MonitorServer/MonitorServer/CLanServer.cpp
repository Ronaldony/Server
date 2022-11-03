#include <stdio.h>
#include <WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#include <crtdbg.h>
#include <DbgHelp.h>
#include "CLanServer.h"
#include "SystemLog.h"
#include "CCrashDump.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

CLanServer::CLanServer()
	: _pSessionArr(NULL), _dwMaxSession(0), _dwPort(0), _bNagleOpt(false), _hIOCP(NULL), _phThreads(NULL), _pdwThreadsID(NULL),
	_dwWorkderThreadNum(0), _dwActiveThreadNum(0)
{
	memset(_wchIP, 0, sizeof(_wchIP));
	memset(&_stMonitoringOnGoing, 0, sizeof(_stMonitoringOnGoing));
	_dwMonitorTick = timeGetTime();
}

CLanServer::~CLanServer()
{
	Stop();
	if (WAIT_FAILED == WaitForMultipleObjects(_dwWorkderThreadNum, _phThreads, TRUE, INFINITE))
		wprintf(L"WaitForMultipleObjects failed\n");
	else
		wprintf(L"Success for exit all thread\n");

	delete[] _phThreads;
	delete[] _pdwThreadsID;

	WSACleanup();
}

////////////////////////////////////////////////////////////////////////
// 서버 설정 및 활성화
// 
// Parameter: (DWORD)IO 워커 스레드 생성 개수, (DWORD)IO 워커 스레드 동시 실행 개수, (const WCHAR*)서버 바인드 IP, (DWORD)서버 포트, (DWORD)세션 생성 개수, 
// (BYTE)패킷 코드, (BYTE)패킷 고정 키, (bool)네이글 옵션
// Return: bool (true)활성화 성공, (false)활성화 실패
////////////////////////////////////////////////////////////////////////
bool CLanServer::Start(int dwWorkerThradNum, DWORD dwActiveThreadNum, const WCHAR* pwchIP, DWORD dwPort, DWORD dwMaxSessionNum, bool bNagleOpt)
{
	// 네이글 옵션 
	_bNagleOpt = bNagleOpt;

	// 세션수 최대 크기 검사
	if (dfSESSION_INDEX_MASKING < dwMaxSessionNum)
	{
		OnError(0, L"[Start] The creation number for Session exceeds limitation\n");
		return false;
	}

	// 멤버 변수 초기화
	_dwMaxSession = dwMaxSessionNum;
	_pSessionArr = (st_LAN_SESSION*)malloc(sizeof(st_LAN_SESSION) * dwMaxSessionNum);

	for (int cnt = 0; cnt < (int)dwMaxSessionNum; cnt++)
	{
		_pSessionArr[cnt].IsInit = false;
	}

	// 초기 모든 세션 메모리 입력
	for (int cnt = 0; cnt < (int)dwMaxSessionNum; cnt++)
	{
		_AvailableSessionStack.Push(cnt);
	}

	_dwPort = dwPort;
	errno_t err = wcscpy_s(_wchIP, pwchIP);
	if (0 != err)
	{
		OnError(0, L"IP wcscpy_s error\n");
		return false;
	}

	// 윈소켓 초기화
	WSAData wsa;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
	{
		OnError(WSAGetLastError(), L"WSAStartup error\n");
		return false;
	}

	// IOCP 생성
	_dwActiveThreadNum = dwActiveThreadNum;
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, dwActiveThreadNum);
	if (NULL == _hIOCP)
	{
		OnError(WSAGetLastError(), L"CreateIoCompletionPort error\n");
		return false;
	}
	
	// 스레드 생성
	_dwWorkderThreadNum = dwWorkerThradNum;

	_phThreads = new HANDLE[dwWorkerThradNum + 2];		// +1은 Accept 스레드 생성
	_pdwThreadsID = new DWORD[dwWorkerThradNum + 2];

	// IO 워커 스레드
	for (int cnt = 0; cnt < (int)dwWorkerThradNum; cnt++)
	{
		// Worker 스레드
		_phThreads[cnt] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread,
			(void*)this, CREATE_SUSPENDED, (unsigned int*)&_pdwThreadsID[cnt]);
		if (_phThreads[cnt] == 0 || _phThreads[cnt] == (HANDLE)(-1))
		{
			OnError(WSAGetLastError(), L"_beginthreadex error\n");
			return false;
		}

		// CPU 선호도 지정
		ULONG64 masking = 1;
		SetThreadAffinityMask(_phThreads[cnt], masking << cnt);
		ResumeThread(_phThreads[cnt]);
	}

	// Accept 스레드
	_phThreads[dwWorkerThradNum] = (HANDLE)_beginthreadex(NULL, 0, AcceptThread,
		(void*)this, 0, (unsigned int*)&_pdwThreadsID[dwWorkerThradNum]);
	if (_phThreads[dwWorkerThradNum] == 0 || _phThreads[dwWorkerThradNum] == (HANDLE)(-1))
	{
		OnError(WSAGetLastError(), L"_beginthreadex error\n");
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////
// 서버 종료
// 
// Parameter: 없음
// Return: 없음
////////////////////////////////////////////////////////////////////////
void CLanServer::Stop(void)
{
	// TODO: Accept 및 워커 스레드 종료 코드(PQCS)
}

////////////////////////////////////////////////////////////////////////
// 세션 연결 종료
// 
// Parameter: (ULONG64)세션 ID
// Return: bool (true)끊기 성공, (false)끊기 실패
////////////////////////////////////////////////////////////////////////
bool CLanServer::Disconnect(ULONG64 udlSessionID)
{
	// 세션 검색
	if ((udlSessionID & dfSESSION_INDEX_MASKING) >= _dwMaxSession)
		return false;

	st_LAN_SESSION* pSession = &_pSessionArr[udlSessionID & dfSESSION_INDEX_MASKING];
	if (udlSessionID != pSession->SessionID)
		return false;

	// IO Count 증가 및 Release 플래그 확인
	if (InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount) & dfRELEASE_FLAG_MASKING)
		return false;

	// 만약 해당 세션이 재활용된 경우라면 IO Count를 다시 차감해야 한다.
	// 만약 차감하지 않으면 재활용된 세션이 Release 되지 않을 것이다.
	if (udlSessionID != pSession->SessionID)
	{
		if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
			Release(pSession);
		return false;
	}

	pSession->IsDisconn = true;
	// IO 취소
	CancelIoEx((HANDLE)(pSession->Sock), NULL);

	// IO Count 감소
	if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
		Release(pSession);

	return true;
}

////////////////////////////////////////////////////////////////////////
// 세션 연결 종료 및 자원 반환
// 
// Parameter: (st_LAN_SESSION*)세션 주소
// Return: bool (true)성공, (false)실패
////////////////////////////////////////////////////////////////////////
void CLanServer::Release(st_LAN_SESSION* pSession)
{
	// 외부에서 SendPacket 혹은 Disconnect를 사용중인지 확인하기 위한 동작
	if (0 != InterlockedCompareExchange64(&pSession->ReleaseFlagAndIOCount, dfRELEASE_FLAG_MASKING, 0))
		return;

	pSession->IsDisconn = true;

	if (INVALID_SOCKET != pSession->Sock)
	{
		if (SOCKET_ERROR == closesocket(pSession->Sock))
		{
			LOG(L"Release", CSystemLog::LEVEL_ERROR, L"Error code: %d / SID: %lld, sock: %d, index: %d, useFlag: %d, IO count: %lld\n", 
				WSAGetLastError(), pSession->SessionID, pSession->Sock, pSession->SessionIndex, pSession->UseFlag, pSession->ReleaseFlagAndIOCount);
			OnError(WSAGetLastError(), L"[Disconnect] closesocket error\n");
			return;
		}
		pSession->Sock = INVALID_SOCKET;
	}

	OnClientLeave(pSession->SessionID);

	// 만약 SendPost에서 송신 패킷을 DeQ 하였는데 WSASend에 실패하는 경우
	// 이곳에서 송신 패킷 메모리 Free
	for (int cnt = 0; cnt < pSession->SendRqstNum; cnt++)
	{
		CPacket::Free(pSession->SendPacket[cnt]);
	}

	// 아직 DeQ 되지 않은 송신 패킷 DeQ 및 메모리 Free
	CPacket* freePacket;
	while (pSession->SendQ.Dequeue(freePacket))
	{
		CPacket::Free(freePacket);
	}

	// 세션 종료 알림 및 세션 초기화
	pSession->SessionID = 0xFFFFFFFFFFFFFFFF;
	pSession->UseFlag = false;
	_AvailableSessionStack.Push(pSession->SessionIndex);

	// 모니터 정보
#ifndef dfSERVER_MODULE_BENCHMARK
	InterlockedDecrement64(&_stMonitoringOnGoing.NowSessionNum);
	InterlockedIncrement64(&_stMonitoringOnGoing.DisconnectTPS);
	//InterlockedIncrement64(&_stMonitoringOnGoing.DisconnetTotal);
#endif

	return;
}

//////////////////////////////////////////////////////////////////////////
// 전송할 패킷을 SendQ 에 인큐
//
// Parameters: (ULONG64)세션 ID, (CPacket*)전송 패킷 직렬화버퍼 주소
// Return: bool (true)성공, (false)실패
//////////////////////////////////////////////////////////////////////////
void CLanServer::SendPacket(ULONG64 SessionID, CPacket* packet)
{
	st_LAN_SESSION* pSession = FindSession(SessionID);
	if (NULL == pSession)
		return;

	// IO Count 증가 및 Release 플래그 확인
	if (InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount) & dfRELEASE_FLAG_MASKING)
		return;

	// 만약 해당 세션이 이미 재활용된 경우라면 IO Count를 다시 차감해야 한다.
	// 만약 차감하지 않으면 재활용된 세션이 Release 되지 않을 것이다.
	if (SessionID != pSession->SessionID)
	{
		if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
		{
			LOG(L"SendPacket", CSystemLog::LEVEL_DEBUG, L"Now Session ID: %lld / Prev Session ID: %lld\n", SessionID, pSession->SessionID);
			Release(pSession);
		}
		return;
	}

	// SendQ EnQ
	if (packet->GetDataSize())
	{
		packet->addRef();
		if (false == packet->AlreadyLANHeader())
		{
			st_LanHeader lanHeader;
			lanHeader.Len = packet->GetDataSize();
			packet->PutLANHeader((char*)&lanHeader, lanHeader.Len);
		}
		pSession->SendQ.Enqueue(packet);
	}

	// 패킷 전송
	SendPost(pSession);

	// IO Count 감소
	if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
		Release(pSession);

	return;
}

//////////////////////////////////////////////////////////////////////////
// 세션 Accept
//
// Parameters: (void*)CLanServer 객체 주소
// Return: 
//////////////////////////////////////////////////////////////////////////
unsigned int __stdcall CLanServer::AcceptThread(void* pvServerObj)
{
	CLanServer* pServer = (CLanServer*)pvServerObj;

	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == listenSock)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] socket error\n");
		return 0;
	}

	SOCKADDR_IN serveraddr;

	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	InetPtonW(AF_INET, pServer->_wchIP, &serveraddr.sin_addr);	// L"0.0.0.0" => INADDR_ANY
	serveraddr.sin_port = htons((u_short)(pServer->_dwPort));

	int retval;
	retval = bind(listenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (SOCKET_ERROR == retval)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] bind error\n");
		return 0;
	}
 
	// 소켓 옵션 - 링거 옵션 Off
	LINGER lig;
	lig.l_onoff = 1;
	lig.l_linger = 0;

	retval = setsockopt(listenSock, SOL_SOCKET, SO_LINGER, (const char*)&lig, sizeof(lig));
	if (SOCKET_ERROR == retval)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] setsockopt Linger error\n");
		return 0;
	}

	// 소켓 옵션 - 소켓 송신 버퍼 0 설정
	DWORD sndSockBuffer = 0;
	retval = setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (const char*)&sndSockBuffer, sizeof(sndSockBuffer));
	if (SOCKET_ERROR == retval)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] setsockopt Nagle error\n");
		return 0;
	}

	// 소켓 옵션 - 네이글 알고리즘 Off
	DWORD delayOpt = !pServer->_bNagleOpt;
	retval = setsockopt(listenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&delayOpt, sizeof(delayOpt));
	if (SOCKET_ERROR == retval)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] setsockopt Nagle error\n");
		return 0;
	}

	// Listen 소켓
	retval = listen(listenSock, SOMAXCONN_HINT(SOMAXCONN));
	if (SOCKET_ERROR == retval) 
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] listen error\n");
		return 0;
	}

	//------------------------------------------------------
	// Accept
	//------------------------------------------------------
	ULONG64 ullSessionIDCount = dfSESSION_INDEX_MASKING + 1;

	while (1)
	{
		// 새로운 세션
		int iSessionIndex = pServer->FindAvailableSession();
		if ((-1) == iSessionIndex)
			continue;

		st_LAN_SESSION* pSession = &pServer->_pSessionArr[iSessionIndex];

		sockaddr_in clientaddr;
		int addrLen = sizeof(clientaddr);
		ZeroMemory(&clientaddr, sizeof(clientaddr));

		SOCKET newConnect = accept(listenSock, (sockaddr*)&clientaddr, &addrLen);
		if (INVALID_SOCKET == newConnect)
		{
			pServer->OnError(WSAGetLastError(), L"[Accept] accept error\n");
			return 0;
		}

		// White IP 확인
		WCHAR wchIP[17] = {0,};
		USHORT ushPort = ntohs(clientaddr.sin_port);
		InetNtopW(AF_INET, &clientaddr.sin_addr, wchIP, 16);

		if (false == pServer->OnConnectionRequest(wchIP, ushPort))
		{
			if (SOCKET_ERROR == closesocket(newConnect))
			{
				pServer->OnError(WSAGetLastError(), L"[Accept] closesocket error\n");
				return 0;
			}
			continue;
		}

		if (false == pSession->IsInit)
		{
			pSession->IsInit = true;
			new (pSession) st_LAN_SESSION;
		}
		// 세션 초기화 및 생성
		pSession->ReleaseFlagAndIOCount = 0;
		pSession->IsDisconn = false;
		pSession->SessionID = (ullSessionIDCount & ~dfSESSION_INDEX_MASKING) | iSessionIndex;
		pSession->SessionIndex = iSessionIndex;
		pSession->Sock = newConnect;
		pSession->SockAddr.sin_port = ushPort;
		pSession->SockAddr.sin_addr.s_addr = clientaddr.sin_addr.s_addr;

		pSession->RecvQ.ClearBuffer();
		ZeroMemory(&pSession->SendOverlapped, sizeof(pSession->SendOverlapped));
		ZeroMemory(&pSession->RecvOverlapped, sizeof(pSession->RecvOverlapped));
		pSession->SendRqstNum = 0;
		InterlockedExchange64(&pSession->SendFlag, true);
		// printf("NetWorkLIB - 클라이언트 접속: IP 주소=%s / 포트 번호=%d\n",inet_ntoa(pSession->SockAddr.sin_addr), ntohs(pSession->SockAddr.sin_port));

		// IOCP에 소켓 등록
		CreateIoCompletionPort((HANDLE)newConnect, pServer->_hIOCP, (ULONG64)pSession, 0);

		pServer->RecvPost(pSession);

		// 사용자에게 새로운 세션 알림
		pServer->OnClientJoin(pSession->SessionID);

		// 세션 ID 갱신
		ullSessionIDCount += dfSESSION_INDEX_MASKING + 1;
		if (0 == ullSessionIDCount)
		{
			ullSessionIDCount = dfSESSION_INDEX_MASKING + 1;
		}

#ifndef dfSERVER_MODULE_BENCHMARK
		// 모니터 정보
		InterlockedIncrement64(&pServer->_stMonitoringOnGoing.AcceptTPS);
		//InterlockedIncrement64(&pServer->_stMonitoringOnGoing.AcceptTotal);
		InterlockedIncrement64(&pServer->_stMonitoringOnGoing.NowSessionNum);
#endif
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// IO 워커 스레드
//
// Parameters: (void*)CLanServer 객체 주소
// Return: 
//////////////////////////////////////////////////////////////////////////
unsigned int __stdcall CLanServer::WorkerThread(void* pvServerObj)
{
	printf("Net_Worker Start\n");

	CLanServer* pServer = (CLanServer*)pvServerObj;
	CPacket* pEmptyPacket = CPacket::Alloc();

	while (1)
	{
		st_LAN_OVERLAPPED_EX* pOverlappedData = NULL;
		st_LAN_SESSION* pSession = NULL;
		DWORD dwTransferredBytes = 0;

		BOOL bRetGQCS = GetQueuedCompletionStatus(
			pServer->_hIOCP, &dwTransferredBytes, (PULONG_PTR)&pSession,
			(LPOVERLAPPED*)&pOverlappedData, INFINITE);

		if (NULL == pOverlappedData)
		{
			// 상황1: IOCP 디큐잉 실패 => IOCP 자체 에러
			// 상황2: 외부에서 PQCS를 통해 의도적으로 Worker 스레드를 종료하기 위함
			pServer->OnError(0, L"IOCP dequeuing error\n");
			break;
		}

		//if (false == bRetGQCS)
			//LOG(L"GQCS ERROR", CSystemLog::LEVEL_ERROR, L"Error code: %d / Session ID: %lld\n", WSAGetLastError(), pSession->SessionID);

		if (dwTransferredBytes == 0)
		{
			pServer->Disconnect(pSession->SessionID);
		}
		else
		{
			// IO 작업에 대한 송수신 바이트 개수가 0보다 큰 경우, 다음과 같은 상황이다.
			// 송신: (요청한 크기가 반환된 경우)송신 성공, (요청한 크기보다 작은 값이 반환된 경우)IO 작업 중 세션 종료됨
			// 수신: 수신 성공
			if (true == pOverlappedData->bIsSend)
			{
#ifndef dfSERVER_MODULE_BENCHMARK
				int iTotalSendBytes = 0;
#endif

				//송신 완료 통지 처리
				for (int cnt = 0; cnt < pSession->SendRqstNum; cnt++)
				{
#ifndef dfSERVER_MODULE_BENCHMARK
					iTotalSendBytes += pSession->SendPacket[cnt]->GetDataSizeWithLANHeader();
#endif
					CPacket::Free(pSession->SendPacket[cnt]);
				}

				pSession->SendRqstNum = 0;

				InterlockedExchange64(&pSession->SendFlag, true);

#ifndef dfSERVER_MODULE_BENCHMARK
				// 모니터 정보
				InterlockedAdd64(&pServer->_stMonitoringOnGoing.SendBPS, iTotalSendBytes + 40);
#endif
				if (pSession->SendQ.GetUseSize() > 0)
					pServer->SendPacket(pSession->SessionID, pEmptyPacket);
			}
			else
			{
				// 수신 완료 통지 처리
				pSession->RecvQ.MoveRear(dwTransferredBytes);

#ifndef dfSERVER_MODULE_BENCHMARK
				// 모니터 정보
				InterlockedAdd64(&pServer->_stMonitoringOnGoing.RecvBPS, dwTransferredBytes + 40);	// + 40(IP, TCP 헤더)
#endif

				while (1)
				{
					// 네트워크 메시지 헤더 및 페이로드 추출
					st_LanHeader lanHeader;

					if (pSession->RecvQ.GetUseSize() < sizeof(st_LanHeader))
						break;

					pSession->RecvQ.Peek((char*)&lanHeader, sizeof(st_LanHeader));

					if (pSession->RecvQ.GetUseSize() < (LONG64)(sizeof(st_LanHeader) + lanHeader.Len))
						break;

					pSession->RecvQ.MoveFront(sizeof(st_LanHeader));

					// 버퍼 크기 미체크 시 아래 RecvQ.Dequeue 명령에서 직렬화
					if (CPacket::GetBufferSize() <= lanHeader.Len)
					{
						pServer->OnError(1, L"CPacekt Over Size\n");
						break;
					}

					CPacket* pRecvPacket = CPacket::Alloc();

					pSession->RecvQ.Dequeue(pRecvPacket->GetBufferPtr(), lanHeader.Len);
					pRecvPacket->MoveWritePos(lanHeader.Len);

					pServer->OnRecv(pSession->SessionID, pRecvPacket);
					
					CPacket::Free(pRecvPacket);
				}
				pServer->RecvPost(pSession);
			}
		}
		// IO count 차감 - 완료 통지 처리에 대한 차감
		if (0 == (InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount) & dfSESSION_IOCOUNT_MASKING))
			pServer->Release(pSession);
	}
	CPacket::Free(pEmptyPacket);
	 
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// SendQ에 저장된패킷을 WSASend에 등록
//
// Parameters: (st_LAN_SESSION*)세션 주소
// Return: bool (true)등록 성공, (false)등록 실패
//////////////////////////////////////////////////////////////////////////
void CLanServer::SendPost(st_LAN_SESSION* pSession)
{
	// 송신 플래그 갱신
	if (false == InterlockedExchange64(&pSession->SendFlag, false))
		return;
	
	if (0 == pSession->SendQ.Dequeue(pSession->SendPacket[0]))
	{
		InterlockedExchange64(&pSession->SendFlag, true);
		return;
	}
	
	// 전송할 패킷 직렬화 버퍼 꺼내기
	int deqNum = 1;
	while (pSession->SendQ.Dequeue(pSession->SendPacket[deqNum]))
	{
		deqNum++;
		if (deqNum >= dfSEND_NET_PAKCET_SIZE)
			break;
	}

	// WSABUF 등록
	WSABUF wsaSendBuf[dfSEND_NET_PAKCET_SIZE];

	for (int cnt = 0; cnt < deqNum; cnt++)
	{
		wsaSendBuf[cnt].len = pSession->SendPacket[cnt]->GetDataSizeWithLANHeader();
		wsaSendBuf[cnt].buf = pSession->SendPacket[cnt]->GetLANHeaderPtr();
	}

	// WSASend 호출 전에 IO count를 먼저 증가시키는 이유
	// WSASend가 반환되기도 전에 해당 송신 완료 통지 처리로 인해 IO count가 0이 될 수 있다.
	pSession->SendRqstNum = deqNum;
	pSession->SendOverlapped.bIsSend = true;
	InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount);

	int sendRet = WSASend(
		pSession->Sock, wsaSendBuf, deqNum, NULL,	// 정상 작동시 &dwRecvBytes 추가하여 실행해보기
		0, (OVERLAPPED*)&pSession->SendOverlapped, NULL
	);

	if (SOCKET_ERROR == sendRet)
	{
		int err = WSAGetLastError();
		if (WSA_IO_PENDING != err)
		{
			if (WSAECONNRESET != err)
				OnError(err, L"Socket Send error\n");

			if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
				Release(pSession);
			return;
		}
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
// 수신용 링버퍼를 WSARecv에 등록
//
// Parameters: (st_LAN_SESSION*)세션 주소
// Return: bool (true)등록 성공, (false)등록 실패
//////////////////////////////////////////////////////////////////////////
void CLanServer::RecvPost(st_LAN_SESSION* pSession)
{
	if (true == pSession->IsDisconn)
		return;

	DWORD recvFlag = 0;
	WSABUF wsaRecvBuf[2];

	// 버퍼 등록
	wsaRecvBuf[0].len = pSession->RecvQ.DirectEnqueueSize();
	wsaRecvBuf[0].buf = pSession->RecvQ.GetRearBufferPtr();
	wsaRecvBuf[1].len = pSession->RecvQ.GetFreeSize() - pSession->RecvQ.DirectEnqueueSize();
	wsaRecvBuf[1].buf = pSession->RecvQ.GetBufferPtr();

	pSession->RecvOverlapped.bIsSend = false;

	// WSARecv 호출 전에 IO count를 먼저 증가시키는 이유
	// WSARecv가 반환되기도 전에 해당 송신 완료 통지 처리 루틴으로 인해 IO count가 0이 될 수 있다.
	InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount);

	int recvRet = WSARecv(
		pSession->Sock, wsaRecvBuf, 2, NULL,	// 정상 작동시 &dwRecvBytes 추가하여 실행해보기
		&recvFlag, (OVERLAPPED*)&pSession->RecvOverlapped, NULL
	);

	if (SOCKET_ERROR == recvRet)
	{
		int err = WSAGetLastError();
		if (WSA_IO_PENDING != err)
		{
			if (WSAECONNRESET != err)
				OnError(err, L"Socket Recv error\n");

			if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
				Release(pSession);
			return;
		}
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
// 사용 가능한 세션 배열 인덱스 반환
//
// Parameters: 없음
// Return: int (양수)인덱스 번호, (-1)사용 가능한 인덱스 없음
//////////////////////////////////////////////////////////////////////////
int CLanServer::FindAvailableSession(void)
{
	// 스택 방법
	// 추후 위 순회 방법과 성능 비교
	int index;
	if (0 == _AvailableSessionStack.Pop(index))
		return (-1);

	return index;
}

//////////////////////////////////////////////////////////////////////////
// 세션 ID에 해당하는 세션 주소 반환
//
// Parameters: (ULONG64)세션 ID
// Return: st_LAN_SESSION* (!NULL)세션 주소, (NULL)ID에 해당하는 세션이 이미 끊겼거나 없음
//////////////////////////////////////////////////////////////////////////
st_LAN_SESSION* CLanServer::FindSession(ULONG64 SessionID)
{
	// 세션 배열 인덱스 초과
	if ((SessionID & dfSESSION_INDEX_MASKING) >= _dwMaxSession)
		return NULL;

	st_LAN_SESSION* pSession = &_pSessionArr[SessionID & dfSESSION_INDEX_MASKING];

	if ((pSession->SessionID != SessionID) || (true == pSession->IsDisconn))
		return NULL;
	else
		return pSession;
}

//////////////////////////////////////////////////////////////////////////
// 외부로 제공될 모니터링 정보 저장
//
// Parameters: (st_MonitoringInfo*)외부로 제공될 모니터링 정보 주소
// Return: 없음
//////////////////////////////////////////////////////////////////////////
void CLanServer::GetMonitoringInfo(st_MonitoringInfo* pMonitorInfo)
{
	pMonitorInfo->AcceptTPS = InterlockedExchange64(&_stMonitoringOnGoing.AcceptTPS, 0);
	pMonitorInfo->DisconnectTPS = InterlockedExchange64(&_stMonitoringOnGoing.DisconnectTPS, 0);

	_stMonitoringOnGoing.AcceptTotal += pMonitorInfo->AcceptTPS;
	_stMonitoringOnGoing.DisconnetTotal += pMonitorInfo->DisconnectTPS;

	pMonitorInfo->AcceptTotal = _stMonitoringOnGoing.AcceptTotal;
	pMonitorInfo->DisconnetTotal = _stMonitoringOnGoing.DisconnetTotal;

	pMonitorInfo->RecvTPS = InterlockedExchange64(&_stMonitoringOnGoing.RecvTPS, 0);
	pMonitorInfo->SendTPS = InterlockedExchange64(&_stMonitoringOnGoing.SendTPS, 0);
	pMonitorInfo->NowSessionNum = _stMonitoringOnGoing.NowSessionNum;

	pMonitorInfo->RecvBPS = InterlockedExchange64(&_stMonitoringOnGoing.RecvBPS, 0);
	pMonitorInfo->SendBPS = InterlockedExchange64(&_stMonitoringOnGoing.SendBPS, 0);
}
