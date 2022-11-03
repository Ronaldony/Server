#include "stdafx.h"
#include <Pdh.h>
#include <cpp_redis/cpp_redis>
#include "LoginServer.h"
#include "CCrashDump.h"

CCrashDump crashDump;
LoginServer contents;

int main(void)
{
	timeBeginPeriod(1);

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