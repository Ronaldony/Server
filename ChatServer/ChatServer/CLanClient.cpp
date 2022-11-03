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
// ���� ���� �� Ȱ��ȭ
// 
// Parameter: (DWORD)IO ��Ŀ ������ ���� ����, (DWORD)IO ��Ŀ ������ ���� ���� ����, (const WCHAR*)���� IP, (DWORD)���� ��Ʈ, (DWORD)���� ���� ����, 
// (BYTE)��Ŷ �ڵ�, (BYTE)��Ŷ ���� Ű, (bool)���̱� �ɼ�
// Return: bool (true)Ȱ��ȭ ����, (false)Ȱ��ȭ ����
////////////////////////////////////////////////////////////////////////
bool CLanClient::Start(int dwWorkerThradNum, DWORD dwActiveThreadNum, const WCHAR* pwchIP, DWORD dwPort, bool bNagleOpt)
{
	// ���̱� �ɼ� 
	_bNagleOpt = bNagleOpt;

	_dwServerPort = dwPort;
	errno_t err = wcscpy_s(_wchServerIP, pwchIP);
	if (0 != err)
	{
		OnError(0, L"IP wcscpy_s error\n");
		return false;
	}

	// ������ �ʱ�ȭ
	WSAData wsa;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
	{
		OnError(WSAGetLastError(), L"[Net_Option] WSAStartup error\n");
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
		ULONG64 masking = 1;
		SetThreadAffinityMask(_phThreads[cnt], masking << cnt);
		ResumeThread(_phThreads[cnt]);
	}

	_MySession.UseFlag = false;

	return true;
}

