#include "stdafx.h"
#include "CLanClient.h"
#include "SystemLog.h"
#include "CCrashDump.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

CLanClient::CLanClient()
	: _dwServerPort(0), _bNagleOpt(false), _hIOCP(NULL), _phThreads(NULL), _pdwThreadsID(NULL),
	_dwWorkderThreadNum(0), _dwActiveThreadNum(0)
{
	memset(_wchServerIP, 0, sizeof(_wchServerIP));
}

CLanClient::~CLanClient()
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
// Parameter: (DWORD)IO 워커 스레드 생성 개수, (DWORD)IO 워커 스레드 동시 실행 개수, (const WCHAR*)서버 IP, (DWORD)서버 포트, (DWORD)세션 생성 개수, 
// (BYTE)패킷 코드, (BYTE)패킷 고정 키, (bool)네이글 옵션
// Return: bool (true)활성화 성공, (false)활성화 실패
////////////////////////////////////////////////////////////////////////
bool CLanClient::Start(int dwWorkerThradNum, DWORD dwActiveThreadNum, const WCHAR* pwchIP, DWORD dwPort, bool bNagleOpt)
{
	// 네이글 옵션 
	_bNagleOpt = bNagleOpt;

	_dwServerPort = dwPort;
	errno_t err = wcscpy_s(_wchServerIP, pwchIP);
	if (0 != err)
	{
		OnError(0, L"IP wcscpy_s error\n");
		return false;
	}

	// 윈소켓 초기화
	WSAData wsa;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
	{
		OnError(WSAGetLastError(), L"[Net_Option] WSAStartup error\n");
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
		ULONG64 masking = 1;
		SetThreadAffinityMask(_phThreads[cnt], masking << cnt);
		ResumeThread(_phThreads[cnt]);
	}

	_MySession.UseFlag = false;

	return true;
}

////////////////////////////////////////////////////////////////////////
// 서버 종료
// 
// Parameter: 없음
// Return: 없음
////////////////////////////////////////////////////////////////////////
void CLanClient::Stop(void)
{
	// TODO: Accept 및 워커 스레드 종료 코드(PQCS)
}

////////////////////////////////////////////////////////////////////////
// 세션 연결 종료
// 
// Parameter: (ULONG64)세션 ID
// Return: bool (true)끊기 성공, (false)끊기 실패
////////////////////////////////////////////////////////////////////////
bool CLanClient::Disconnect(void)
{
	// IO Count 증가 및 Release 플래그 확인
	if (InterlockedIncrement64(&_MySession.ReleaseFlagAndIOCount) & dfRELEASE_FLAG_MASKING)
		return false;

	_MySession.IsDisconn = true;
	
	// IO 취소
	CancelIoEx((HANDLE)(_MySession.Sock), NULL);


	// IO Count 감소
	if (0 == InterlockedDecrement64(&_MySession.ReleaseFlagAndIOCount))
		Release();

	return true;
}

////////////////////////////////////////////////////////////////////////
// 세션 연결 종료 및 자원 반환
// 
// Parameter: (st_LAN_SESSION*)세션 주소
// Return: bool (true)성공, (false)실패
////////////////////////////////////////////////////////////////////////
void CLanClient::Release(void)
{
	// 외부에서 SendPacket 혹은 Disconnect를 사용중인지 확인하기 위한 동작
	if (0 != InterlockedCompareExchange64(&_MySession.ReleaseFlagAndIOCount, dfRELEASE_FLAG_MASKING, 0))
		return;

	_MySession.IsDisconn = true;

	if (INVALID_SOCKET != _MySession.Sock)
	{
		if (SOCKET_ERROR == closesocket(_MySession.Sock))
		{
			LOG(L"Release", CSystemLog::LEVEL_ERROR, L"Error code: %d / sock: %d, IO count: %lld\n", 
				WSAGetLastError(), _MySession.Sock, _MySession.ReleaseFlagAndIOCount);
			OnError(WSAGetLastError(), L"[Disconnect] closesocket error\n");
			return;
		}
		_MySession.Sock = INVALID_SOCKET;
	}


	// 만약 SendPost에서 송신 패킷을 DeQ 하였는데 WSASend에 실패하는 경우
	// 이곳에서 송신 패킷 메모리 Free
	for (int cnt = 0; cnt < _MySession.SendRqstNum; cnt++)
	{
		CPacket::Free(_MySession.SendPacket[cnt]);
	}

	// 아직 DeQ 되지 않은 송신 패킷 DeQ 및 메모리 Free
	CPacket* freePacket;
	while (_MySession.SendQ.Dequeue(freePacket))
	{
		CPacket::Free(freePacket);
	}

	_MySession.UseFlag = false;

	OnLeaveServer();

	return;
}

