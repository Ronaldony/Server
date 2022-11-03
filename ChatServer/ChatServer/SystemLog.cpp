#include <time.h>
#include <strsafe.h>
#include <windows.h>
#include "SystemLog.h"

CSystemLog::CSystemLog()
{
	memset(&stSystemLogData, 0, sizeof(stSystemLogData));
	InitializeSRWLock(&stSystemLogData.Lock);
	pwchLogFile = NULL;
}

void CSystemLog::SystemLog(const wchar_t* szType, en_LOG_LEVEL LogLevel, const wchar_t* szStringFormat, ...)
{
	GetInstance();

	WCHAR szLogLevel[3][10] =
	{
		L"DEBUG",
		L"WARNING",
		L"ERROR"
	};
	WCHAR szLogBuff[1024] = {0,};

	if ((SystemLogInstance->stSystemLogData.LogLevel > LogLevel) || (LEVEL_DEBUG > LogLevel))
		return;

	// Type
	HRESULT hResultType;
	hResultType = StringCchPrintfW(szLogBuff, 1024, L"[%s]", szType);

	if (FAILED(hResultType))
	{
		int* exit = NULL;
		*exit = 1;
	}
	
	// 시간 + 로그레벨 + 로그 카운트
	time_t timer = time(NULL);
	struct tm realTime;
	(void)localtime_s(&realTime, &timer);

	InterlockedIncrement64(&SystemLogInstance->stSystemLogData.LogCount);

	HRESULT hResultLogInfo;
	hResultLogInfo = StringCchPrintfW(szLogBuff, 1024, L"%s [%d-%02d-%02d %02d:%02d:%02d / %s / %09lld] ", szLogBuff, realTime.tm_year + 1900, realTime.tm_mon + 1, realTime.tm_mday, realTime.tm_hour, realTime.tm_min, realTime.tm_sec,
		szLogLevel[LogLevel], SystemLogInstance->stSystemLogData.LogCount);

	if (FAILED(hResultLogInfo))
	{
		int* exit = NULL;
		*exit = 1;
	}

	// 가변인자를 사용한 로그 문자열
	va_list va;
	size_t wstrLen;
	HRESULT hResultStrLen;

	va_start(va, szStringFormat);
	hResultStrLen = StringCchLengthW(szLogBuff, 1024, &wstrLen);
	if (FAILED(hResultStrLen))
	{
		int* exit = NULL;
		*exit = 1;
	}

	HRESULT hResultValist;
	hResultValist = StringCchVPrintfW(szLogBuff + wstrLen, 256, szStringFormat, va);

	if (FAILED(hResultValist))
	{
		int* exit = NULL;
		*exit = 1;
	}
	va_end(va);

	// 개행 문자 대입
	hResultStrLen = StringCchLengthW(szLogBuff, 1024, &wstrLen);
	if (FAILED(hResultStrLen))
	{
		int* exit = NULL;
		*exit = 1;
	}

	HRESULT hResultNewLine;
	hResultNewLine = StringCchPrintfW(szLogBuff, 1024, L"%s\n", szLogBuff);

	if (FAILED(hResultNewLine))
	{
		int* exit = NULL;
		*exit = 1;
	}
	va_end(va);


	// 파일에 기록
	AcquireSRWLockExclusive(&SystemLogInstance->stSystemLogData.Lock);
	if (SystemLogInstance->pwchLogFile == NULL)
	{
		SystemLog_Directory(L"Default");
	}

	FILE* fp;
	errno_t err = _wfopen_s(&fp, SystemLogInstance->pwchLogFile, L"ab, ccs=UTF-16LE");
	if ((err != 0) || (fp == NULL))
	{
		int* exit = NULL;
		*exit = 1;
	}

	hResultStrLen =  StringCchLengthW(szLogBuff, 1024, &wstrLen);
	if (FAILED(hResultStrLen))
	{
		int* exit = NULL;
		*exit = 1;
	}
	size_t size = fwrite(szLogBuff, 2, wstrLen, fp);

	fclose(fp);

	ReleaseSRWLockExclusive(&SystemLogInstance->stSystemLogData.Lock);

	if (SystemLogInstance->stSystemLogData.OuputConsole)
		wprintf(L"%s\n", szLogBuff);
}

