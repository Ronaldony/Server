#ifndef __LOG_H__
#define __LOG_H__
//#include <windows.h>

#define dfSYSTEM_LOG_LEVEL_DEBUG	0
#define dfSYSTEM_LOG_LEVEL_ERROR	1
#define dfSYSTEM_LOG_LEVEL_SYSTEM	2

#define dfSYSTEM_LOG_CONSOLE

#define LOG(type, level, format, ...)							\
	do															\
	{															\
		CSystemLog::SystemLog(type, level, format, __VA_ARGS__);\
	} while(0)

#define LOG_HEX(type, level, log, pByte, byteLne)				\
	do															\
	{															\
		CSystemLog::LogHex(type, level, log, pByte, byteLne);\
	} while(0)

#define LOG_DIRECTORY(str)						\
	do											\
	{											\
		CSystemLog::SystemLog_Directory(str);	\
	} while(0)

#define LOG_LEVEL(level)					\
	do										\
	{										\
		CSystemLog::SystemLog_Level(level);	\
	} while(0)

#define LOG_CONSOLE(flag)							\
	do												\
	{												\
		CSystemLog::SystemLog_OutputConsole(flag);	\
	} while(0)

class CSystemLog
{
public:
	enum en_LOG_LEVEL
	{
		LEVEL_DEBUG = 0,
		LEVEL_ERROR,
		LEVEL_SYSTEM
	};
private:
	struct st_SystemLog
	{
		long long alignas(64) LogCount;
		SRWLOCK Lock;
		int LogLevel;
		bool OuputConsole;
	};

public:
	static void SystemLog(const wchar_t* szType, en_LOG_LEVEL LogLevel, const wchar_t* szStringFormat, ...);		// �α� ���
	static void LogHex(const wchar_t* szType, en_LOG_LEVEL LogLevel, const wchar_t* szLog, unsigned char* pByte, int iByteLen); // Hex �α�

	static void SystemLog_Directory(const wchar_t* szString);	// �α� ���丮 ����
	static void SystemLog_Level(int iLogLevel);					// �α� ���� ����
	static void SystemLog_OutputConsole(bool bFlag);			// �α� �ܼ� ��� ����

private:
	CSystemLog();
	static void GetInstance(void);
	
	st_SystemLog stSystemLogData;
	wchar_t* pwchLogFile;
	inline static CSystemLog* SystemLogInstance;
	inline static long long IsFirstInstance;
};


#endif