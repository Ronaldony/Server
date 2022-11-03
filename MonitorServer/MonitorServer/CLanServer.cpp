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
// ���� ���� �� Ȱ��ȭ
// 
// Parameter: (DWORD)IO ��Ŀ ������ ���� ����, (DWORD)IO ��Ŀ ������ ���� ���� ����, (const WCHAR*)���� ���ε� IP, (DWORD)���� ��Ʈ, (DWORD)���� ���� ����, 
// (BYTE)��Ŷ �ڵ�, (BYTE)��Ŷ ���� Ű, (bool)���̱� �ɼ�
// Return: bool (true)Ȱ��ȭ ����, (false)Ȱ��ȭ ����
////////////////////////////////////////////////////////////////////////
bool CLanServer::Start(int dwWorkerThradNum, DWORD dwActiveThreadNum, const WCHAR* pwchIP, DWORD dwPort, DWORD dwMaxSessionNum, bool bNagleOpt)
{
	// ���̱� �ɼ� 
	_bNagleOpt = bNagleOpt;

	// ���Ǽ� �ִ� ũ�� �˻�
	if (dfSESSION_INDEX_MASKING < dwMaxSessionNum)
	{
		OnError(0, L"[Start] The creation number for Session exceeds limitation\n");
		return false;
	}

	// ��� ���� �ʱ�ȭ
	_dwMaxSession = dwMaxSessionNum;
	_pSessionArr = (st_LAN_SESSION*)malloc(sizeof(st_LAN_SESSION) * dwMaxSessionNum);

	for (int cnt = 0; cnt < (int)dwMaxSessionNum; cnt++)
	{
		_pSessionArr[cnt].IsInit = false;
	}

	// �ʱ� ��� ���� �޸� �Է�
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

	// ������ �ʱ�ȭ
	WSAData wsa;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
	{
		OnError(WSAGetLastError(), L"WSAStartup error\n");
		return false;
	}

	// IOCP ����
	_dwActiveThreadNum = dwActiveThreadNum;
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, dwActiveThreadNum);
	if (NULL == _hIOCP)
	{
		OnError(WSAGetLastError(), L"CreateIoCompletionPort error\n");
		return false;
	}
	
	// ������ ����
	_dwWorkderThreadNum = dwWorkerThradNum;

	_phThreads = new HANDLE[dwWorkerThradNum + 2];		// +1�� Accept ������ ����
	_pdwThreadsID = new DWORD[dwWorkerThradNum + 2];

	// IO ��Ŀ ������
	for (int cnt = 0; cnt < (int)dwWorkerThradNum; cnt++)
	{
		// Worker ������
		_phThreads[cnt] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread,
			(void*)this, CREATE_SUSPENDED, (unsigned int*)&_pdwThreadsID[cnt]);
		if (_phThreads[cnt] == 0 || _phThreads[cnt] == (HANDLE)(-1))
		{
			OnError(WSAGetLastError(), L"_beginthreadex error\n");
			return false;
		}

		// CPU ��ȣ�� ����
		ULONG64 masking = 1;
		SetThreadAffinityMask(_phThreads[cnt], masking << cnt);
		ResumeThread(_phThreads[cnt]);
	}

	// Accept ������
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
// ���� ����
// 
// Parameter: ����
// Return: ����
////////////////////////////////////////////////////////////////////////
void CLanServer::Stop(void)
{
	// TODO: Accept �� ��Ŀ ������ ���� �ڵ�(PQCS)
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ����
// 
// Parameter: (ULONG64)���� ID
// Return: bool (true)���� ����, (false)���� ����
////////////////////////////////////////////////////////////////////////
bool CLanServer::Disconnect(ULONG64 udlSessionID)
{
	// ���� �˻�
	if ((udlSessionID & dfSESSION_INDEX_MASKING) >= _dwMaxSession)
		return false;

	st_LAN_SESSION* pSession = &_pSessionArr[udlSessionID & dfSESSION_INDEX_MASKING];
	if (udlSessionID != pSession->SessionID)
		return false;

	// IO Count ���� �� Release �÷��� Ȯ��
	if (InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount) & dfRELEASE_FLAG_MASKING)
		return false;

	// ���� �ش� ������ ��Ȱ��� ����� IO Count�� �ٽ� �����ؾ� �Ѵ�.
	// ���� �������� ������ ��Ȱ��� ������ Release ���� ���� ���̴�.
	if (udlSessionID != pSession->SessionID)
	{
		if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
			Release(pSession);
		return false;
	}

	pSession->IsDisconn = true;
	// IO ���
	CancelIoEx((HANDLE)(pSession->Sock), NULL);

	// IO Count ����
	if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
		Release(pSession);

	return true;
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ���� �� �ڿ� ��ȯ
// 
// Parameter: (st_LAN_SESSION*)���� �ּ�
// Return: bool (true)����, (false)����
////////////////////////////////////////////////////////////////////////
void CLanServer::Release(st_LAN_SESSION* pSession)
{
	// �ܺο��� SendPacket Ȥ�� Disconnect�� ��������� Ȯ���ϱ� ���� ����
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

	// ���� SendPost���� �۽� ��Ŷ�� DeQ �Ͽ��µ� WSASend�� �����ϴ� ���
	// �̰����� �۽� ��Ŷ �޸� Free
	for (int cnt = 0; cnt < pSession->SendRqstNum; cnt++)
	{
		CPacket::Free(pSession->SendPacket[cnt]);
	}

	// ���� DeQ ���� ���� �۽� ��Ŷ DeQ �� �޸� Free
	CPacket* freePacket;
	while (pSession->SendQ.Dequeue(freePacket))
	{
		CPacket::Free(freePacket);
	}

	// ���� ���� �˸� �� ���� �ʱ�ȭ
	pSession->SessionID = 0xFFFFFFFFFFFFFFFF;
	pSession->UseFlag = false;
	_AvailableSessionStack.Push(pSession->SessionIndex);

	// ����� ����
#ifndef dfSERVER_MODULE_BENCHMARK
	InterlockedDecrement64(&_stMonitoringOnGoing.NowSessionNum);
	InterlockedIncrement64(&_stMonitoringOnGoing.DisconnectTPS);
	//InterlockedIncrement64(&_stMonitoringOnGoing.DisconnetTotal);
#endif

	return;
}

//////////////////////////////////////////////////////////////////////////
// ������ ��Ŷ�� SendQ �� ��ť
//
// Parameters: (ULONG64)���� ID, (CPacket*)���� ��Ŷ ����ȭ���� �ּ�
// Return: bool (true)����, (false)����
//////////////////////////////////////////////////////////////////////////
void CLanServer::SendPacket(ULONG64 SessionID, CPacket* packet)
{
	st_LAN_SESSION* pSession = FindSession(SessionID);
	if (NULL == pSession)
		return;

	// IO Count ���� �� Release �÷��� Ȯ��
	if (InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount) & dfRELEASE_FLAG_MASKING)
		return;

	// ���� �ش� ������ �̹� ��Ȱ��� ����� IO Count�� �ٽ� �����ؾ� �Ѵ�.
	// ���� �������� ������ ��Ȱ��� ������ Release ���� ���� ���̴�.
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

	// ��Ŷ ����
	SendPost(pSession);

	// IO Count ����
	if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
		Release(pSession);

	return;
}

