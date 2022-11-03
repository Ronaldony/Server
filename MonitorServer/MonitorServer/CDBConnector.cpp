#include <windows.h>
#include <strsafe.h>
#include "CDBConnector.h"


CDBConnector::CDBConnector()
	:_pMySQL(nullptr), _pSqlResult(nullptr)
{
	memset(_szQuery, 0, sizeof(_szQuery));
	memset(_szQueryUTF8, 0, sizeof(_szQueryUTF8));
	_iLastError = 0;
	memset(_szLastErrorMsg, 0, sizeof(_szLastErrorMsg));


	// �ʱ�ȭ
	mysql_init(&_MySQL);
}

CDBConnector::~CDBConnector()
{
	//---------------------------------
	// DB ���� ����
	//---------------------------------
	Disconnect();
}

//////////////////////////////////////////////////////////////////////
// MySQL DB ����
//////////////////////////////////////////////////////////////////////
bool CDBConnector::Connect(CONST WCHAR* szDBIP, CONST WCHAR* szUser, CONST WCHAR* szPassword, CONST WCHAR* szDBName, int iDBPort)
{
	//-------------------------------------------------------------
	// DB ���� ����
	//
	//-------------------------------------------------------------

	memcpy_s(_szDBIP, sizeof(_szDBIP), szDBIP, wcslen(szDBIP) * sizeof(WCHAR));
	memcpy_s(_szDBUser, sizeof(_szDBUser), szUser, wcslen(szUser) * sizeof(WCHAR));
	memcpy_s(_szDBPassword, sizeof(_szDBPassword), szPassword, wcslen(szPassword) * sizeof(WCHAR));
	memcpy_s(_szDBName, sizeof(_szDBName), szDBName, wcslen(szDBName) * sizeof(WCHAR));
	_iDBPort = iDBPort;

	// Disable SSL
	int sslmode = 1;
	mysql_options(&_MySQL, MYSQL_OPT_SSL_MODE, &sslmode);

	
	//---------------------------------
	// ���� ���� UTF-8 ��ȯ
	//---------------------------------

	int iLen;
	char szDBIPConn[256] = { 0, };
	char szUserConn[256] = { 0, };
	char szPasswordConn[256] = { 0, };
	char szDBNameConn[256] = { 0, };

	iLen = WideCharToMultiByte(CP_ACP, 0, _szDBIP, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_ACP, 0, _szDBIP, -1, szDBIPConn, iLen, NULL, NULL);

	iLen = WideCharToMultiByte(CP_ACP, 0, _szDBUser, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_ACP, 0, _szDBUser, -1, szUserConn, iLen, NULL, NULL);

	iLen = WideCharToMultiByte(CP_ACP, 0, _szDBPassword, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_ACP, 0, _szDBPassword, -1, szPasswordConn, iLen, NULL, NULL);

	iLen = WideCharToMultiByte(CP_ACP, 0, _szDBName, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_ACP, 0, _szDBName, -1, szDBNameConn, iLen, NULL, NULL);


	//---------------------------------
	// DB ����
	//---------------------------------

	_pMySQL = mysql_real_connect(&_MySQL, szDBIPConn, szUserConn, szPasswordConn, szDBNameConn, _iDBPort, (char*)NULL, 0);
	if (_pMySQL == NULL)
	{
		SaveLastError();
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////
// MySQL DB ����
//////////////////////////////////////////////////////////////////////
bool CDBConnector::Disconnect(void)
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

	return true;
}


//////////////////////////////////////////////////////////////////////
// Query - ���� ������ ����� �ӽ� ����
// Query_Save - ������ ������ ������� �������� ����
// ���� �߻��� �α�(������ ��ü, �����ڵ�, �����޽���)
// ������ �������ϸ� ���� ����ð� ���� ms->�ð� �ʰ��� �α�
//
//////////////////////////////////////////////////////////////////////
bool CDBConnector::Query(CONST WCHAR* szStringFormat, ...)
{
	//---------------------------------
	// ������ ����
	//---------------------------------
	va_list va;
	HRESULT hResult;
	
	va_start(va, szStringFormat);

	hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
	if (FAILED(hResult))
	{
		// TODO: ���� ó�� ����
		int* exit = NULL;
		*exit = 1;
	}
	va_end(va);

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
}

bool CDBConnector::Query_Save(CONST WCHAR* szStringFormat, ...)
{
	//---------------------------------
	// ������ ����
	//---------------------------------
	va_list va;
	HRESULT hResult;

	va_start(va, szStringFormat);

	hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
	if (FAILED(hResult))
	{
		// TODO: ���� ó�� ����
		int* exit = NULL;
		*exit = 1;
	}
	va_end(va);

	int iLen = WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, _szQueryUTF8, iLen, NULL, NULL);

	//---------------------------------
	// ���� ����
	//---------------------------------
	
	// ����� ����
	FreeResult();
	
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
MYSQL_ROW CDBConnector::FetchRow(void)
{
	MYSQL_ROW sql_row = mysql_fetch_row(_pSqlResult);
	return sql_row;
}

//////////////////////////////////////////////////////////////////////
// �� ������ ���� ��� ��� ��� �� ����.
//////////////////////////////////////////////////////////////////////
void CDBConnector::FreeResult(void)
{
	if (NULL != _pSqlResult)
		mysql_free_result(_pSqlResult);
}

//////////////////////////////////////////////////////////////////////
// mysql �� LastError �� �ɹ������� �����Ѵ�.
//////////////////////////////////////////////////////////////////////
void CDBConnector::SaveLastError(void)
{
	int errLen = (int)strlen(mysql_error(&_MySQL));
	int iLen = MultiByteToWideChar(CP_ACP, 0, mysql_error(&_MySQL), errLen, NULL, NULL);
	MultiByteToWideChar(CP_ACP, 0, mysql_error(&_MySQL), errLen, _szLastErrorMsg, iLen);

	fwprintf(stderr, L"Mysql query error : %s", _szLastErrorMsg);
}
