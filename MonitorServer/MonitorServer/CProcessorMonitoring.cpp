#include <windows.h>
#include <Pdh.h>
#include <strsafe.h>
#include "CProcessorMonitoring.h"

#pragma comment(lib,"Pdh.lib")

CProcessorMonitoring::CProcessorMonitoring(HANDLE hProcess)
{
	//------------------------------------------------------------
	// PDH ���� �ڵ� ���� �� ī���� ����(������ ������ �̸� ������ ����)
	// 
	//------------------------------------------------------------
	PdhOpenQuery(NULL, NULL, &_hPdhQuery);

	PdhAddCounter(_hPdhQuery, L"\\Memory\\Available MBytes", NULL, &_PdhCounter[en_PROCESSOR_AVAILABLE_MEMORY]);
	PdhAddCounter(_hPdhQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &_PdhCounter[en_PROCESSOR_NONPAGED]);

	//------------------------------------------------------------
	// ���μ��� �ڵ� �Է��� ���� ��� �ڱ� �ڽ��� �������
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
	// ��Ʈ��ũ Ʈ���� PDH
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

	// ���� �׸�� �������̽� �׸��� ����.
	// ó�� ��ȯ�� ������ ũ�⸦ �𸣱� ������ NULL�� �Է��Ͽ� ���� ����
	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);

	szCounters = new WCHAR[dwCounterSize];
	szInterfaces = new WCHAR[dwInterfaceSize];

	// ���� ���� �׸�� �������̽� �׸��� ����.
	if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
	{
		delete[] szCounters;
		delete[] szInterfaces;

		int* down = NULL;
		*down = 0;
	}

	iCnt = 0;
	szCur = szInterfaces;

	// szInterfaces���� ���ڿ� ������ �����鼭, �̸��� ����޴´�.
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
	// ���μ��� ������ �����Ѵ�.
	// 
	// ������ ��� ����ü�� FILETIME ������, ULARGE_INTEGER �� ������ �����Ƿ� �̸� �����.
	// FILETIME ����ü�� 100 ���뼼���� ������ �ð� ������ ǥ���ϴ� ����ü��.
	//------------------------------------------------------------

	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	//------------------------------------------------------------
	// �ý��� ��� �ð��� ���Ѵ�.
	// 
	// ���̵� Ÿ�� / Ŀ�� ��� Ÿ�� (���̵� ����) / ���� ��� Ÿ��
	//------------------------------------------------------------
	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
	{
		return;
	}

	// Ŀ�� Ÿ�ӿ��� ���̵� Ÿ���� ���Ե�
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
		// Ŀ�� Ÿ�ӿ� ���̵� Ÿ���� �����Ƿ� ���� ���
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
	// PDH ����͸� ���� ����
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
	// �̴��� ������ŭ ���鼭 �� ���� ����
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