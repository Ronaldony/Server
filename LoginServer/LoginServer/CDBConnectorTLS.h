#ifndef __PROCADEMY_LIB_DBCONNECTOR__
#define __PROCADEMY_LIB_DBCONNECTOR__

//#include <windows.h>
//#include <strsafe.h>
#include "mysql/include/mysql.h"
#include "mysql/include/errmsg.h"

#pragma comment(lib, "mysql/lib/vs14/mysqlclient.lib")


/////////////////////////////////////////////////////////
// MySQL DB ���� Ŭ����
//
// �ܼ��ϰ� MySQL Connector �� ���� DB ���Ḹ �����Ѵ�.
//
// �����忡 �������� �����Ƿ� ���� �ؾ� ��.
// ���� �����忡�� ���ÿ� �̸� ����Ѵٸ� ������ ��.
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
			// DB ���� ����
			//
			//-------------------------------------------------------------

			memcpy_s(_szDBIP, sizeof(_szDBIP), szDBIP, wcslen(szDBIP) * sizeof(WCHAR));
			memcpy_s(_szDBUser, sizeof(_szDBUser), szUser, wcslen(szUser) * sizeof(WCHAR));
			memcpy_s(_szDBPassword, sizeof(_szDBPassword), szPassword, wcslen(szPassword) * sizeof(WCHAR));
			memcpy_s(_szDBName, sizeof(_szDBName), szDBName, wcslen(szDBName) * sizeof(WCHAR));
			_iDBPort = iDBPort;

			// �ʱ�ȭ
			mysql_init(&_MySQL);
		}

		~CDBConnector()
		{
			//---------------------------------
			// DB ���� ����
			//---------------------------------
			Disconnect();
		}

		//////////////////////////////////////////////////////////////////////
		// MySQL DB ����
		//////////////////////////////////////////////////////////////////////
		bool Connect(void)
		{
			// Disable SSL
			int sslmode = 1;
			mysql_options(&_MySQL, MYSQL_OPT_SSL_MODE, &sslmode);


			//---------------------------------
			// ���� ���� UTF-8 ��ȯ
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
			// DB ����
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
		// MySQL DB ����
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

			// DB ����ݱ�
			mysql_close(_pMySQL);
		}


		//////////////////////////////////////////////////////////////////////
		// Query - ���� ������ ����� �ӽ� ����
		// Query_Save - ������ ������ ������� �������� ����
		// ���� �߻��� �α�(������ ��ü, �����ڵ�, �����޽���)
		// ������ �������ϸ� ���� ����ð� ���� ms->�ð� �ʰ��� �α�
		//
		//////////////////////////////////////////////////////////////////////
		bool Query(CONST WCHAR* szStringFormat, va_list va)
		{
			//---------------------------------
			// ������ ����
			//---------------------------------
			
			HRESULT hResult;

			//va_start(va, szStringFormat);

			hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
			if (FAILED(hResult))
			{
				// TODO: ���� ó�� ����
				int* exit = NULL;
				*exit = 1;
			}
			//va_end(va);

			int iLen = WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, _szQueryUTF8, iLen, NULL, NULL);


			//---------------------------------
			// ���� ����
			//---------------------------------
			_iLastError = mysql_query(_pMySQL, _szQueryUTF8);
			if (_iLastError != 0)
			{
				SaveLastError();
				return false;
			}

			//---------------------------------
			// ���� ��� ����
			//---------------------------------
			_pSqlResult = mysql_store_result(_pMySQL);		// ��� ��ü�� �̸� ������
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
			// ������ ����
			//---------------------------------
			HRESULT hResult;

			//va_start(va, szStringFormat);

			hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
			if (FAILED(hResult))
			{
				// TODO: ���� ó�� ����
				int* exit = NULL;
				*exit = 1;
			}
			//va_end(va);

			int iLen = WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, _szQueryUTF8, iLen, NULL, NULL);

			//---------------------------------
			// ���� ����
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
		// ������ ���� �ڿ� ��� �̾ƿ���.
		//
		// ����� ���ٸ� NULL ����.
		//////////////////////////////////////////////////////////////////////
		MYSQL_ROW FetchRow(void)
		{
			MYSQL_ROW sql_row = mysql_fetch_row(_pSqlResult);
			return sql_row;
		}

		//////////////////////////////////////////////////////////////////////
		// �� ������ ���� ��� ��� ��� �� ����.
		//////////////////////////////////////////////////////////////////////
		void FreeResult(void)
		{
			memset(_szQuery, 0, sizeof(_szQuery));
			memset(_szQueryUTF8, 0, sizeof(_szQueryUTF8));

			if (NULL != _pSqlResult)
				mysql_free_result(_pSqlResult);
		}

		//////////////////////////////////////////////////////////////////////
		// Error ���.�� ������ ���� ��� ��� ��� �� ����.
		//////////////////////////////////////////////////////////////////////
		int			GetLastError(void) { return _iLastError; }
		WCHAR*		GetLastErrorMsg(void) { return _szLastErrorMsg; }

	private:

		//////////////////////////////////////////////////////////////////////
		// mysql �� LastError �� �ɹ������� �����Ѵ�.
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
		// MySQL ���ᰴü ��ü
		//-------------------------------------------------------------
		MYSQL		_MySQL;

		//-------------------------------------------------------------
		// MySQL ���ᰴü ������. �� ������ ��������. 
		// �� �������� null ���η� ������� Ȯ��.
		//-------------------------------------------------------------
		MYSQL* _pMySQL;

		//-------------------------------------------------------------
		// ������ ���� �� Result �����.
		//
		//-------------------------------------------------------------
		MYSQL_RES* _pSqlResult;

		//-------------------------------------------------------------
		// DB ���� ����
		//
		//-------------------------------------------------------------
		WCHAR		_szDBIP[16];
		WCHAR		_szDBUser[64];
		WCHAR		_szDBPassword[64];
		WCHAR		_szDBName[64];
		int			_iDBPort;


		//-------------------------------------------------------------
		// ���� ��Ʈ��(UTF-16)�� �̸� UTF-8 ��ȯ�� ��Ʈ��
		//
		//-------------------------------------------------------------
		WCHAR		_szQuery[eQUERY_MAX_LEN];
		char		_szQueryUTF8[eQUERY_MAX_LEN];

		//-------------------------------------------------------------
		// ���� ���� ���� ���
		//
		//-------------------------------------------------------------
		int			_iLastError;
		WCHAR		_szLastErrorMsg[128];
	};

public:

	CDBConnectorTLS(CONST WCHAR* szDBIP, CONST WCHAR* szUser, CONST WCHAR* szPassword, CONST WCHAR* szDBName, int iDBPort); // szDBName�� ��Ű��
	virtual		~CDBConnectorTLS();

	bool		Connect(void);
	bool		Disconnect(void);

	bool		Query(CONST WCHAR* szStringFormat, ...);
	bool		Query_Save(CONST WCHAR* szStringFormat, ...);	// DBWriter �������� Save ���� ����

	MYSQL_ROW	FetchRow(void);

	void		FreeResult(void);

	int			GetLastError(void);
	WCHAR*		GetLastErrorMsg(void);


private:
	//-------------------------------------------------------------
	// DB ���� ����
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