//////////////////////////////////////////////////////////////////////////
// 전송할 패킷을 SendQ 에 인큐
//
// Parameters: (ULONG64)세션 ID, (CPacket*)전송 패킷 직렬화버퍼 주소
// Return: bool (true)성공, (false)실패
//////////////////////////////////////////////////////////////////////////
void CLanClient::SendPacket(CPacket* packet)
{
	if ((true == _MySession.IsDisconn) || (false == _MySession.UseFlag))
		return;

	// IO Count 증가 및 Release 플래그 확인
	if (InterlockedIncrement64(&_MySession.ReleaseFlagAndIOCount) & dfRELEASE_FLAG_MASKING)
		return;

	// SendQ EnQ
	if (packet->GetDataSize())
	{
		packet->addRef();

		st_LanHeader lanHeader;
		lanHeader.Len = packet->GetDataSize();
		packet->PutLANHeader((char*)&lanHeader, sizeof(st_LanHeader));

		_MySession.SendQ.Enqueue(packet);
	}

	// 패킷 전송
	SendPost();

	// IO Count 감소
	if (0 == InterlockedDecrement64(&_MySession.ReleaseFlagAndIOCount))
		Release();

	return;
}

//////////////////////////////////////////////////////////////////////////
// 세션 생성
//
// Parameters: 없음
// Return: (true) 세션 생성 성공, (false) 세션 생성 실패
//////////////////////////////////////////////////////////////////////////
bool CLanClient::Connect(void)
{
	if (true == _MySession.UseFlag)
	{
		Disconnect();
		return false;
	}

	_MySession.Sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _MySession.Sock)
	{
		OnError(WSAGetLastError(), L"[Net_Option] socket error\n");
		return false;
	}

	ZeroMemory(&_MySession.SockAddr, sizeof(_MySession.SockAddr));
	_MySession.SockAddr.sin_family = AF_INET;
	InetPtonW(AF_INET, _wchServerIP, &_MySession.SockAddr.sin_addr);	// L"0.0.0.0" => INADDR_ANY
	_MySession.SockAddr.sin_port = htons((u_short)(_dwServerPort));

	// 소켓 옵션 - 링거 옵션 Off
	LINGER lig;
	lig.l_onoff = 1;
	lig.l_linger = 0;

	int retval = setsockopt(_MySession.Sock, SOL_SOCKET, SO_LINGER, (const char*)&lig, sizeof(lig));
	if (SOCKET_ERROR == retval)
	{
		OnError(WSAGetLastError(), L"[Net_Option] setsockopt Linger error\n");
		return false;
	}

	// 소켓 옵션 - 소켓 송신 버퍼 0 설정
	DWORD sndSockBuffer = 0;
	retval = setsockopt(_MySession.Sock, SOL_SOCKET, SO_SNDBUF, (const char*)&sndSockBuffer, sizeof(sndSockBuffer));
	if (SOCKET_ERROR == retval)
	{
		OnError(WSAGetLastError(), L"[Net_Option] setsockopt Send Buffer error\n");
		return false;
	}

	// 소켓 옵션 - 네이글 알고리즘 Off
	DWORD delayOpt = !_bNagleOpt;
	retval = setsockopt(_MySession.Sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&delayOpt, sizeof(delayOpt));
	if (SOCKET_ERROR == retval)
	{
		OnError(WSAGetLastError(), L"[Net_Option] setsockopt Nagle error\n");
		return false;
	}

	int retConnect = connect(_MySession.Sock, (SOCKADDR*)&_MySession.SockAddr, sizeof(SOCKADDR_IN));
	if (SOCKET_ERROR == retConnect)
	{
		OnError(WSAGetLastError(), L"[Net_MakeSession] connect error\n");
		closesocket(_MySession.Sock);
		return false;
	}

	// 세션 초기화 및 생성
	_MySession.ReleaseFlagAndIOCount = 0;
	_MySession.IsDisconn = false;
	_MySession.RecvQ.ClearBuffer();

	ZeroMemory(&_MySession.SendOverlapped, sizeof(_MySession.SendOverlapped));
	ZeroMemory(&_MySession.RecvOverlapped, sizeof(_MySession.RecvOverlapped));

	_MySession.SendRqstNum = 0;
	_MySession.UseFlag = true;
	InterlockedExchange64(&_MySession.SendFlag, true);

	// IOCP에 소켓 등록
	CreateIoCompletionPort((HANDLE)_MySession.Sock, _hIOCP, (ULONG64)&_MySession, 0);

	RecvPost();

	// 사용자에게 새로운 세션 알림
	OnEnterJoinServer();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// IO 워커 스레드
