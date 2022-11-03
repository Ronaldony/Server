#ifndef __CONTENTS_H__
#define __CONTENTS_H__
#include "CLanClient.h"

// ä�� ���� - �̱� ������
class MonitorClient : public CLanClient
{
public:
	enum en_USERCHECK
	{
		en_CHECK_TIMEOUT = 2000,
		en_CHECK_CHATUSER = 200
	};

	MonitorClient();
	~MonitorClient();

	void mpLoginReq(CPacket* pSendPacket, int ServerNo);										// �α��� ��û
	void mpUpdateMonitor(CPacket* pSendPacket, BYTE DataType, int DataValue, int TimeStamp);	// ������ ������Ʈ

private:
	/* CNetServer �Լ� ���� */
	void OnEnterJoinServer(void);
	void OnLeaveServer(void);

	void OnRecv(CPacket* pPacket);
	void OnSend(int sendsize);

	void OnError(int errorcode, const WCHAR* comment);

	static unsigned int __stdcall ConnectThread(void* pvParam);		// Connection ��û ������

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;
	
	// �α� ����
	DWORD		_dwLogLevel;		// �α� ����
	bool		_bSaveLog;			// �α� ���� ���� ����
	WCHAR		_LogFileName[_MAX_PATH];

	HANDLE		_hConnectThread;
	DWORD		_hConnectThreadID;
};

#endif
