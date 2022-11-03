#ifndef __PROCESS_MONITORING_H__
#define __PROCESS_MONITORING_H__

//#include <windows.h>

class CProcessMonitoring
{
public:
	enum en_PDH_COUNTER
	{
		en_PROCESS_USER_MEMORY,
		en_PROCESS_NONPAGED,
		en_PDH_COUNTER_NUMBER
	};
	//------------------------------------------------------------
	// 생성자, 확인대상 프로세스 핸들. 미입력시 자기 자신
	//------------------------------------------------------------

	CProcessMonitoring(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void UpdateMonitorInfo(void);


	float	ProcessTotal(void) { return _fProcessTotal; }
	float	ProcessUser(void) { return _fProcessUser; }
	float	ProcessKernel(void) { return _fProcessKernel; }

	double	ProcessUserAllocMemory(void) { return _PdhCounterVal[en_PROCESS_USER_MEMORY].doubleValue; }
	double	ProcessNonPagedMemory(void) { return _PdhCounterVal[en_PROCESS_NONPAGED].doubleValue; }

private:
	
	HANDLE	_hProcess;
	int		_iNumberOfProcessors;

	float	_fProcessTotal;
	float	_fProcessUser;
	float	_fProcessKernel;

	ULARGE_INTEGER	_ftProcess_LastKernel;
	ULARGE_INTEGER	_ftProcess_LastUser;
	ULARGE_INTEGER	_ftProcess_LastTime;


	// PDH
	PDH_HQUERY _hPdhQuery;
	PDH_HCOUNTER _PdhCounter[en_PDH_COUNTER_NUMBER];
	PDH_FMT_COUNTERVALUE _PdhCounterVal[en_PDH_COUNTER_NUMBER];
};
#endif