////////////////////////////////////////////////////////////////////////
// ���� ����
// 
// Parameter: ����
// Return: ����
////////////////////////////////////////////////////////////////////////
void CLanClient::Stop(void)
{
	// TODO: Accept �� ��Ŀ ������ ���� �ڵ�(PQCS)
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ����
// 
// Parameter: (ULONG64)���� ID
// Return: bool (true)���� ����, (false)���� ����
////////////////////////////////////////////////////////////////////////
bool CLanClient::Disconnect(void)
{
	// IO Count ���� �� Release �÷��� Ȯ��
	if (InterlockedIncrement64(&_MySession.ReleaseFlagAndIOCount) & dfRELEASE_FLAG_MASKING)
		return false;

	_MySession.IsDisconn = true;
	
	// IO ���
	CancelIoEx((HANDLE)(_MySession.Sock), NULL);


	// IO Count ����
	if (0 == InterlockedDecrement64(&_MySession.ReleaseFlagAndIOCount))
		Release();

	return true;
}

////////////////////////////////////////////////////////////////////////
// ���� ���� ���� �� �ڿ� ��ȯ
// 
// Parameter: (st_LAN_SESSION*)���� �ּ�
// Return: bool (true)����, (false)����
////////////////////////////////////////////////////////////////////////
void CLanClient::Release(void)
{
	// �ܺο��� SendPacket Ȥ�� Disconnect�� ��������� Ȯ���ϱ� ���� ����
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


	// ���� SendPost���� �۽� ��Ŷ�� DeQ �Ͽ��µ� WSASend�� �����ϴ� ���
	// �̰����� �۽� ��Ŷ �޸� Free
	for (int cnt = 0; cnt < _MySession.SendRqstNum; cnt++)
	{
		CPacket::Free(_MySession.SendPacket[cnt]);
	}

	// ���� DeQ ���� ���� �۽� ��Ŷ DeQ �� �޸� Free
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
// ������ ��Ŷ�� SendQ �� ��ť
//
// Parameters: (ULONG64)���� ID, (CPacket*)���� ��Ŷ ����ȭ���� �ּ�
// Return: bool (true)����, (false)����
//////////////////////////////////////////////////////////////////////////
void CLanClient::SendPacket(CPacket* packet)
{
	if ((true == _MySession.IsDisconn) || (false == _MySession.UseFlag))
		return;

	// IO Count ���� �� Release �÷��� Ȯ��
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

	// ��Ŷ ����
	SendPost();

	// IO Count ����
	if (0 == InterlockedDecrement64(&_MySession.ReleaseFlagAndIOCount))
		Release();

	return;
}

//////////////////////////////////////////////////////////////////////////
// ���� ����
//
// Parameters: ����
// Return: (true) ���� ���� ����, (false) ���� ���� ����
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

	// ���� �ɼ� - ���� �ɼ� Off
	LINGER lig;
	lig.l_onoff = 1;
	lig.l_linger = 0;

	int retval = setsockopt(_MySession.Sock, SOL_SOCKET, SO_LINGER, (const char*)&lig, sizeof(lig));
	if (SOCKET_ERROR == retval)
	{
		OnError(WSAGetLastError(), L"[Net_Option] setsockopt Linger error\n");
		return false;
	}

	// ���� �ɼ� - ���� �۽� ���� 0 ����
	DWORD sndSockBuffer = 0;
	retval = setsockopt(_MySession.Sock, SOL_SOCKET, SO_SNDBUF, (const char*)&sndSockBuffer, sizeof(sndSockBuffer));
	if (SOCKET_ERROR == retval)
	{
		OnError(WSAGetLastError(), L"[Net_Option] setsockopt Send Buffer error\n");
		return false;
	}

	// ���� �ɼ� - ���̱� �˰��� Off
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

	// ���� �ʱ�ȭ �� ����
	_MySession.ReleaseFlagAndIOCount = 0;
	_MySession.IsDisconn = false;
	_MySession.RecvQ.ClearBuffer();

	ZeroMemory(&_MySession.SendOverlapped, sizeof(_MySession.SendOverlapped));
	ZeroMemory(&_MySession.RecvOverlapped, sizeof(_MySession.RecvOverlapped));

	_MySession.SendRqstNum = 0;
	_MySession.UseFlag = true;
	InterlockedExchange64(&_MySession.SendFlag, true);

	// IOCP�� ���� ���
	CreateIoCompletionPort((HANDLE)_MySession.Sock, _hIOCP, (ULONG64)&_MySession, 0);

	RecvPost();

	// ����ڿ��� ���ο� ���� �˸�
	OnEnterJoinServer();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// IO ��Ŀ ������
//
// Parameters: (void*)CLanClient ��ü �ּ�
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
			// ��Ȳ1: IOCP ��ť�� ���� => IOCP ��ü ����
			// ��Ȳ2: �ܺο��� PQCS�� ���� �ǵ������� Worker �����带 �����ϱ� ����
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
			// IO �۾��� ���� �ۼ��� ����Ʈ ������ 0���� ū ���, ������ ���� ��Ȳ�̴�.
			// �۽�: (��û�� ũ�Ⱑ ��ȯ�� ���)�۽� ����, (��û�� ũ�⺸�� ���� ���� ��ȯ�� ���)IO �۾� �� ���� �����
			// ����: ���� ����
			if (true == pOverlappedData->bIsSend)
			{
				int iSendSize = 0;

				//�۽� �Ϸ� ���� ó��
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
				// ���� �Ϸ� ���� ó��
				pSession->RecvQ.MoveRear(dwTransferredBytes);

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

					pServer->OnRecv(pRecvPacket);
					
					CPacket::Free(pRecvPacket);
				}
				pServer->RecvPost();
			}
		}
		// IO count ���� - �Ϸ� ���� ó���� ���� ����
		if (0 == InterlockedDecrement64(&pSession->ReleaseFlagAndIOCount))
			pServer->Release();
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
void CLanClient::SendPost(void)
{
	// �۽� �÷��� ����
	if (false == InterlockedExchange64(&_MySession.SendFlag, false))
		return;
	
	if (0 == _MySession.SendQ.Dequeue(_MySession.SendPacket[0]))
	{
		InterlockedExchange64(&_MySession.SendFlag, true);
		return;
	}
	
	// ������ ��Ŷ ����ȭ ���� ������
	int deqNum = 1;
	while (_MySession.SendQ.Dequeue(_MySession.SendPacket[deqNum]))
	{
		deqNum++;
		if (deqNum >= dfLANSEND_PAKCET_NUMBER)
			break;
	}

	// WSABUF ���
	WSABUF wsaSendBuf[dfLANSEND_PAKCET_NUMBER];

	for (int cnt = 0; cnt < deqNum; cnt++)
	{
		wsaSendBuf[cnt].len = _MySession.SendPacket[cnt]->GetDataSizeWithLANHeader();
		wsaSendBuf[cnt].buf = _MySession.SendPacket[cnt]->GetLANHeaderPtr();
	}

	// WSASend ȣ�� ���� IO count�� ���� ������Ű�� ����
	// WSASend�� ��ȯ�Ǳ⵵ ���� �ش� �۽� �Ϸ� ���� ó���� ���� IO count�� 0�� �� �� �ִ�.
	_MySession.SendRqstNum = deqNum;
	_MySession.SendOverlapped.bIsSend = true;
	InterlockedIncrement64(&_MySession.ReleaseFlagAndIOCount);

	int sendRet = WSASend(
		_MySession.Sock, wsaSendBuf, deqNum, NULL,	// ���� �۵��� &dwRecvBytes �߰��Ͽ� �����غ���
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
// ���ſ� �����۸� WSARecv�� ���
//
// Parameters: (st_LAN_SESSION*)���� �ּ�
// Return: bool (true)��� ����, (false)��� ����
//////////////////////////////////////////////////////////////////////////
void CLanClient::RecvPost(void)
{
	if (true == _MySession.IsDisconn)
		return;

	DWORD recvFlag = 0;
	WSABUF wsaRecvBuf[2];

	// ���� ���
	wsaRecvBuf[0].len = _MySession.RecvQ.DirectEnqueueSize();
	wsaRecvBuf[0].buf = _MySession.RecvQ.GetRearBufferPtr();
	wsaRecvBuf[1].len = _MySession.RecvQ.GetFreeSize() - _MySession.RecvQ.DirectEnqueueSize();
	wsaRecvBuf[1].buf = _MySession.RecvQ.GetBufferPtr();

	_MySession.RecvOverlapped.bIsSend = false;

	// WSARecv ȣ�� ���� IO count�� ���� ������Ű�� ����
	// WSARecv�� ��ȯ�Ǳ⵵ ���� �ش� �۽� �Ϸ� ���� ó�� ��ƾ���� ���� IO count�� 0�� �� �� �ִ�.
	InterlockedIncrement64(&_MySession.ReleaseFlagAndIOCount);

	int recvRet = WSARecv(
		_MySession.Sock, wsaRecvBuf, 2, NULL,	// ���� �۵��� &dwRecvBytes �߰��Ͽ� �����غ���
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