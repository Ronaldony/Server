#include "stdafx.h"
#include <Pdh.h>
#include <cpp_redis/cpp_redis>
#include "ChattingServer.h"
#include "CCrashDump.h"

CCrashDump crashDump;
ChattingServer contents;

int main(void)
{
	timeBeginPeriod(1);
	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

	while (1)
	{
		contents.MonitoringOutput();
		Sleep(1000);
	}

	Sleep(INFINITE);
	wprintf(L"Finish\n");

	timeEndPeriod(1);

	return 0;
}