#ifndef __PROCESSOR_MONITORING_H__
#define __PROCESSOR_MONITORING_H__

//#include <windows.h>

#define df_PDH_ETHERNET_MAX		8

class CProcessorMonitoring
{
public:
	enum en_PDH_COUNTER
	{
		en_PROCESSOR_AVAILABLE_MEMORY,
		en_PROCESSOR_NONPAGED,
		en_PDH_COUNTER_NUMBER
	};

	struct st_ETHERNET
	{
		bool			_bUse;
		WCHAR			_szName[128];

		PDH_HCOUNTER	_pdh_Counter_Network_RecvBytes;
		PDH_HCOUNTER	_pdh_Counter_Network_SendBytes;
	};

	//------------------------------------------------------------
	// 생성자, 확인대상 프로세스 핸들. 미입력시 자기 자신
	//------------------------------------------------------------

	CProcessorMonitoring(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void UpdateMonitorInfo(void);

	float	ProcessorTotal(void) { return _fProcessorTotal; }
	float	ProcessorUser(void) { return _fProcessorUser; }
	float	ProcessorKernel(void) { return _fProcessorKernel; }

	double	ProcessorAvailableMemory(void) { return _PdhCounterVal[en_PROCESSOR_AVAILABLE_MEMORY].doubleValue; }
	double	ProcessorNonPagedMemory(void) { return _PdhCounterVal[en_PROCESSOR_NONPAGED].doubleValue; }
	double	NetworkSendTraffic(void) { return _pdh_value_Network_SendBytes; }
	double	NetworkRecvTraffic(void) { return _pdh_value_Network_RecvBytes; }


private:

	void UpdateProcessor(void);
	void UpdatePDH(void);
	void UpdateNetwork(void);


private:
	
	//------------------------------------------------------------
	// 프로세서 모니터링
	//------------------------------------------------------------
	float	_fProcessorTotal;
	float	_fProcessorUser;
	float	_fProcessorKernel;

	ULARGE_INTEGER	_ftProcessor_LastKernel;
	ULARGE_INTEGER	_ftProcessor_LastUser;
	ULARGE_INTEGER	_ftProcessor_LastIdle;


	//------------------------------------------------------------
	// PDH 모니터링
	//------------------------------------------------------------
	PDH_HQUERY _hPdhQuery;
	PDH_HCOUNTER _PdhCounter[en_PDH_COUNTER_NUMBER];
	PDH_FMT_COUNTERVALUE _PdhCounterVal[en_PDH_COUNTER_NUMBER];


	//------------------------------------------------------------
	// 이더넷 모니터링
	//------------------------------------------------------------
	st_ETHERNET		_EthernetData[df_PDH_ETHERNET_MAX];		// 랜카드 별 PDH 정보
	double			_pdh_value_Network_RecvBytes;			// 모든 이더넷의 총 Recv Bytes
	double			_pdh_value_Network_SendBytes;			// 모든 이더넷의 총 Send Bytes
};
#endif