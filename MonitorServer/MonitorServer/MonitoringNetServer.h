#ifndef __MONITOR_NET_SERVER_H_
#define __MONITOR_NET_SERVER_H_

//#include <unordered_map>
//#include <unordered_set>
//#include <strsafe.h>
//#include <Pdh.h>
#include "CNetServer.h"
#include "CProcessorMonitoring.h"
#include "LockFreeQueue.h"
#include "CDBConnector.h"
#include "CDBJob.h"

#define dfNUMBER_MEGA		1000000
#define dfNUMBER_KILLO		1000

// ä�� ���� - �̱� ������
class MonitoringNetServer : public CNetServer
{
public:
	enum en_TIME_CHECK
	{
		en_MAX_MONITOR_USER = 200,
		en_SLEEP_TIME_CHECK_PERIOD = 100,			// Ÿ�� üũ ���� Ÿ��
		en_USER_TIME_OUT_PERIOD = 1000,				// ���� Ÿ�� �ƿ� ó�� �ֱ�
		en_MONITOR_COLLECT_TIME_PERIOD = 600000		// DB �α� �ֱ�(ms)
	};

	enum en_PROCESSOR_MONITOR_COLLECTOR
	{
		en_PRO_CPU_SHARE = 0,
		en_PRO_NON_PAGED_MEMORY,
		en_PRO_NET_RECV,
		en_PRO_NET_SEND,
		en_PRO_CPU_AVA_MEMORY,
		en_PRO_TOTAL
	};

	enum en_SERVER_MONITOR_COLLECTOR
	{
		en_SRV_CLT_CPU_SHARE = 0,
		en_SRV_CLT_MEMORY,
		en_SRV_CLT_SESSION_COUNT,
		en_SRV_CLT_PLAYER_COUNT,
		en_SRV_CLT_MSG_TPS,
		en_SRV_CLT_PACKET_POOL,
		en_SRV_CLT_TOTAL
	};

	struct st_PROCESSOR_MONITOR_COLLECTOR
	{
		LONG64 Count;
		LONG64 Total;
		LONG64 Max;
		LONG64 Min;
		int Type;
	};

	struct st_SERVER_MONITOR_COLLECTOR
	{
		LONG64 alignas(64) Count;
		LONG64 alignas(64) Total;
		LONG64 Max;
		LONG64 Min;
		int Type;
	};

	MonitoringNetServer();
	~MonitoringNetServer();

	void MonitoringOutput(void);

	void PacketProc_UpdateServer(CPacket*);	// ����͸� ���� ������Ʈ

private:
	/* CLanServer �Լ� ���� */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 udlSessionID);
	void OnClientLeave(ULONG64 udlSessionID);

	void OnRecv(ULONG64 udlSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	static unsigned int __stdcall Proc_TimeCheck(void* pvParam);	// Ÿ�� üũ
	static unsigned int __stdcall DBWriterThread(void* pvParam);	// DB ����

	/* ���ν��� */
	void PacketProc_LoginMonitorClient(ULONG64 udlSessionID, CPacket*);		// ����͸� Ŭ���̾�Ʈ �α���

	/* ��Ŷ ���� */
	void mpLoginRes(CPacket* pSendPacket, BYTE Status);						// �α��� ���� �޽��� ����
	void mpUpdateData(CPacket* pSendPacket, BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp);			// ���� �̵� �޽��� ����

	void AddUser(ULONG64 udlSessionID);			// ���� ���� �߰�
	bool FindUser(ULONG64 udlSessionID);		// ���� ���� �˻�
	void DeleteUser(ULONG64 udlSessionID);		// ���� ���� ����

	void CollectMonitorInfo(void);				// ���μ��� ����͸� ���� ����

	void ServerControl(void);					// ����͸� ���� Ÿ��

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// ���� ���� ���� �����̳�
	// Key: SessionID, Val: st_USER_INFO*
	std::list<ULONG64>	_UserMap;
	SRWLOCK				_UserMapLock;
	
	// �α� ����
	DWORD		_dwLogLevel;				// �α� ����
	bool		_bSaveLog;					// �α� ���� ���� ����
	WCHAR		_LogFileName[_MAX_PATH];

	// ����͸� ����
	HANDLE	_hTimeCheckThread;
	DWORD	_dwTimeCheckThreadsID;
	
	ULONG64	_udlMonitorTimeTick;
	ULONG64	_udlDBWriteTimeTick;

	CProcessorMonitoring	_ProcessorMonitor;

	// DB ����
	HANDLE	_hDBWriteEvent;
	HANDLE	_hDBWritehThread;
	DWORD	_dwDBWriteThreadsID;
	

	CDBConnector			_DBWriter;
	CLockFreeQueue<IDBJob*>	_DBJobQ;

	st_PROCESSOR_MONITOR_COLLECTOR	_ProcessorMonitorCollector[en_PRO_TOTAL];
	st_SERVER_MONITOR_COLLECTOR		_ServerMonitorCollector[en_SRV_CLT_TOTAL];
};

#endif