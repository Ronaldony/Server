#ifndef __CONTENTS_H__
#define __CONTENTS_H__
#include "CLanClient.h"

// 채팅 서버 - 싱글 스레드
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

	void mpLoginReq(CPacket* pSendPacket, int ServerNo);										// 로그인 요청
	void mpUpdateMonitor(CPacket* pSendPacket, BYTE DataType, int DataValue, int TimeStamp);	// 데이터 업데이트

private:
	/* CNetServer 함수 정의 */
	void OnEnterJoinServer(void);
	void OnLeaveServer(void);

	void OnRecv(CPacket* pPacket);
	void OnSend(int sendsize);

	void OnError(int errorcode, const WCHAR* comment);

	static unsigned int __stdcall ConnectThread(void* pvParam);		// Connection 요청 스레드

private:
	wchar_t*			_whiteListIP;
	int					_whiteListIPCount;
	
	// 로그 관련
	DWORD		_dwLogLevel;		// 로그 레벨
	bool		_bSaveLog;			// 로그 파일 저장 여부
	WCHAR		_LogFileName[_MAX_PATH];

	HANDLE		_hConnectThread;
	DWORD		_hConnectThreadID;
};

#endif
