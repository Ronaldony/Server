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
// 패킷 청소.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void CPacket::Clear(void)
{
	_iDataSize = 0;
	_write = 0;
	_read = 0;
}

//////////////////////////////////////////////////////////////////////////
// 버퍼 Pos 이동. (음수이동은 안됨)
// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
//
// Parameters: (int) 이동 사이즈.
// Return: (int) 이동된 사이즈.
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

	// 쓰기 위치 초과
	if (pos >= _write)
		return 0;

	_read = pos;
	_iDataSize -= iSize;

	return iSize;
}

//////////////////////////////////////////////////////////////////////////
// 데이타 얻기.
//
// Parameters: (char *)Dest 포인터. (int)Size.
// Return: (int)복사한 사이즈.
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
// 데이타 삽입.
//
// Parameters: (char *)Src 포인터. (int)SrcSize.
// Return: (int)복사한 사이즈.
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
// LAN 헤더 삽입
//
// Parameters: (char *)Src 포인터. (int)SrcSize.
// Return: (int)복사한 사이즈.
//////////////////////////////////////////////////////////////////////////
int CPacket::PutLANHeader(char* chpSrc, int iSrcSize)
{
	memcpy_s(_pchLanHeaderBuffer, iSrcSize, chpSrc, iSrcSize);
	return iSrcSize;
}

//////////////////////////////////////////////////////////////////////////
// WAN 헤더 삽입
//
// Parameters: (char *)Src 포인터. (int)SrcSize.
// Return: (int)복사한 사이즈.
//////////////////////////////////////////////////////////////////////////
int CPacket::PutWANHeader(char* chpSrc, int iSrcSize)
{
	memcpy_s(_pchWanHeaderBuffer, iSrcSize, chpSrc, iSrcSize);
	return iSrcSize;
}

//////////////////////////////////////////////////////////////////////////
// 체크섬 계산
//
// Parameters: (BYTE*)체크섬 값 반환 주소, (BYTE*)체크섬 계산 대상 주소, (int)체크섬 계산 대상 크기
// Return: 없음
//////////////////////////////////////////////////////////////////////////
void CPacket::CalcCheckSum(BYTE* pbyChecksum, BYTE* pbyData, int iSize)
{
	for (int cnt = 0; cnt < iSize; cnt++)
		*pbyChecksum += pbyData[cnt];

	return;
}

//////////////////////////////////////////////////////////////////////////
// 패킷 인코딩
//
// Parameters: (BYTE*)인코딩 대상 주소, (int)인코딩 대상 크기, (BYTE)고정 키
// Return: 없음
//////////////////////////////////////////////////////////////////////////
void CPacket::PacketEncode(BYTE byPacketKey, BYTE byCode)
{
	// 이미 인코딩된 상황
	if (0 != InterlockedExchange64(&_llEncodeFlag, true))
		return;

	BYTE byPreP;
	BYTE byPreE;
	BYTE byNowP;
	BYTE byNowE;

	// 헤더 생성
	BYTE byChecksum = 0;
	CalcCheckSum(&byChecksum, (BYTE*)GetBufferPtr(), GetDataSize());

	st_NetHeader header;
	BYTE byRandKey = (BYTE)rand();

	header.Code = byCode;
	header.Len = GetDataSize();
	header.RandKey = byRandKey;
	header.CheckSum = byChecksum;

	// 체크섬 인코딩
	byNowP = header.CheckSum ^ (byRandKey + 1);
	byNowE = byNowP ^ (byPacketKey + 1);
	header.CheckSum = byNowE;

	// 데이터 인코딩
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
// 패킷 디코딩
//
// Parameters: (BYTE*)디코딩 대상 주소, (int)디코딩 대상 크기, (BYTE)고정 키
// Return: 없음
//////////////////////////////////////////////////////////////////////////
bool CPacket::PacketDecode(st_NetHeader* header, BYTE byPacketKey, BYTE byCode)
{
	BYTE byPreP;
	BYTE byPreE;
	BYTE byNowP;
	BYTE byNowE;

	// 복호화 
	// 적용 법칙1: x^x = 0, x^0 = x;
	// 적용 법칙2: if a=b, then a^c=b^c
	
	// 헤더 디코딩
	byNowE = header->CheckSum;
	byNowP = byNowE ^ (byPacketKey + 1);
	header->CheckSum = byNowP ^ (header->RandKey + 1); // 랜덤 키

	// 데이터 디코딩
	BYTE* pbyData = (BYTE*)GetBufferPtr();

	//byNowE = pbyData[0];
	//byNowP = byNowE ^ (byPacketKey + 1);
	//pbyData[0] = byNowP ^ (header->RandKey + 1); // 랜덤 키

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
// 연산자 오버로딩
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

