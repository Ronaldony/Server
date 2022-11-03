#ifndef __PROCADEMY_LIB_DBCONNECTOR__
#define __PROCADEMY_LIB_DBCONNECTOR__

//#include <windows.h>
//#include <strsafe.h>
#include "mysql/include/mysql.h"
#include "mysql/include/errmsg.h"

#pragma comment(lib, "mysql/lib/vs14/mysqlclient.lib")


/////////////////////////////////////////////////////////
// MySQL DB 연결 클래스
//
// 단순하게 MySQL Connector 를 통한 DB 연결만 관리한다.
//
// 스레드에 안전하지 않으므로 주의 해야 함.
// 여러 스레드에서 동시에 이를 사용한다면 개판이 됨.
//
/////////////////////////////////////////////////////////

class CDBConnectorTLS
{
public: 
	enum en_DB_CONNECTOR
	{
		eQUERY_MAX_LEN = 2048
	};

	class CDBConnector
	{
	public:

		CDBConnector(WCHAR* szDBIP, WCHAR* szUser, WCHAR* szPassword, WCHAR* szDBName, int iDBPort)
			:_pMySQL(nullptr), _pSqlResult(nullptr)
		{
			memset(_szQuery, 0, sizeof(_szQuery));
			memset(_szQueryUTF8, 0, sizeof(_szQueryUTF8));
			_iLastError = 0;
			memset(_szLastErrorMsg, 0, sizeof(_szLastErrorMsg));

			//-------------------------------------------------------------
			// DB 연결 정보
			//
			//-------------------------------------------------------------

			memcpy_s(_szDBIP, sizeof(_szDBIP), szDBIP, wcslen(szDBIP) * sizeof(WCHAR));
			memcpy_s(_szDBUser, sizeof(_szDBUser), szUser, wcslen(szUser) * sizeof(WCHAR));
			memcpy_s(_szDBPassword, sizeof(_szDBPassword), szPassword, wcslen(szPassword) * sizeof(WCHAR));
			memcpy_s(_szDBName, sizeof(_szDBName), szDBName, wcslen(szDBName) * sizeof(WCHAR));
			_iDBPort = iDBPort;

			// 초기화
			mysql_init(&_MySQL);
		}

		~CDBConnector()
		{
			//---------------------------------
			// DB 세션 종료
			//---------------------------------
			Disconnect();
		}

		//////////////////////////////////////////////////////////////////////
		// MySQL DB 연결
		//////////////////////////////////////////////////////////////////////
		bool Connect(void)
		{
			// Disable SSL
			int sslmode = 1;
			mysql_options(&_MySQL, MYSQL_OPT_SSL_MODE, &sslmode);


			//---------------------------------
			// 연결 정보 UTF-8 변환
			//---------------------------------

			int iLen;
			char szDBIP[256] = { 0, };
			char szUser[256] = { 0, };
			char szPassword[256] = { 0, };
			char szDBName[256] = { 0, };

			iLen = WideCharToMultiByte(CP_ACP, 0, _szDBIP, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szDBIP, -1, szDBIP, iLen, NULL, NULL);

			iLen = WideCharToMultiByte(CP_ACP, 0, _szDBUser, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szDBUser, -1, szUser, iLen, NULL, NULL);

			iLen = WideCharToMultiByte(CP_ACP, 0, _szDBPassword, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szDBPassword, -1, szPassword, iLen, NULL, NULL);

			iLen = WideCharToMultiByte(CP_ACP, 0, _szDBName, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szDBName, -1, szDBName, iLen, NULL, NULL);


			//---------------------------------
			// DB 연결
			//---------------------------------

			_pMySQL = mysql_real_connect(&_MySQL, szDBIP, szUser, szPassword, szDBName, _iDBPort, (char*)NULL, 0);
			if (_pMySQL == NULL)
			{
				//StringCchPrintfW(_szLastErrorMsg, sizeof(_szLastErrorMsg), L"%s", mysql_error(&_MySQL));
				SaveLastError();
				return false;
			}

			return true;
		}

		//////////////////////////////////////////////////////////////////////
		// MySQL DB 끊기
		//////////////////////////////////////////////////////////////////////
		bool Disconnect(void)
		{
			if (_pMySQL == NULL)
			{
				fwprintf(stderr, L"Mysql Not Connected");
				return false;
			}

			if (_pSqlResult != NULL)
				mysql_free_result(_pSqlResult);

			// DB 연결닫기
			mysql_close(_pMySQL);
		}


		//////////////////////////////////////////////////////////////////////
		// Query - 쿼리 날리고 결과셋 임시 보관
		// Query_Save - 쿼리만 날리고 결과셋은 저장하지 않음
		// 에러 발생시 로그(쿼리문 전체, 에러코드, 에러메시지)
		// 간단한 프로파일링 쿼리 실행시간 측정 ms->시간 초과시 로그
		//
		//////////////////////////////////////////////////////////////////////
		bool Query(CONST WCHAR* szStringFormat, va_list va)
		{
			//---------------------------------
			// 쿼리문 저장
			//---------------------------------
			
			HRESULT hResult;

			//va_start(va, szStringFormat);

			hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
			if (FAILED(hResult))
			{
				// TODO: 에러 처리 변경
				int* exit = NULL;
				*exit = 1;
			}
			//va_end(va);

			int iLen = WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, _szQueryUTF8, iLen, NULL, NULL);


