#ifndef __DB_JOB_H__
#define __DB_JOB_H__

//#include <windows.h>
#include "CDBConnector.h"

class IDBJob
{
public:
	virtual void Exec(CDBConnector* DBConnection) = 0;
};


class CDBServerMonitorJob : public IDBJob
{
public:
	~CDBServerMonitorJob() {}
	void Exec(CDBConnector* DBConnection)
	{
		DBConnection->Query_Save(L"INSERT INTO logdb.monitorlog VALUES(NULL, NOW(), %d, %s, %d, %d, %d, %d)", _iServerNo, L"\'MonitorServer\'", _iType, _iValueAvr, _iValueMin, _iValueMax);
	}
	int _iType;
	int _iServerNo;
	int _iValueAvr;
	int _iValueMin;
	int _iValueMax;
};

#endif