#include <windows.h>
#include <new.h>
#include "CPacketSingleton.h"

#pragma warning(disable:26495)

CPacket::CPacket()
	: _iDataSize(0), _write(0), _read(0)
{
	_iBufferSize = eBUFFER_DEFAULT;
	_pchWanHeaderBuffer = new char[eBUFFER_DEFAULT];
	_pchLanHeaderBuffer = _pchWanHeaderBuffer + (eWAN_HEADER_SIZE - eLAN_HEADER_SIZE);
	_pchBuffer = _pchWanHeaderBuffer + eWAN_HEADER_SIZE;
}

CPacket::CPacket(int iPacketSize)
	: _iDataSize(0), _write(0), _read(0)
{
	_iBufferSize = iPacketSize;
	_pchWanHeaderBuffer = new char[iPacketSize];
	_pchLanHeaderBuffer = _pchWanHeaderBuffer + (eWAN_HEADER_SIZE - eLAN_HEADER_SIZE);
	_pchBuffer = _pchWanHeaderBuffer + eWAN_HEADER_SIZE;
}

CPacket::~CPacket()
{
	delete[] _pchWanHeaderBuffer;
}

//////////////////////////////////////////////////////////////////////////
// ��Ŷ û��.
//
// Parameters: ����.
// Return: ����.
//////////////////////////////////////////////////////////////////////////
void CPacket::Clear(void)
{
	_iDataSize = 0;
	_write = 0;
	_read = 0;
}

//////////////////////////////////////////////////////////////////////////
// ���� Pos �̵�. (�����̵��� �ȵ�)
// GetBufferPtr �Լ��� �̿��Ͽ� �ܺο��� ������ ���� ������ ������ ��� ���. 
//
// Parameters: (int) �̵� ������.
// Return: (int) �̵��� ������.
//////////////////////////////////////////////////////////////////////////
int	CPacket::MoveWritePos(int iSize)
{
	if ((iSize + _write) >= _iBufferSize)
		return 0;

	_write += iSize;
	_iDataSize += iSize;

	return iSize;
}

int	CPacket::MoveReadPos(int iSize)
{
	int pos = _read + iSize;

	// ���� ��ġ �ʰ�
	if (pos >= _write)
		return 0;

	_read = pos;
	_iDataSize -= iSize;

	return iSize;
}

//////////////////////////////////////////////////////////////////////////
// ����Ÿ ���.
//
// Parameters: (char *)Dest ������. (int)Size.
// Return: (int)������ ������.
//////////////////////////////////////////////////////////////////////////
int	CPacket::GetData(char* chpDest, int iSize)
{
	int cpyCnt;

	if (iSize > _iDataSize)
		cpyCnt = _iDataSize;
	else
		cpyCnt = iSize;

	memcpy_s(chpDest, iSize, _pchBuffer + _read, cpyCnt);

	_read += cpyCnt;
	_iDataSize -= cpyCnt;

	return cpyCnt;
}

//////////////////////////////////////////////////////////////////////////
// ����Ÿ ����.
//
// Parameters: (char *)Src ������. (int)SrcSize.
// Return: (int)������ ������.
//////////////////////////////////////////////////////////////////////////
int	CPacket::PutData(char* chpSrc, int iSrcSize)
{
	if ((iSrcSize + _write) >= _iBufferSize)
		return 0;
	
	memcpy_s(_pchBuffer + _write, _iBufferSize - _write, chpSrc, iSrcSize);
	_write += iSrcSize;
	_iDataSize += iSrcSize;

	return iSrcSize;
}

//////////////////////////////////////////////////////////////////////////
// LAN ��� ����
//
// Parameters: (char *)Src ������. (int)SrcSize.
// Return: (int)������ ������.
//////////////////////////////////////////////////////////////////////////
int CPacket::PutLANHeader(char* chpSrc, int iSrcSize)
{
	memcpy_s(_pchLanHeaderBuffer, iSrcSize, chpSrc, iSrcSize);
	return iSrcSize;
}

//////////////////////////////////////////////////////////////////////////
// WAN ��� ����
//
// Parameters: (char *)Src ������. (int)SrcSize.
// Return: (int)������ ������.
//////////////////////////////////////////////////////////////////////////
int CPacket::PutWANHeader(char* chpSrc, int iSrcSize)
{
	memcpy_s(_pchWanHeaderBuffer, iSrcSize, chpSrc, iSrcSize);
	return iSrcSize;
}

//////////////////////////////////////////////////////////////////////////
// üũ�� ���
//
// Parameters: (BYTE*)üũ�� �� ��ȯ �ּ�, (BYTE*)üũ�� ��� ��� �ּ�, (int)üũ�� ��� ��� ũ��
// Return: ����
//////////////////////////////////////////////////////////////////////////
void CPacket::CalcCheckSum(BYTE* pbyChecksum, BYTE* pbyData, int iSize)
{
	for (int cnt = 0; cnt < iSize; cnt++)
		*pbyChecksum += pbyData[cnt];

	return;
}