			//---------------------------------
			// 쿼리 실행
			//---------------------------------
			_iLastError = mysql_query(_pMySQL, _szQueryUTF8);
			if (_iLastError != 0)
			{
				SaveLastError();
				return false;
			}

			//---------------------------------
			// 쿼리 결과 저장
			//---------------------------------
			_pSqlResult = mysql_store_result(_pMySQL);		// 결과 전체를 미리 가져옴
			if (NULL == _pSqlResult)
			{
				SaveLastError();
				return false;
			}

			return true;
		}

		bool Query_Save(CONST WCHAR* szStringFormat, va_list va)
		{
			//---------------------------------
			// 쿼리문 저장
			//---------------------------------
			HRESULT hResult;

			//va_start(va, szStringFormat);

			hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
			if (FAILED(hResult))
			{
				// TODO: 에러 처리 변경
				int* exit = NULL;
				*exit = 1;
			}
			//va_end(va);

			int iLen = WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, _szQueryUTF8, iLen, NULL, NULL);

			//---------------------------------
			// 쿼리 실행
			//---------------------------------
			_iLastError = mysql_query(_pMySQL, _szQueryUTF8);
			if (_iLastError != 0)
			{
				SaveLastError();
				return false;
			}
			
			return true;
		}


		//////////////////////////////////////////////////////////////////////
		// 쿼리를 날린 뒤에 결과 뽑아오기.
		//
		// 결과가 없다면 NULL 리턴.
		//////////////////////////////////////////////////////////////////////
		MYSQL_ROW FetchRow(void)
		{
			MYSQL_ROW sql_row = mysql_fetch_row(_pSqlResult);
			return sql_row;
		}

		//////////////////////////////////////////////////////////////////////
		// 한 쿼리에 대한 결과 모두 사용 후 정리.
		//////////////////////////////////////////////////////////////////////
		void FreeResult(void)
		{
			memset(_szQuery, 0, sizeof(_szQuery));
			memset(_szQueryUTF8, 0, sizeof(_szQueryUTF8));

			if (NULL != _pSqlResult)
				mysql_free_result(_pSqlResult);
		}

		//////////////////////////////////////////////////////////////////////
		// Error 얻기.한 쿼리에 대한 결과 모두 사용 후 정리.
		//////////////////////////////////////////////////////////////////////
		int			GetLastError(void) { return _iLastError; }
		WCHAR*		GetLastErrorMsg(void) { return _szLastErrorMsg; }

	private:

		//////////////////////////////////////////////////////////////////////
		// mysql 의 LastError 를 맴버변수로 저장한다.
		//////////////////////////////////////////////////////////////////////
		void SaveLastError(void)
		{
			int errorLen = (int)strlen(mysql_error(&_MySQL));
			int iLen = MultiByteToWideChar(CP_ACP, 0, mysql_error(&_MySQL), errorLen, NULL, NULL);
			MultiByteToWideChar(CP_ACP, 0, mysql_error(&_MySQL), errorLen, _szLastErrorMsg, iLen);

			fwprintf(stderr, L"Mysql query error : %s", _szLastErrorMsg);
		}

	private:

		//-------------------------------------------------------------
		// MySQL 연결객체 본체
		//-------------------------------------------------------------
		MYSQL		_MySQL;

		//-------------------------------------------------------------
		// MySQL 연결객체 포인터. 위 변수의 포인터임. 
		// 이 포인터의 null 여부로 연결상태 확인.
		//-------------------------------------------------------------
		MYSQL* _pMySQL;

		//-------------------------------------------------------------
		// 쿼리를 날린 뒤 Result 저장소.
		//
		//-------------------------------------------------------------
		MYSQL_RES* _pSqlResult;

		//-------------------------------------------------------------
		// DB 연결 정보
		//
		//-------------------------------------------------------------
		WCHAR		_szDBIP[16];
		WCHAR		_szDBUser[64];
		WCHAR		_szDBPassword[64];
		WCHAR		_szDBName[64];
		int			_iDBPort;


		//-------------------------------------------------------------
		// 쿼리 스트링(UTF-16)과 이를 UTF-8 변환한 스트링
		//
		//-------------------------------------------------------------
		WCHAR		_szQuery[eQUERY_MAX_LEN];
		char		_szQueryUTF8[eQUERY_MAX_LEN];

		//-------------------------------------------------------------
		// 쿼리 실행 에러 결과
		//
		//-------------------------------------------------------------
		int			_iLastError;
		WCHAR		_szLastErrorMsg[128];
	};

public:

	CDBConnectorTLS(CONST WCHAR* szDBIP, CONST WCHAR* szUser, CONST WCHAR* szPassword, CONST WCHAR* szDBName, int iDBPort); // szDBName는 스키마
	virtual		~CDBConnectorTLS();

	bool		Connect(void);
	bool		Disconnect(void);

	bool		Query(CONST WCHAR* szStringFormat, ...);
	bool		Query_Save(CONST WCHAR* szStringFormat, ...);	// DBWriter 스레드의 Save 쿼리 전용

	MYSQL_ROW	FetchRow(void);

	void		FreeResult(void);

	int			GetLastError(void);
	WCHAR*		GetLastErrorMsg(void);


private:
	//-------------------------------------------------------------
	// DB 연결 정보
	//
	//-------------------------------------------------------------
	WCHAR		_szDBIP[16];
	WCHAR		_szDBUser[64];
	WCHAR		_szDBPassword[64];
	WCHAR		_szDBName[64];
	int			_iDBPort;

	inline static DWORD _TlsNumConnector;
};

#endif