//////////////////////////////////////////////////////////////////////////
// ���� Accept
//
// Parameters: (void*)CLanServer ��ü �ּ�
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
 
	// ���� �ɼ� - ���� �ɼ� Off
	LINGER lig;
	lig.l_onoff = 1;
	lig.l_linger = 0;

	retval = setsockopt(listenSock, SOL_SOCKET, SO_LINGER, (const char*)&lig, sizeof(lig));
	if (SOCKET_ERROR == retval)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] setsockopt Linger error\n");
		return 0;
	}

	// ���� �ɼ� - ���� �۽� ���� 0 ����
	DWORD sndSockBuffer = 0;
	retval = setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (const char*)&sndSockBuffer, sizeof(sndSockBuffer));
	if (SOCKET_ERROR == retval)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] setsockopt Nagle error\n");
		return 0;
	}

	// ���� �ɼ� - ���̱� �˰��� Off
	DWORD delayOpt = !pServer->_bNagleOpt;
	retval = setsockopt(listenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&delayOpt, sizeof(delayOpt));
	if (SOCKET_ERROR == retval)
	{
		pServer->OnError(WSAGetLastError(), L"[Accept] setsockopt Nagle error\n");
		return 0;
	}

	// Listen ����
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
		// ���ο� ����
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

		// White IP Ȯ��
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
		// ���� �ʱ�ȭ �� ����
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
		// printf("NetWorkLIB - Ŭ���̾�Ʈ ����: IP �ּ�=%s / ��Ʈ ��ȣ=%d\n",inet_ntoa(pSession->SockAddr.sin_addr), ntohs(pSession->SockAddr.sin_port));

		// IOCP�� ���� ���
		CreateIoCompletionPort((HANDLE)newConnect, pServer->_hIOCP, (ULONG64)pSession, 0);

		pServer->RecvPost(pSession);

		// ����ڿ��� ���ο� ���� �˸�
		pServer->OnClientJoin(pSession->SessionID);

		// ���� ID ����
		ullSessionIDCount += dfSESSION_INDEX_MASKING + 1;
		if (0 == ullSessionIDCount)
		{
			ullSessionIDCount = dfSESSION_INDEX_MASKING + 1;
		}