//////////////////////////////////////////////////////////////////////////
// ��Ŷ ���ڵ�
//
// Parameters: (BYTE*)���ڵ� ��� �ּ�, (int)���ڵ� ��� ũ��, (BYTE)���� Ű
// Return: ����
//////////////////////////////////////////////////////////////////////////
void CPacket::PacketEncode(BYTE byPacketKey, BYTE byCode)
{
	// �̹� ���ڵ��� ��Ȳ
	if (0 != InterlockedExchange64(&_llEncodeFlag, true))
		return;

	BYTE byPreP;
	BYTE byPreE;
	BYTE byNowP;
	BYTE byNowE;

	// ��� ����
	BYTE byChecksum = 0;
	CalcCheckSum(&byChecksum, (BYTE*)GetBufferPtr(), GetDataSize());

	st_NetHeader header;
	BYTE byRandKey = (BYTE)rand();

	header.Code = byCode;
	header.Len = GetDataSize();
	header.RandKey = byRandKey;
	header.CheckSum = byChecksum;

	// üũ�� ���ڵ�
	byNowP = header.CheckSum ^ (byRandKey + 1);
	byNowE = byNowP ^ (byPacketKey + 1);
	header.CheckSum = byNowE;

	// ������ ���ڵ�
	BYTE* pbyData = (BYTE*)GetBufferPtr();

	//byNowP = pbyData[0] ^ (byRandKey + 1);
	//byNowE = byNowP ^ (byPacketKey + 1);
	//pbyData[0] = byNowE;

	for (int cnt = 0; cnt < GetDataSize(); cnt++)
	{
		byPreP = byNowP;
		byPreE = byNowE;

		byNowP = pbyData[cnt] ^ (byPreP + byRandKey + cnt + 2);
		byNowE = byNowP ^ (byPreE + byPacketKey + cnt + 2);
		pbyData[cnt] = byNowE;
	}

	PutWANHeader((char*)&header, sizeof(header));
}

//////////////////////////////////////////////////////////////////////////
// ��Ŷ ���ڵ�
//
// Parameters: (BYTE*)���ڵ� ��� �ּ�, (int)���ڵ� ��� ũ��, (BYTE)���� Ű
// Return: ����
//////////////////////////////////////////////////////////////////////////
bool CPacket::PacketDecode(st_NetHeader* header, BYTE byPacketKey, BYTE byCode)
{
	BYTE byPreP;
	BYTE byPreE;
	BYTE byNowP;
	BYTE byNowE;

	// ��ȣȭ 
	// ���� ��Ģ1: x^x = 0, x^0 = x;
	// ���� ��Ģ2: if a=b, then a^c=b^c
	
	// ��� ���ڵ�
	byNowE = header->CheckSum;
	byNowP = byNowE ^ (byPacketKey + 1);
	header->CheckSum = byNowP ^ (header->RandKey + 1); // ���� Ű

	// ������ ���ڵ�
	BYTE* pbyData = (BYTE*)GetBufferPtr();

	//byNowE = pbyData[0];
	//byNowP = byNowE ^ (byPacketKey + 1);
	//pbyData[0] = byNowP ^ (header->RandKey + 1); // ���� Ű

	for (int cnt = 0; cnt < GetDataSize(); cnt++)
	{
		byPreP = byNowP;
		byPreE = byNowE;

		byNowE = pbyData[cnt];
		byNowP = byNowE ^ (byPreE + byPacketKey + cnt + 2);
		pbyData[cnt] = byNowP ^ (byPreP + header->RandKey + cnt + 2);
	}

	BYTE byDataChecksum = 0;
	CalcCheckSum(&byDataChecksum, (BYTE*)GetBufferPtr(), GetDataSize());

	if (byDataChecksum != header->CheckSum)
		return false;

	return true;
}

/* ============================================================================= */
// ������ �����ε�
/* ============================================================================= */
CPacket& CPacket::operator = (CPacket& clSrcPacket)
{
	this->_iBufferSize = clSrcPacket._iBufferSize;
	this->_iDataSize = clSrcPacket._iDataSize;
	this->_read = clSrcPacket._read;
	this->_write = clSrcPacket._write;

	if (_pchBuffer != NULL)
		delete[] _pchBuffer;

	_pchBuffer = new char[this->_iBufferSize];
	memcpy_s(this->_pchBuffer, this->_iBufferSize, clSrcPacket._pchBuffer, clSrcPacket._iBufferSize);

	return *this;
}