//
// Parameters: (void*)CLanClient 객체 주소
// Return: 
//////////////////////////////////////////////////////////////////////////
unsigned int __stdcall CLanClient::WorkerThread(void* pvServerObj)
{
	printf("Net_Worker Start\n");

	CLanClient* pServer = (CLanClient*)pvServerObj;
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
			pServer->Disconnect();
		}
		else
		{
			// IO 작업에 대한 송수신 바이트 개수가 0보다 큰 경우, 다음과 같은 상황이다.
			// 송신: (요청한 크기가 반환된 경우)송신 성공, (요청한 크기보다 작은 값이 반환된 경우)IO 작업 중 세션 종료됨
			// 수신: 수신 성공
			if (true == pOverlappedData->bIsSend)
			{
				int iSendSize = 0;

				//송신 완료 통지 처리
				for (int cnt = 0; cnt < pSession->SendRqstNum; cnt++)
				{
					iSendSize += pSession->SendPacket[cnt]->GetDataSize();

					CPacket::Free(pSession->SendPacket[cnt]);
				}
				pSession->SendRqstNum = 0;

				InterlockedExchange64(&pSession->SendFlag, true);

				pServer->OnSend(iSendSize);

				if (pSession->SendQ.GetUseSize() > 0)
					pServer->SendPacket(pEmptyPacket);
			}
			else
			{
				// 수신 완료 통지 처리
				pSession->RecvQ.MoveRear(dwTransferredBytes);

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

					pServer->OnRecv(pRecvPacket);
					
					CPacket::Free(pRecvPacket);
				}
				pServer->RecvPost();
			}
		}
		// IO count 차감 - 완료 통지 처리에 대한 차감
		if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
			pServer->Release();
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
void CLanClient::SendPost(void)
{
	// 송신 플래그 갱신
	if (false == InterlockedExchange64(&_MySession.SendFlag, false))
		return;
	
	if (0 == _MySession.SendQ.Dequeue(_MySession.SendPacket[0]))
	{
		InterlockedExchange64(&_MySession.SendFlag, true);
		return;
	}
	
	// 전송할 패킷 직렬화 버퍼 꺼내기
	int deqNum = 1;
	while (_MySession.SendQ.Dequeue(_MySession.SendPacket[deqNum]))
	{
		deqNum++;
		if (deqNum >= dfLANSEND_PAKCET_NUMBER)
			break;
	}

	// WSABUF 등록
	WSABUF wsaSendBuf[dfLANSEND_PAKCET_NUMBER];

	for (int cnt = 0; cnt < deqNum; cnt++)
	{
		wsaSendBuf[cnt].len = _MySession.SendPacket[cnt]->GetDataSizeWithLANHeader();
		wsaSendBuf[cnt].buf = _MySession.SendPacket[cnt]->GetLANHeaderPtr();
	}

	// WSASend 호출 전에 IO count를 먼저 증가시키는 이유
	// WSASend가 반환되기도 전에 해당 송신 완료 통지 처리로 인해 IO count가 0이 될 수 있다.
	_MySession.SendRqstNum = deqNum;
	_MySession.SendOverlapped.bIsSend = true;
	InterlockedIncrement64(&_MySession.ReleaseFlagAndIOCount);

	int sendRet = WSASend(
		_MySession.Sock, wsaSendBuf, deqNum, NULL,	// 정상 작동시 &dwRecvBytes 추가하여 실행해보기
		0, (OVERLAPPED*)&_MySession.SendOverlapped, NULL
	);

	if (SOCKET_ERROR == sendRet)
	{
		int err = WSAGetLastError();
		if (WSA_IO_PENDING != err)
		{
			if (WSAECONNRESET != err)
				OnError(err, L"Socket Send error\n");

			if (0 == InterlockedDecrement64(&_MySession.ReleaseFlagAndIOCount))
				Release();
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
void CLanClient::RecvPost(void)
{
	if (true == _MySession.IsDisconn)
		return;

	DWORD recvFlag = 0;
	WSABUF wsaRecvBuf[2];

	// 버퍼 등록
	wsaRecvBuf[0].len = _MySession.RecvQ.DirectEnqueueSize();
	wsaRecvBuf[0].buf = _MySession.RecvQ.GetRearBufferPtr();
	wsaRecvBuf[1].len = _MySession.RecvQ.GetFreeSize() - _MySession.RecvQ.DirectEnqueueSize();
	wsaRecvBuf[1].buf = _MySession.RecvQ.GetBufferPtr();

	_MySession.RecvOverlapped.bIsSend = false;

	// WSARecv 호출 전에 IO count를 먼저 증가시키는 이유
	// WSARecv가 반환되기도 전에 해당 송신 완료 통지 처리 루틴으로 인해 IO count가 0이 될 수 있다.
	InterlockedIncrement64(&_MySession.ReleaseFlagAndIOCount);

	int recvRet = WSARecv(
		_MySession.Sock, wsaRecvBuf, 2, NULL,	// 정상 작동시 &dwRecvBytes 추가하여 실행해보기
		&recvFlag, (OVERLAPPED*)&_MySession.RecvOverlapped, NULL
	);

	if (SOCKET_ERROR == recvRet)
	{
		int err = WSAGetLastError();
		if (WSA_IO_PENDING != err)
		{
			if (WSAECONNRESET != err)
				OnError(err, L"Socket Recv error\n");

			if (0 == InterlockedDecrement64(&_MySession.ReleaseFlagAndIOCount))
				Release();
			return;
		}
	}

	return;
}