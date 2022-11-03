#include <windows.h>
#include <Pdh.h>
#include <strsafe.h>
#include "CProcessorMonitoring.h"

#pragma comment(lib,"Pdh.lib")

CProcessorMonitoring::CProcessorMonitoring(HANDLE hProcess)
{
	//------------------------------------------------------------
	// PDH 쿼리 핸들 생성 및 카운터 생성(여러개 수집시 이를 여러개 생성)
	// 
	//------------------------------------------------------------
	PdhOpenQuery(NULL, NULL, &_hPdhQuery);

	PdhAddCounter(_hPdhQuery, L"\\Memory\\Available MBytes", NULL, &_PdhCounter[en_PROCESSOR_AVAILABLE_MEMORY]);
	PdhAddCounter(_hPdhQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &_PdhCounter[en_PROCESSOR_NONPAGED]);

	//------------------------------------------------------------
	// 프로세스 핸들 입력이 없는 경우 자기 자신을 대상으로
	// 
	//------------------------------------------------------------
	if (INVALID_HANDLE_VALUE == hProcess)
	{
		hProcess = GetCurrentProcess();
	}

	_ftProcessor_LastKernel.QuadPart = 0;
	_ftProcessor_LastUser.QuadPart = 0;
	_ftProcessor_LastIdle.QuadPart = 0;


	//------------------------------------------------------------
	// 네트워크 트래픽 PDH
	// 
	//------------------------------------------------------------
	int		iCnt = 0;
	bool	bErr = false;
	WCHAR*	szCur = NULL;
	WCHAR*	szCounters = NULL;
	WCHAR*	szInterfaces = NULL;
	DWORD	dwCounterSize = 0;
	DWORD	dwInterfaceSize = 0;
	WCHAR	szQuery[1024] = { 0, };

	// 측정 항목과 인터페이스 항목을 얻음.
	// 처음 반환될 버퍼의 크기를 모르기 때문에 NULL을 입력하여 길이 얻어옴
	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);

	szCounters = new WCHAR[dwCounterSize];
	szInterfaces = new WCHAR[dwInterfaceSize];

	// 실제 측정 항목과 인터페이스 항목을 얻음.
	if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
	{
		delete[] szCounters;
		delete[] szInterfaces;

		int* down = NULL;
		*down = 0;
	}

	iCnt = 0;
	szCur = szInterfaces;

	// szInterfaces에서 문자열 단위로 끊으면서, 이름을 복사받는다.
	for (; *szCur != L'\0' && iCnt < df_PDH_ETHERNET_MAX; szCur += wcslen(szCur) + 1, iCnt++)
	{
		_EthernetData[iCnt]._bUse = true;
		_EthernetData[iCnt]._szName[0] = L'\0';

		wcscpy_s(_EthernetData[iCnt]._szName, szCur);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Received/sec", szCur);
		PdhAddCounter(_hPdhQuery, szQuery, NULL, &_EthernetData[iCnt]._pdh_Counter_Network_RecvBytes);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Sent/sec", szCur);
		PdhAddCounter(_hPdhQuery, szQuery, NULL, &_EthernetData[iCnt]._pdh_Counter_Network_SendBytes);
	}

	UpdateMonitorInfo();
}

void CProcessorMonitoring::UpdateMonitorInfo(void)
{
	UpdatePDH();
	UpdateProcessor();
	UpdateNetwork();
}

void CProcessorMonitoring::UpdateProcessor(void)
{
	//------------------------------------------------------------
	// 프로세서 사용률을 갱신한다.
	// 
	// 본래의 사용 구조체는 FILETIME 이지만, ULARGE_INTEGER 와 구조가 같으므로 이를 사용함.
	// FILETIME 구조체는 100 나노세컨드 단위의 시간 단위를 표현하는 구조체임.
	//------------------------------------------------------------

	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	//------------------------------------------------------------
	// 시스템 사용 시간을 구한다.
	// 
	// 아이들 타임 / 커널 사용 타임 (아이들 포함) / 유저 사용 타임
	//------------------------------------------------------------
	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
	{
		return;
	}

	// 커널 타임에는 아이들 타임이 포함됨
	ULONGLONG KernelDiff = Kernel.QuadPart - _ftProcessor_LastKernel.QuadPart;
	ULONGLONG UserDiff = User.QuadPart - _ftProcessor_LastUser.QuadPart;
	ULONGLONG IdleDiff = Idle.QuadPart - _ftProcessor_LastIdle.QuadPart;

	ULONGLONG Total = KernelDiff + UserDiff;

	if (Total == 0)
	{
		_fProcessorTotal = 0;
		_fProcessorUser = 0;
		_fProcessorKernel = 0;
	}
	else
	{
		// 커널 타임에 아이들 타임이 있으므로 빼서 계산
		_fProcessorTotal = (float)((double)(Total - IdleDiff) / Total * 100.0f);
		_fProcessorUser = (float)((double)UserDiff / Total * 100.0f);
		_fProcessorKernel = (float)((double)(KernelDiff - IdleDiff) / Total * 100.0f);
	}

	_ftProcessor_LastKernel = Kernel;
	_ftProcessor_LastUser = User;
	_ftProcessor_LastIdle = Idle;

}

void CProcessorMonitoring::UpdatePDH(void)
{
	//-------------------------------------------------------------
	// PDH 모니터링 정보 갱신
	// 
	//-------------------------------------------------------------
	PdhCollectQueryData(_hPdhQuery);

	for (int cnt = 0; cnt < en_PDH_COUNTER_NUMBER; ++cnt)
	{

		PdhGetFormattedCounterValue(_PdhCounter[cnt], PDH_FMT_DOUBLE, NULL, &_PdhCounterVal[cnt]);
	}
}
void CProcessorMonitoring::UpdateNetwork(void)
{
	//-------------------------------------------------------------
	// 이더넷 개수만큼 돌면서 총 합을 뽑음
	// 
	//-------------------------------------------------------------

	PDH_STATUS status;
	PDH_FMT_COUNTERVALUE CounterValue;

	_pdh_value_Network_RecvBytes = 0;
	_pdh_value_Network_SendBytes = 0;

	for (int iCnt = 0; iCnt < df_PDH_ETHERNET_MAX; iCnt++)
	{
		if (_EthernetData[iCnt]._bUse)
		{
			status = PdhGetFormattedCounterValue(_EthernetData[iCnt]._pdh_Counter_Network_RecvBytes, PDH_FMT_DOUBLE, NULL, &CounterValue);
			if (status == 0)
				_pdh_value_Network_RecvBytes += CounterValue.doubleValue;

			status = PdhGetFormattedCounterValue(_EthernetData[iCnt]._pdh_Counter_Network_SendBytes, PDH_FMT_DOUBLE, NULL, &CounterValue);
			if (status == 0) 
				_pdh_value_Network_SendBytes += CounterValue.doubleValue;
		}
	}
}