#ifndef dfSERVER_MODULE_BENCHMARK
		// ����� ����
		InterlockedIncrement64(&pServer->_stMonitoringOnGoing.AcceptTPS);
		//InterlockedIncrement64(&pServer->_stMonitoringOnGoing.AcceptTotal);
		InterlockedIncrement64(&pServer->_stMonitoringOnGoing.NowSessionNum);
#endif
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// IO ��Ŀ ������
//
// Parameters: (void*)CLanServer ��ü �ּ�
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
			// ��Ȳ1: IOCP ��ť�� ���� => IOCP ��ü ����
			// ��Ȳ2: �ܺο��� PQCS�� ���� �ǵ������� Worker �����带 �����ϱ� ����
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
			// IO �۾��� ���� �ۼ��� ����Ʈ ������ 0���� ū ���, ������ ���� ��Ȳ�̴�.
			// �۽�: (��û�� ũ�Ⱑ ��ȯ�� ���)�۽� ����, (��û�� ũ�⺸�� ���� ���� ��ȯ�� ���)IO �۾� �� ���� �����
			// ����: ���� ����
			if (true == pOverlappedData->bIsSend)
			{
#ifndef dfSERVER_MODULE_BENCHMARK
				int iTotalSendBytes = 0;
#endif

				//�۽� �Ϸ� ���� ó��
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
				// ����� ����
				InterlockedAdd64(&pServer->_stMonitoringOnGoing.SendBPS, iTotalSendBytes + 40);
#endif
				if (pSession->SendQ.GetUseSize() > 0)
					pServer->SendPacket(pSession->SessionID, pEmptyPacket);
			}
			else
			{
				// ���� �Ϸ� ���� ó��
				pSession->RecvQ.MoveRear(dwTransferredBytes);

#ifndef dfSERVER_MODULE_BENCHMARK
				// ����� ����
				InterlockedAdd64(&pServer->_stMonitoringOnGoing.RecvBPS, dwTransferredBytes + 40);	// + 40(IP, TCP ���)
#endif

				while (1)
				{
					// ��Ʈ��ũ �޽��� ��� �� ���̷ε� ����
					st_LanHeader lanHeader;

					if (pSession->RecvQ.GetUseSize() < sizeof(st_LanHeader))
						break;

					pSession->RecvQ.Peek((char*)&lanHeader, sizeof(st_LanHeader));

					if (pSession->RecvQ.GetUseSize() < (LONG64)(sizeof(st_LanHeader) + lanHeader.Len))
						break;

					pSession->RecvQ.MoveFront(sizeof(st_LanHeader));

					// ���� ũ�� ��üũ �� �Ʒ� RecvQ.Dequeue ��ɿ��� ����ȭ
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
		// IO count ���� - �Ϸ� ���� ó���� ���� ����
		if (0 == (InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount) & dfSESSION_IOCOUNT_MASKING))
			pServer->Release(pSession);
	}
	CPacket::Free(pEmptyPacket);
	 
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// SendQ�� �������Ŷ�� WSASend�� ���
//
// Parameters: (st_LAN_SESSION*)���� �ּ�
// Return: bool (true)��� ����, (false)��� ����
//////////////////////////////////////////////////////////////////////////
void CLanServer::SendPost(st_LAN_SESSION* pSession)
{
	// �۽� �÷��� ����
	if (false == InterlockedExchange64(&pSession->SendFlag, false))
		return;
	
	if (0 == pSession->SendQ.Dequeue(pSession->SendPacket[0]))
	{
		InterlockedExchange64(&pSession->SendFlag, true);
		return;
	}
	
	// ������ ��Ŷ ����ȭ ���� ������
	int deqNum = 1;
	while (pSession->SendQ.Dequeue(pSession->SendPacket[deqNum]))
	{
		deqNum++;
		if (deqNum >= dfSEND_NET_PAKCET_SIZE)
			break;
	}

	// WSABUF ���
	WSABUF wsaSendBuf[dfSEND_NET_PAKCET_SIZE];

	for (int cnt = 0; cnt < deqNum; cnt++)
	{
		wsaSendBuf[cnt].len = pSession->SendPacket[cnt]->GetDataSizeWithLANHeader();
		wsaSendBuf[cnt].buf = pSession->SendPacket[cnt]->GetLANHeaderPtr();
	}

	// WSASend ȣ�� ���� IO count�� ���� ������Ű�� ����
	// WSASend�� ��ȯ�Ǳ⵵ ���� �ش� �۽� �Ϸ� ���� ó���� ���� IO count�� 0�� �� �� �ִ�.
	pSession->SendRqstNum = deqNum;
	pSession->SendOverlapped.bIsSend = true;
	InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount);

	int sendRet = WSASend(
		pSession->Sock, wsaSendBuf, deqNum, NULL,	// ���� �۵��� &dwRecvBytes �߰��Ͽ� �����غ���
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
// ���ſ� �����۸� WSARecv�� ���
//
// Parameters: (st_LAN_SESSION*)���� �ּ�
// Return: bool (true)��� ����, (false)��� ����
//////////////////////////////////////////////////////////////////////////
void CLanServer::RecvPost(st_LAN_SESSION* pSession)
{
	if (true == pSession->IsDisconn)
		return;

	DWORD recvFlag = 0;
	WSABUF wsaRecvBuf[2];

	// ���� ���
	wsaRecvBuf[0].len = pSession->RecvQ.DirectEnqueueSize();
	wsaRecvBuf[0].buf = pSession->RecvQ.GetRearBufferPtr();
	wsaRecvBuf[1].len = pSession->RecvQ.GetFreeSize() - pSession->RecvQ.DirectEnqueueSize();
	wsaRecvBuf[1].buf = pSession->RecvQ.GetBufferPtr();

	pSession->RecvOverlapped.bIsSend = false;

	// WSARecv ȣ�� ���� IO count�� ���� ������Ű�� ����
	// WSARecv�� ��ȯ�Ǳ⵵ ���� �ش� �۽� �Ϸ� ���� ó�� ��ƾ���� ���� IO count�� 0�� �� �� �ִ�.
	InterlockedIncrement64(&pSession->ReleaseFlagAndIOCount);

	int recvRet = WSARecv(
		pSession->Sock, wsaRecvBuf, 2, NULL,	// ���� �۵��� &dwRecvBytes �߰��Ͽ� �����غ���
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
// ��� ������ ���� �迭 �ε��� ��ȯ
//
// Parameters: ����
// Return: int (���)�ε��� ��ȣ, (-1)��� ������ �ε��� ����
//////////////////////////////////////////////////////////////////////////
int CLanServer::FindAvailableSession(void)
{
	// ���� ���
	// ���� �� ��ȸ ����� ���� ��
	int index;
	if (0 == _AvailableSessionStack.Pop(index))
		return (-1);

	return index;
}

//////////////////////////////////////////////////////////////////////////
// ���� ID�� �ش��ϴ� ���� �ּ� ��ȯ
//
// Parameters: (ULONG64)���� ID
// Return: st_LAN_SESSION* (!NULL)���� �ּ�, (NULL)ID�� �ش��ϴ� ������ �̹� ����ų� ����
//////////////////////////////////////////////////////////////////////////
st_LAN_SESSION* CLanServer::FindSession(ULONG64 SessionID)
{
	// ���� �迭 �ε��� �ʰ�
	if ((SessionID & dfSESSION_INDEX_MASKING) >= _dwMaxSession)
		return NULL;

	st_LAN_SESSION* pSession = &_pSessionArr[SessionID & dfSESSION_INDEX_MASKING];

	if ((pSession->SessionID != SessionID) || (true == pSession->IsDisconn))
		return NULL;
	else
		return pSession;
}

//////////////////////////////////////////////////////////////////////////
// �ܺη� ������ ����͸� ���� ����
//
// Parameters: (st_MonitoringInfo*)�ܺη� ������ ����͸� ���� �ּ�
// Return: ����
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