void CSystemLog::LogHex(const wchar_t* szType, en_LOG_LEVEL LogLevel, const wchar_t* szLog, unsigned char* pByte, int iByteLen)
{
	GetInstance();

	WCHAR szLogLevel[3][10] =
	{
		L"DEBUG",
		L"WARNING",
		L"ERROR"
	};
	WCHAR szLogBuff[1024] = { 0, };

	if (SystemLogInstance->stSystemLogData.LogLevel > LogLevel)
		return;

	// Type
	HRESULT hResultType;
	hResultType = StringCchPrintfW(szLogBuff, 1024, L"[%s]", szType);

	if (FAILED(hResultType))
	{
		int* exit = NULL;
		*exit = 1;
	}

	// 시간 + 로그레벨 + 로그 카운트
	time_t timer = time(NULL);
	struct tm realTime;
	(void)localtime_s(&realTime, &timer);

	InterlockedIncrement64(&SystemLogInstance->stSystemLogData.LogCount);

	HRESULT hResultLogInfo;
	hResultLogInfo = StringCchPrintfW(szLogBuff, 1024, L"%s [%d-%02d-%02d %02d:%02d:%02d / %s / %09lld] ", szLogBuff, realTime.tm_year + 1900, realTime.tm_mon + 1, realTime.tm_mday, realTime.tm_hour, realTime.tm_min, realTime.tm_sec,
		szLogLevel[LogLevel], SystemLogInstance->stSystemLogData.LogCount);

	// 함수 실패 여부 확인
	if (FAILED(hResultLogInfo))
	{
		int* exit = NULL;
		*exit = 1;
	}

	// 문자 로그 기록
	size_t wstrLen;
	HRESULT hResultStrLen;

	hResultStrLen = StringCchLengthW(szLogBuff, 1024, &wstrLen);
	if (FAILED(hResultStrLen))
	{
		int* exit = NULL;
		*exit = 1;
	}

	HRESULT hResultLog;
	hResultLog = StringCchPrintfW(szLogBuff + wstrLen, 1024 - wstrLen, L"%s: ", szLog);

	if (FAILED(hResultLog))
	{
		int* exit = NULL;
		*exit = 1;
	}

	// 바이트 문자 대입
	for (int cnt = 0; cnt < iByteLen; cnt++)
	{
		size_t wstrLen;
		HRESULT hResultStrLen;

		hResultStrLen = StringCchLengthW(szLogBuff, 1024, &wstrLen);
		if (FAILED(hResultStrLen))
		{
			int* exit = NULL;
			*exit = 1;
		}

		HRESULT hResultLog;
		hResultLog = StringCchPrintfW(szLogBuff + wstrLen, 1024 - wstrLen, L"%02x", *(pByte + cnt));

		if (FAILED(hResultLog))
		{
			int* exit = NULL;
			*exit = 1;
		}
	}

	// 개행 문자 대입
	hResultStrLen = StringCchLengthW(szLogBuff, 1024, &wstrLen);
	if (FAILED(hResultStrLen))
	{
		int* exit = NULL;
		*exit = 1;
	}

	HRESULT hResultNewLine;
	hResultNewLine = StringCchPrintfW(szLogBuff, 1024 - wstrLen, L"%s\n", szLogBuff);

	if (FAILED(hResultNewLine))
	{
		int* exit = NULL;
		*exit = 1;
	}


	// 파일에 기록
	AcquireSRWLockExclusive(&SystemLogInstance->stSystemLogData.Lock);
	if (SystemLogInstance->pwchLogFile == NULL)
	{
		SystemLog_Directory(L"Default");
	}

	FILE* fp;
	errno_t err = _wfopen_s(&fp, SystemLogInstance->pwchLogFile, L"ab, ccs=UTF-16LE");
	if ((err != 0) || (fp == NULL))
	{
		int* exit = NULL;
		*exit = 1;
	}

	hResultStrLen = StringCchLengthW(szLogBuff, 1024, &wstrLen);
	if (FAILED(hResultStrLen))
	{
		int* exit = NULL;
		*exit = 1;
	}
	size_t size = fwrite(szLogBuff, 2, wstrLen, fp);

	fclose(fp);

	ReleaseSRWLockExclusive(&SystemLogInstance->stSystemLogData.Lock);

	if (SystemLogInstance->stSystemLogData.OuputConsole)
		wprintf(L"%s\n", szLogBuff);
}

void CSystemLog::SystemLog_Directory(const wchar_t* szString)
{
	GetInstance();

	if (SystemLogInstance->pwchLogFile != NULL)
		delete [] SystemLogInstance->pwchLogFile;

	SystemLogInstance->pwchLogFile = new wchar_t[1024];
	memset(SystemLogInstance->pwchLogFile, 0, 1024 * sizeof(wchar_t));

	if (false == CreateDirectoryW(szString, NULL))
	{
		if (ERROR_ALREADY_EXISTS != GetLastError())
		{
			int* exit = NULL;
			*exit = 1;
		}
	}

	HRESULT hResult = StringCchPrintfW(SystemLogInstance->pwchLogFile, 1024, L"./%s/SystemLog_", szString);
	if (FAILED(hResult))
	{
		int* exit = NULL;
		*exit = 1;
	}

	time_t timer = time(NULL);
	struct tm realTime;
	(void)localtime_s(&realTime, &timer);

	hResult = StringCchPrintfW(SystemLogInstance->pwchLogFile, 1024, L"%s%0d_%02d.txt", SystemLogInstance->pwchLogFile, realTime.tm_year + 1900, realTime.tm_mon + 1);
	if (FAILED(hResult))
	{
		int* exit = NULL;
		*exit = 1;
	}
}

void CSystemLog::SystemLog_Level(int iLogLevel)
{
	GetInstance();

	SystemLogInstance->stSystemLogData.LogLevel = iLogLevel;
}

void CSystemLog::SystemLog_OutputConsole(bool bFlag)
{
	GetInstance();
	SystemLogInstance->stSystemLogData.OuputConsole = bFlag;
}

void CSystemLog::GetInstance(void)
{
	if (false == InterlockedExchange64(&IsFirstInstance, true))
	{
		SystemLogInstance = new CSystemLog;
	}

	while (SystemLogInstance == NULL);
}
