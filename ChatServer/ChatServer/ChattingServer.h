#ifndef __CHATTING_SERVER_H__
#define __CHATTING_SERVER_H__

//#include <unordered_map>
//#include <unordered_set>
#include "CNetServer.h"
#include "ChattingServerDefine.h"
#include "ChattingServerPool.h"
#include "LockFreeQueue.h"
#include "MonitorClient.h"
#include "CProcessMonitoring.h"

// ä�� ���� - �̱� ������
class ChattingServer : public CNetServer
{
public:
	enum en_USER
	{
		en_CHECK_TIMEOUT = 300,
		en_CHECK_CHATUSER = 200
	};

	enum en_TIME_CHECK
	{
		en_SLEEP_TIME_CHECK_PERIOD = 100,			// Ÿ�� üũ ���� Ÿ��
		en_USER_TIME_OUT_PERIOD = 1000,				// ���� Ÿ�� �ƿ� ó�� �ֱ�
		en_MONITOR_TIME_PERIOD = 1000,				// ����� ���� ���� �ֱ�
	};

	struct stMonitorInfo
	{
		alignas(64) LONG64 TPS_ProcMSG;
		LONG64 TPS_ProcMSGMonitor;
		LONG64 TPSMAX_ProcMSG;
		LONG64 TPSMIN_ProcMSG;

		LONG64 Total_ProcMSG;
		LONG64 MonitorCount_ProcMSG;
	};

	ChattingServer();
	~ChattingServer();

	// ����͸� ����
	void MonitoringOutput(void);

private:
	/* CNetServer �Լ� ���� */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 udlSessionID);
	void OnClientLeave(ULONG64 udlSessionID);

	void OnRecv(ULONG64 udlSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	/* �� ������ */
	static unsigned int __stdcall Proc_TimeCheck(void* pvParam);	// ���� Ÿ�Ӿƿ� ���ν���

	/* ���ν��� */
	void PacketProc_LoginUser(ULONG64 udlSessionID, CPacket*);		// ���� �α��� ��û ó��
	void PacketProc_MoveSector(ULONG64 udlSessionID, CPacket*);		// ���� ���� �̵� ó��
	void PacketProc_Chatting(ULONG64 udlSessionID, CPacket*);		// ä�� ó��
	void PacketProc_HeartBeat(ULONG64 udlSessionID);				// ���� Ÿ�� �ƿ� ó��

	bool AuthenticateUser(ULONG64 udlSessionID, INT64 AccountNo, char* SessionKey);			// ���� ����
	bool AddUserInfo(ULONG64 udlSessionID, INT64 AccountNo, WCHAR* ID, WCHAR* NickName);	// ���� ����Ʈ�� ���ο� ���� ���� �߰�
	void CheckUserTimeout(void);		// ���� Ÿ�Ӿƿ� �˻�

	void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);		// �ֺ� ���� ���ϱ�

	/* ��Ŷ ���� */
	void mpLoginRes(CPacket* pSendPacket, INT64 AccountNo, BYTE Status);													// �α��� ���� �޽��� ����
	void mpMoveSectorRes(CPacket* pSendPacket, INT64 AccountNo, WORD SectorX, WORD SectorY);								// ���� �̵� �޽��� ����
	void mpChatting(CPacket* pSendPacket, INT64 AccountNo, WCHAR* ID, WCHAR* Nickname, WORD MessageLen, WCHAR* Message);	// ä�� ���� �޽��� ����

	void AddUser(ULONG64 udlSessionID, st_USER_INFO* pUser);	// ���� ���� �߰�
	st_USER_INFO* FindUser(ULONG64 udlSessionID);				// ���� ���� �˻�
	void DeleteUser(ULONG64 udlSessionID);						// ���� ���� ����

	void AddAccount(ULONG64 udlAccount, ULONG64 udlSessionID);		// Account ��ȣ �߰�
	ULONG64 FindAccount(ULONG64 udlAccount);						// Account ��ȣ �˻�
	void DeleteAccount(ULONG64 udlAccount, ULONG64 udlSessionID);	// Account ��ȣ ����

	void AddSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID);		// ���� ���� �߰�
	bool FindSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID);	// ���� ���� �˻�
	void DeleteSector(WORD wSectorY, WORD wSectorX, ULONG64 udlSessionID);	// ���� ���� ����

	void ServerControl(void);	// ����͸� ���� Ÿ��

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// ���� ���� ���� �����̳�
	// Key: SessionID, Val: st_USER_INFO*
	std::unordered_map<ULONG64, st_USER_INFO*>	_UserMap;
	SRWLOCK										_UserMapLock;

	// Account ��ȣ ���� �����̳�
	// Key: Account ��ȣ, Val: ���� ID
	std::unordered_multimap<ULONG64, ULONG64>	_AccountMap;

	// ���� ���� ���� �����̳�
	// Key: ���� ����(x, y). Value: ���� ID
	std::unordered_set<ULONG64>					_SectorPosMap[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
	SRWLOCK										_SectorMapLock[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

	LONG64 alignas(64)	_dlNowLoginUserNum;		// ���� �α��� ������
	DWORD				_dwMaxLoginUserNum;		// �ִ� �α��� ������
	LONG64				_dlNowSessionNum;		// ���� ���Ǽ�

	DWORD		_dwTimeoutValue;			// ���� Ÿ�Ӿƿ� ó�� �ִ� �ð�

	HANDLE		_hTimeCheckThread;			// Ÿ�� üũ ������ �ڵ�
	DWORD		_dwTimeCheckThreadsID;		// Ÿ�� üũ ������ ID

	DWORD		_dwLogLevel;				// �α� ����
	bool		_bSaveLog;					// �α� ���� ���� ����
	WCHAR		_LogFileName[_MAX_PATH];	// �α� ���� ���� �̸�
	
	ULONG64			_udlUserTimeTick;		// ���� Ÿ�Ӿƿ� ��� �ð�
	ULONG64			_udlMonitorTick;		// ����� ���� ��� �ð�

	DWORD		_dwWorkerThreadNum;			// IO ��Ŀ ������ ����
	DWORD		_dwCoreNum;					// �ھ� ����

	struct tm _StartTime;					// ���� ���� �ð�

	stMonitorInfo	_MonitorData;			// TPS ����� ����

	ULONG64		_udlMaxSessionTime;
	ULONG64		_udlMaxSessionTimeMonitor;
	ULONG64		_udlMaxSessionTimeMonitorMax;
	ULONG64		_udlDisConnCount;
	ULONG64		_udlDisConnSession[0x1FFF + 1];

	// ����͸� Ŭ���̾�Ʈ ����
	CProcessMonitoring		_ProcessMonitor;
	MonitorClient			_MonitorClient;

	cpp_redis::client		_RedisClient;
};

#endif