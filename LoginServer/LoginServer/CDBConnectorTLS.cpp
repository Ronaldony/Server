#include <windows.h>
#include <strsafe.h>
#include "CDBConnectorTLS.h"

CDBConnectorTLS::CDBConnectorTLS(CONST WCHAR* szDBIP, CONST WCHAR* szUser, CONST WCHAR* szPassword, CONST WCHAR* szDBName, int iDBPort)
{
	_TlsNumConnector = TlsAlloc();

	//-------------------------------------------------------------
	// DB ���� ����
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

	// �ʱ�ȭ
	//mysql_init(&_MySQL);
}

CDBConnectorTLS::~CDBConnectorTLS()
{
	//---------------------------------
	// DB ���� ����
	//---------------------------------
	// Disconnect();
	// TOOD: �� ������ �Ҹ��� ��� ȣ���� �� ����غ���
}

//////////////////////////////////////////////////////////////////////
// MySQL DB ����
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
// MySQL DB ����
//////////////////////////////////////////////////////////////////////
bool CDBConnectorTLS::Disconnect(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
		return false;

	return pConnector->Disconnect();
}


//////////////////////////////////////////////////////////////////////
// Query - ���� ������ ����� �ӽ� ����
// Query_Save - ������ ������ ������� �������� ����
// ���� �߻��� �α�(������ ��ü, �����ڵ�, �����޽���)
// ������ �������ϸ� ���� ����ð� ���� ms->�ð� �ʰ��� �α�
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
// ������ ���� �ڿ� ��� �̾ƿ���.
//
// ����� ���ٸ� NULL ����.
//////////////////////////////////////////////////////////////////////
MYSQL_ROW CDBConnectorTLS::FetchRow(void)
{
	CDBConnector* pConnector = (CDBConnector*)TlsGetValue(_TlsNumConnector);

	if (NULL == pConnector)
		return NULL;

	return pConnector->FetchRow();
}

//////////////////////////////////////////////////////////////////////
// �� ������ ���� ��� ��� ��� �� ����.
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