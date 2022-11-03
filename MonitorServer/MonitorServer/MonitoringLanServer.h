#ifndef __MONITOR_LAN_SERVER_H_
#define __MONITOR_LAN_SERVER_H_

//#include <unordered_map>
//#include <unordered_set>
//#include <strsafe.h>
//#include <Pdh.h>
#include "CLanServer.h"
#include "MonitoringNetServer.h"
#include "MonitoringLanServerPool.h"

#define dfNUMBER_MEGA		1000000
#define dfNUMBER_KILLO		1000


// ä�� ���� - �̱� ������
class MonitoringLanServer : public CLanServer 
{
public:

	MonitoringLanServer();
	~MonitoringLanServer();

	void RegisterMonitorNet(MonitoringNetServer* pMonitor) { _NetMonitor = pMonitor; }

private:
	/* CLanServer �Լ� ���� */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 udlSessionID);
	void OnClientLeave(ULONG64 udlSessionID);

	void OnRecv(ULONG64 udlSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	/* ���ν��� */
	void PacketProc_LoginServer(ULONG64 udlSessionID, CPacket*);			// ���� �α��� ��û ó��

	void AddUser(ULONG64 udlSessionID, st_LAN_MONITOR_USER_INFO* pUser);	// ���� ���� �߰�
	st_LAN_MONITOR_USER_INFO* FindUser(ULONG64 udlSessionID);				// ���� ���� �˻�
	void DeleteUser(ULONG64 udlSessionID);									// ���� ���� ����

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// ���� ���� ���� �����̳�
	// Key: SessionID, Val: st_USER_INFO*
	std::unordered_map<ULONG64, st_LAN_MONITOR_USER_INFO*>	_UserMap;
	SRWLOCK													_UserMapLock;
	
	// �α� ����
	DWORD		_dwLogLevel;				// �α� ����
	bool		_bSaveLog;					// �α� ���� ���� ����
	WCHAR		_LogFileName[_MAX_PATH];

	HANDLE	_hTimeCheckThread;
	DWORD	_dwTimeCheckThreadsID;
	
	DWORD	_dwTimeOutTick;

	MonitoringNetServer* _NetMonitor;
};

#endif