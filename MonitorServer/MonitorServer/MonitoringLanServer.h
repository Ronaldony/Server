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


// 채팅 서버 - 싱글 스레드
class MonitoringLanServer : public CLanServer 
{
public:

	MonitoringLanServer();
	~MonitoringLanServer();

	void RegisterMonitorNet(MonitoringNetServer* pMonitor) { _NetMonitor = pMonitor; }

private:
	/* CLanServer 함수 정의 */
	bool OnConnectionRequest(WCHAR* pwchIP, USHORT ushPort);

	void OnClientJoin(ULONG64 udlSessionID);
	void OnClientLeave(ULONG64 udlSessionID);

	void OnRecv(ULONG64 udlSessionID, CPacket* pPacket);

	void OnError(int errorcode, const WCHAR* comment);

	/* 프로시저 */
	void PacketProc_LoginServer(ULONG64 udlSessionID, CPacket*);			// 유저 로그인 요청 처리

	void AddUser(ULONG64 udlSessionID, st_LAN_MONITOR_USER_INFO* pUser);	// 유저 정보 추가
	st_LAN_MONITOR_USER_INFO* FindUser(ULONG64 udlSessionID);				// 유저 정보 검색
	void DeleteUser(ULONG64 udlSessionID);									// 유저 정보 삭제

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;

	// 유저 정보 관리 컨테이너
	// Key: SessionID, Val: st_USER_INFO*
	std::unordered_map<ULONG64, st_LAN_MONITOR_USER_INFO*>	_UserMap;
	SRWLOCK													_UserMapLock;
	
	// 로그 관련
	DWORD		_dwLogLevel;				// 로그 레벨
	bool		_bSaveLog;					// 로그 파일 저장 여부
	WCHAR		_LogFileName[_MAX_PATH];

	HANDLE	_hTimeCheckThread;
	DWORD	_dwTimeCheckThreadsID;
	
	DWORD	_dwTimeOutTick;

	MonitoringNetServer* _NetMonitor;
};

#endif