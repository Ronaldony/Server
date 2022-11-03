#ifndef __LIB_CCRASH_DUMP_H__
#define __LIB_CCRASH_DUMP_H__

//#include <stdio.h>
//#include <Windows.h>
//#include <crtdbg.h>
//#include <DbgHelp.h>

#pragma comment(lib, "Dbghelp.lib")
//#pragma comment(lib, "Kernel32.lib")

class CCrashDump
{
public:
	CCrashDump()
	{
		_DumpCount = 0;

		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = myInvalidParameterHandler;
		oldHandler = _set_invalid_parameter_handler(newHandler);	// crt 함수에 null 포인터 등을 넣었을 때

		_CrtSetReportMode(_CRT_WARN, 0);		// CRT 오류 메시지 표시 중단. 바로 덤프로 남도록
		_CrtSetReportMode(_CRT_ASSERT, 0);
		_CrtSetReportMode(_CRT_ERROR, 0);

		_CrtSetReportHook(_custom_Report_hook);

		_set_purecall_handler(myPurecallHandler);

		SetHandlerDump();
	}

	static void Crash(void)
	{
		int* p = nullptr;
		*p = 0;
	}
	
	static LONG WINAPI myExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
	{
		SYSTEMTIME stNowTime;

		long DumpCount = InterlockedIncrement(&_DumpCount);

		//---------------------------------
		// 현재 날짜와 시간 갱신
		//---------------------------------
		WCHAR filename[MAX_PATH];

		GetLocalTime(&stNowTime);
		wsprintf(filename, L"Dump_%d%02d%02d_%02d.%02d.%02d_%d.dmp",
			stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount);

		wprintf(L"\n\n\n!!! Cash Error!!! %d.%d.%d / %d:%d:%d \n",
			stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
		wprintf(L"Now Save Dump file...\n");

		HANDLE hDumpFile = CreateFile(filename,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);

		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION MunidumpExceptionInformation;

			MunidumpExceptionInformation.ThreadId = GetCurrentThreadId();
			MunidumpExceptionInformation.ExceptionPointers = pExceptionPointer;
			MunidumpExceptionInformation.ClientPointers = true;

			MiniDumpWriteDump(GetCurrentProcess(),
				GetCurrentProcessId(),
				hDumpFile,
				MiniDumpWithFullMemory,
				&MunidumpExceptionInformation,
				NULL,
				NULL);

			CloseHandle(hDumpFile);

			wprintf(L"CrashDump Save Finish !");
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}
	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(myExceptionFilter);
	}

	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
	{
		Crash();
	}

	static int _custom_Report_hook(int ireposttype, char* message, int* returnvalue)
	{
		Crash();
		return true;
	}

	static void myPurecallHandler(void)
	{
		Crash();
	}
	
	inline static long _DumpCount;
};
#endif