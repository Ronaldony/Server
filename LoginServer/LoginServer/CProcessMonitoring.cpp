#include <windows.h>
#include <Pdh.h>
#include "CProcessMonitoring.h"

#pragma comment(lib,"Pdh.lib")

CProcessMonitoring::CProcessMonitoring(HANDLE hProcess)
{
	//------------------------------------------------------------
	// PDH ���� �ڵ� ���� �� ī���� ����(������ ������ �̸� ������ ����)
	//------------------------------------------------------------
	PdhOpenQuery(NULL, NULL, &_hPdhQuery);
	
	PdhAddCounter(_hPdhQuery, L"\\Process(LoginServer)\\Private Bytes", NULL, &_PdhCounter[en_PROCESS_USER_MEMORY]);
	PdhAddCounter(_hPdhQuery, L"\\Process(LoginServer)\\Pool Nonpaged Bytes", NULL, &_PdhCounter[en_PROCESS_NONPAGED]);

	//------------------------------------------------------------
	// ���μ��� �ڵ� �Է��� ���� ��� �ڱ� �ڽ��� �������
	//------------------------------------------------------------
	if (INVALID_HANDLE_VALUE == hProcess)
	{
		hProcess = GetCurrentProcess();
	}

	//------------------------------------------------------------
	// ���μ��� ������ Ȯ���Ѵ�.
	// 
	// ���μ��� (exe) ����� ���� cpu ������ �����⸦ �Ͽ� ���� ������ ����
	//------------------------------------------------------------
	SYSTEM_INFO SystemInfo;

	GetSystemInfo(&SystemInfo);
	_iNumberOfProcessors = SystemInfo.dwNumberOfProcessors;

	_fProcessTotal = 0;
	_fProcessUser = 0;
	_fProcessKernel = 0;

	_ftProcess_LastTime.QuadPart = 0;
	_ftProcess_LastUser.QuadPart = 0;
	_ftProcess_LastKernel.QuadPart = 0;

	UpdateMonitorInfo();
}

void CProcessMonitoring::UpdateMonitorInfo(void)
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
	ULONGLONG KernelDiff;
	ULONGLONG UserDiff;

	ULONGLONG Total;
	ULONGLONG TimeDiff;

	//-------------------------------------------------------------
	// ������ ���μ��� ������ ����Ѵ�.
	//-------------------------------------------------------------
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;


	//-------------------------------------------------------------
	// ������ 100 ���뼼���� ���� �ð��� ����. UTC �ð�
	// 
	// ���μ��� ���� �Ǵ��� ����
	// 
	// a = ���� ������ �ý��� �ð� (�׳� ������ ������ �ð�)
	// b = ���μ����� CPU ��� �ð�
	// 
	// a : 100 = b : ���� �������� ������ ����
	//-------------------------------------------------------------

	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);

	//-------------------------------------------------------------
	// �ش� ���μ����� ����� �ð��� ����
	// 
	// �ι�°, ����°�� ����, ���� �ð����� �̻��
	//-------------------------------------------------------------
	GetProcessTimes(GetCurrentProcess(), (LPFILETIME)&None, (LPFILETIME)&None, (LPFILETIME)&Kernel, (LPFILETIME)&User);

	//-------------------------------------------------------------
	// ������ ����� ���μ��� �ð����� ���� ���ؼ� ������ ���� �ð��� �������� Ȯ��
	// 
	// �׸��� ���� ������ �ð����� ������ ������ ����
	//-------------------------------------------------------------

	TimeDiff = NowTime.QuadPart - _ftProcess_LastTime.QuadPart;
	UserDiff = User.QuadPart - _ftProcess_LastUser.QuadPart;
	KernelDiff = Kernel.QuadPart - _ftProcess_LastKernel.QuadPart;

	Total = KernelDiff + UserDiff;

	_fProcessTotal = (float)(Total / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);
	_fProcessKernel = (float)(KernelDiff / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);
	_fProcessUser = (float)(UserDiff / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);

	_ftProcess_LastTime = NowTime;
	_ftProcess_LastUser= User;
	_ftProcess_LastKernel= Kernel;




	//-------------------------------------------------------------
	// PDH ����͸� ���� ����
	// 
	//-------------------------------------------------------------
	for (int cnt = 0; cnt < en_PDH_COUNTER_NUMBER; ++cnt)
	{
		PdhCollectQueryData(_hPdhQuery);
		
		PdhGetFormattedCounterValue(_PdhCounter[cnt], PDH_FMT_DOUBLE, NULL, &_PdhCounterVal[cnt]);
	}
}