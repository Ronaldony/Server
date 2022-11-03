#include <windows.h>
#include <strsafe.h>
#include "CDBConnectorTLS.h"

CDBConnectorTLS::CDBConnectorTLS(CONST WCHAR* szDBIP, CONST WCHAR* szUser, CONST WCHAR* szPassword, CONST WCHAR* szDBName, int iDBPort)
{
	_TlsNumConnector = TlsAlloc();

	//-------------------------------------------------------------
	// DB 연결 정보
	//
	//-------------------------------------------------------------

	memset(_szDBIP, 0, sizeof(_szDBIP));
	memset(_szDBUser, 0, sizeof(_szDBUser));
	memset(_szDBPassword, 0, sizeof(_szDBPassword));
	memset(_szDBName, 0, sizeof(_szDBName));

	memcpy_s(_szDBIP, sizeof(_szDBIP), szDBIP, wcslen(szDBIP) * sizeof(WCHAR));
	memcpy_s(_szDBUser, sizeof(_szDBUser), szUser, wcslen(szUser) * sizeof(WCHAR));
	memcpy_s(_szDBPassword, sizeof(_szDBPassword), szPassword, wcslen(szPassword) * sizeof(WCHAR));
	memcpy_s(_szDBName, sizeof(_szDBName), szDBName, wcslen(szDBName) * sizeof(WCHAR));
	_iDBPort = iDBPort;

	// 초기화
	//mysql_init(&_MySQL);
}

CDBConnectorTLS::~CDBConnectorTLS()
{
	//---------------------------------
	// DB 세션 종료
	//---------------------------------
	// Disconnect();
	// TOOD: 각 스레드 소멸자 어떻게 호출할 지 고민해보기
}

//////////////////////////////////////////////////////////////////////
// MySQL DB 연결
//////////////////////////////////////////////////////////////////////
bool CDBConnectorTLS::Connect(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
	{
		pConnector = new CDBConnector(_szDBIP, _szDBUser, _szDBPassword, _szDBName, _iDBPort);
		TlsSetValue(_TlsNumConnector, pConnector);
	}

	return pConnector->Connect();
}

//////////////////////////////////////////////////////////////////////
// MySQL DB 끊기
//////////////////////////////////////////////////////////////////////
bool CDBConnectorTLS::Disconnect(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
		return false;

	return pConnector->Disconnect();
}


//////////////////////////////////////////////////////////////////////
// Query - 쿼리 날리고 결과셋 임시 보관
// Query_Save - 쿼리만 날리고 결과셋은 저장하지 않음
// 에러 발생시 로그(쿼리문 전체, 에러코드, 에러메시지)
// 간단한 프로파일링 쿼리 실행시간 측정 ms->시간 초과시 로그
//
//////////////////////////////////////////////////////////////////////
bool CDBConnectorTLS::Query(CONST WCHAR* szStringFormat, ...)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
	{
		pConnector = new CDBConnector(_szDBIP, _szDBUser, _szDBPassword, _szDBName, _iDBPort);
		TlsSetValue(_TlsNumConnector, pConnector);
		pConnector->Connect();
	}

	va_list va;
	va_start(va, szStringFormat);
	bool bResult = pConnector->Query(szStringFormat, va);
	va_end(va);

	return bResult;
}

bool CDBConnectorTLS::Query_Save(CONST WCHAR* szStringFormat, ...)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
	{
		pConnector = new CDBConnector(_szDBIP, _szDBUser, _szDBPassword, _szDBName, _iDBPort);
		TlsSetValue(_TlsNumConnector, pConnector);
		pConnector->Connect();
	}

	va_list va;
	va_start(va, szStringFormat);
	bool bResult = pConnector->Query_Save(szStringFormat, va);
	va_end(va);

	return bResult;
}


//////////////////////////////////////////////////////////////////////
// 쿼리를 날린 뒤에 결과 뽑아오기.
//
// 결과가 없다면 NULL 리턴.
//////////////////////////////////////////////////////////////////////
MYSQL_ROW CDBConnectorTLS::FetchRow(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
		return NULL;

	return pConnector->FetchRow();
}

//////////////////////////////////////////////////////////////////////
// 한 쿼리에 대한 결과 모두 사용 후 정리.
//////////////////////////////////////////////////////////////////////
void CDBConnectorTLS::FreeResult(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
		return;

	pConnector->FreeResult();
}


int CDBConnectorTLS::GetLastError(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
		return false;

	return pConnector->GetLastError();
}

WCHAR* CDBConnectorTLS::GetLastErrorMsg(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
		return NULL;

	return pConnector->GetLastErrorMsg();
}