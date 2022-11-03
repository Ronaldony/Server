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


	// 초기화
	mysql_init(&_MySQL);
}

CDBConnector::~CDBConnector()
{
	//---------------------------------
	// DB 세션 종료
	//---------------------------------
	Disconnect();
}

//////////////////////////////////////////////////////////////////////
// MySQL DB 연결
//////////////////////////////////////////////////////////////////////
bool CDBConnector::Connect(CONST WCHAR* szDBIP, CONST WCHAR* szUser, CONST WCHAR* szPassword, CONST WCHAR* szDBName, int iDBPort)
{
	//-------------------------------------------------------------
	// DB 연결 정보
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
	// 연결 정보 UTF-8 변환
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
	// DB 연결
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
// MySQL DB 끊기
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

	// DB 연결닫기
	mysql_close(_pMySQL);

	return true;
}


//////////////////////////////////////////////////////////////////////
// Query - 쿼리 날리고 결과셋 임시 보관
// Query_Save - 쿼리만 날리고 결과셋은 저장하지 않음
// 에러 발생시 로그(쿼리문 전체, 에러코드, 에러메시지)
// 간단한 프로파일링 쿼리 실행시간 측정 ms->시간 초과시 로그
//
//////////////////////////////////////////////////////////////////////
bool CDBConnector::Query(CONST WCHAR* szStringFormat, ...)
{
	//---------------------------------
	// 쿼리문 저장
	//---------------------------------
	va_list va;
	HRESULT hResult;
	
	va_start(va, szStringFormat);

	hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
	if (FAILED(hResult))
	{
		// TODO: 에러 처리 변경
		int* exit = NULL;
		*exit = 1;
	}
	va_end(va);

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
}

bool CDBConnector::Query_Save(CONST WCHAR* szStringFormat, ...)
{
	//---------------------------------
	// 쿼리문 저장
	//---------------------------------
	va_list va;
	HRESULT hResult;

	va_start(va, szStringFormat);

	hResult = StringCchVPrintfW(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
	if (FAILED(hResult))
	{
		// TODO: 에러 처리 변경
		int* exit = NULL;
		*exit = 1;
	}
	va_end(va);

	int iLen = WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_ACP, 0, _szQuery, -1, _szQueryUTF8, iLen, NULL, NULL);

	//---------------------------------
	// 쿼리 실행
	//---------------------------------
	
	// 결과셋 해제
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
// 쿼리를 날린 뒤에 결과 뽑아오기.
//
// 결과가 없다면 NULL 리턴.
//////////////////////////////////////////////////////////////////////
MYSQL_ROW CDBConnector::FetchRow(void)
{
	MYSQL_ROW sql_row = mysql_fetch_row(_pSqlResult);
	return sql_row;
}

//////////////////////////////////////////////////////////////////////
// 한 쿼리에 대한 결과 모두 사용 후 정리.
//////////////////////////////////////////////////////////////////////
void CDBConnector::FreeResult(void)
{
	if (NULL != _pSqlResult)
		mysql_free_result(_pSqlResult);
}

//////////////////////////////////////////////////////////////////////
// mysql 의 LastError 를 맴버변수로 저장한다.
//////////////////////////////////////////////////////////////////////
void CDBConnector::SaveLastError(void)
{
	int errLen = (int)strlen(mysql_error(&_MySQL));
	int iLen = MultiByteToWideChar(CP_ACP, 0, mysql_error(&_MySQL), errLen, NULL, NULL);
	MultiByteToWideChar(CP_ACP, 0, mysql_error(&_MySQL), errLen, _szLastErrorMsg, iLen);

	fwprintf(stderr, L"Mysql query error : %s", _szLastErrorMsg);
}
