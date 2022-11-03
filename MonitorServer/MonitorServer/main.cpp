#include "stdafx.h"
#include <Pdh.h>
#include "MonitoringLanServer.h"
#include "CCrashDump.h"

CCrashDump crashDump;

int main(void)
{
	timeBeginPeriod(1);
	MonitoringLanServer lanMonitor;
	MonitoringNetServer netMonitor;

	lanMonitor.RegisterMonitorNet(&netMonitor);
	while (1)
	{
		netMonitor.MonitoringOutput();
		Sleep(1000);
	}

	Sleep(INFINITE);
	wprintf(L"Finish\n");

	timeEndPeriod(1);

	return 0;
}