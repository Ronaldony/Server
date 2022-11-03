#ifndef __LOGIN_SERVER_H__
#define __LOGIN_SERVER_H__

//#include <unordered_map>
//#include <Pdh.h>
#include "CNetServer.h"
#include "LoginServerPool.h"
#include "LockFreeQueue.h"
#include "CDBConnectorTLS.h"
#include "MonitorClient.h"
#include "CProcessMonitoring.h"

#define dfBYTES_MEGA	1000000
#define dfBYTES_KILLO	1000

// ä�� ���� - �̱� ������
class LoginServer : public CNetServer 
{
public:
	enum en_USERCHECK
	{
		en_CHECK_TIMEOUT = 2000,
		en_CHECK_CHATUSER = 200
	};

	struct stMonitoringInfo
	{
		LONG64 alignas(64)	NowLoginUserNum;	// ���� �α��� ������
		LONG64 alignas(64)	AuthCountTPS;		// �������� TPS
	};

	LoginServer();
	~LoginServer();

	// ����͸� ����
	void MonitoringOutput(void);

private:
	/* CNetServer �Լ� ���� */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 uldSessionID);
	void OnClientLeave(ULONG64 uldSessionID);

	void OnRecv(ULONG64 uldSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	static unsigned int __stdcall Proc_TimeCheck(void* pvParam);

	void PacketProc_LoginUser(ULONG64 uldSessionID, CPacket* pPacket);
	void mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status, WCHAR* ID, WCHAR* NickName,
		WCHAR* GameServerIP, USHORT GameServerPort, WCHAR* ChatServerIP, USHORT ChatServerPort);	// �α��� ���� �޽��� ����

	bool AuthenticateUser(ULONG64 uldSessionID, INT64 AccountNo, char* SessionKey);			// ���� ����
	bool AddUserInfo(ULONG64 uldSessionID, INT64 AccountNo, WCHAR* ID, WCHAR* NickName);	// ���� ����Ʈ�� ���ο� ���� ���� �߰�

	bool UTF8ToUTF16(WCHAR* dstStr, int dstSize, char* srcStr, int srcSize);

	void CheckUserTimeout(void);		// ���� Ÿ�Ӿƿ� �˻�
	
	void AddUser(ULONG64 uldSessionID, st_USER_INFO* pUser);	// ���� ���� �߰�
	st_USER_INFO* FindUser(ULONG64 uldSessionID);				// ���� ���� �˻�
	void DeleteUser(ULONG64 uldSessionID);						// ���� ���� ����

	void AddAccount(ULONG64 uldAccount, ULONG64 uldSessionID);		// Account ��ȣ �߰�
	ULONG64 FindAccount(ULONG64 uldAccount);						// Account ��ȣ �˻�
	void DeleteAccount(ULONG64 uldAccount, ULONG64 uldSessionID);	// Account ��ȣ ����

	void ServerControl(void);	// ����͸� ���� Ÿ��

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// ���� ���� �� ä�� ���� IP, Port
	WCHAR		_wchGameSrvIP[16];
	DWORD		_dwGameSrvPort;
	WCHAR		_wchChatSrvIP[16];
	DWORD		_dwChatSrvPort;

	// ���� ���� ���� �����̳�
	// Key: SessionID, Val: st_USER_INFO*
	std::unordered_map<ULONG64, st_USER_INFO*>	_UserMap;
	SRWLOCK							_UserMapLock;

	// Account ��ȣ ���� �����̳�
	// Key: Account ��ȣ, Val: ���� ID
	std::unordered_multimap<ULONG64, ULONG64>	_AccountMap;
	SRWLOCK			_AccountMapLock;

	DWORD	_dwTimeoutValue;	// ���� Ÿ�Ӿƿ� �ð�
	DWORD	_dwTimeOutTick;		// Ÿ�Ӿƿ� ���ν��� Tick

	// �α� ����
	DWORD		_dwLogLevel;		// �α� ����
	bool		_bSaveLog;			// �α� ���� ���� ����
	WCHAR		_LogFileName[_MAX_PATH];

	HANDLE	_hTimeoutThread;		// ������ �ڵ� �迭
	DWORD	_dwTimeoutThreadID;		// ������ ID

	DWORD	_dwMaxLoginUserNum;		// �ִ� �α��� ������

	stMonitoringInfo	_OngoingMonitoring;		// ��� ���� ����͸� ����
	stMonitoringInfo	_ResultMonitoring;		// ��� ����͸� ����

	CDBConnectorTLS*		_pDBConnector;		// DB Ŀ����

	CProcessMonitoring		_ProcessMonitor;	// ���μ��� ����� ���
	MonitorClient			_MonitorClient;		// ����͸� ������ Ŭ���̾�Ʈ

	cpp_redis::client		_RedisClient;		// Redis Ŭ���̾�Ʈ
};

#endif