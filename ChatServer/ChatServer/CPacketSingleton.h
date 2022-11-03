#ifndef __CPACKET_SINGLETON_H__
#define __CPACKET_SINGLETON_H__
#include "MemoryPoolTLS.h"
#include "CNetProtocol.h"

class CPacket
{
public:
	friend class CMemoryPoolTLS<CPacket>;
	friend class CNetServer;
	friend class CLanClient;

	enum en_PACKET
	{
		eLAN_HEADER_SIZE	= 2,		// LAN ��� ������
		eWAN_HEADER_SIZE	= 5,		// WAN ��� ������
		eBUFFER_DEFAULT		= 4096		// �⺻ ���� ũ��
	};

	void Clear(void);

	inline static int	GetBufferSize(void);
	inline int			GetDataSize(void);
	inline char*		GetBufferPtr(void);
	inline char*		GetBufferReadPtr(void);

	int	MoveWritePos(int iSize);
	int	MoveReadPos(int iSize);

	int	GetData(char* chpDest, int iSize);
	int PutData(char* chpSrc, int iSrcSize);
	
	int Resize(int iSize);

	CPacket& operator = (CPacket& clSrcPacket);

	inline CPacket& operator << (char chValue);
	inline CPacket& operator << (unsigned char uchValue);
	
	inline CPacket& operator << (short shValue);
	inline CPacket& operator << (unsigned short ushValue);

	inline CPacket& operator << (int iValue);
	inline CPacket& operator << (long lValue);
	inline CPacket& operator << (unsigned long ulValue);
	inline CPacket& operator << (float fValue);
	
	inline CPacket& operator << (__int64 iValue);
	inline CPacket& operator << (double dValue);
	
	
	inline CPacket& operator >> (char& chValue);
	inline CPacket& operator >> (unsigned char& uchValue);
	
	inline CPacket& operator >> (short& shValue);
	inline CPacket& operator >> (unsigned short& ushValue);
	
	inline CPacket& operator >> (int& iValue);
	inline CPacket& operator >> (long& lValue);
	inline CPacket& operator >> (unsigned long& ulValue);
	inline CPacket& operator >> (float& fValue);
	
	inline CPacket& operator >> (__int64& iValue);
	inline CPacket& operator >> (double& dValue);

	// �޸�Ǯ TLS���� CPacket�� �Ҵ�
	static CPacket* Alloc(void) 
	{ 
		CPacket* retPacket = _CPacketPoolTLS.Alloc();

		retPacket->addRef();
		retPacket->Clear();
		InterlockedExchange64(&retPacket->_llEncodeFlag, false);

		return retPacket;
	}

	// �޸�Ǯ TLS�� CPacket�� ��ȯ
	inline static void Free(CPacket* pPacket)
	{ 
		if (0 == pPacket->subRef())
			_CPacketPoolTLS.Free(pPacket);
	}

	inline static void Free(CPacket** pPacket, int iNum)
	{
		for (int cnt = 0; cnt < iNum; cnt++)
		{
			if (0 == pPacket[cnt]->subRef())
				_CPacketPoolTLS.Free(pPacket[cnt]);
		}
	}

	// ���� ī��Ʈ ������
	inline void addRef() { InterlockedIncrement64(&_llRefCount); }
	inline LONG64 subRef() { return InterlockedDecrement64(&_llRefCount); }

	bool IsEncoded() { return (bool)_llEncodeFlag; }

	static LONG64 GetCountOfPoolAlloc(void) { return _CPacketPoolTLS.GetAllocCount(); }
	static LONG64 GetCountOfPoolTotalAlloc(void) { return _CPacketPoolTLS.GetChunkAllocCount(); }

private:
	CPacket();
	CPacket(int iPacketSize);
	~CPacket();

	inline int GetDataSizeWithLANHeader(void);
	inline int GetDataSizeWithWANHeader(void);

	inline char* GetLANHeaderPtr(void);
	inline char* GetWANHeaderPtr(void);

	int PutLANHeader(char* chpSrc, int iSrcSize);
	int PutWANHeader(char* chpSrc, int iSrcSize);

	void CalcCheckSum(BYTE* pbyChecksum, BYTE* pbyData, int iSize);
	void PacketEncode(BYTE byPacketKey, BYTE byCode);
	bool PacketDecode(st_NetHeader* header, BYTE byPacketKey, BYTE byCode);

private:
	LONG64 alignas(64)	_llRefCount;
	inline static int	_iBufferSize;			// �ִ� ���� ���� ������
	int		_iDataSize;				// Payload ũ��
	int		_read;					// Payload�� Read ��ġ
	int		_write;					// Payload�� Write ��ġ

	char*	_pchBuffer;				// Payload ���� ������
	char*	_pchLanHeaderBuffer;	// LAN ��� ���� ������
	char*	_pchWanHeaderBuffer;	// WAN ��� ���� ������

	LONG64 alignas(64) _llEncodeFlag;

	inline static CMemoryPoolTLS<CPacket> _CPacketPoolTLS;	// CPacket �޸�Ǯ TLS
};

//////////////////////////////////////////////////////////////////////////
// ���� ������ ���.
//
// Parameters: ����.
// Return: (int)��Ŷ ���� ������ ���.
//////////////////////////////////////////////////////////////////////////
int	CPacket::GetBufferSize(void) { return _iBufferSize; }

//////////////////////////////////////////////////////////////////////////
// ���� ������� ������ ���.
//
// Parameters: ����.
// Return: (int)������� ����Ÿ ������.
//////////////////////////////////////////////////////////////////////////
int	CPacket::GetDataSize(void) { return _iDataSize; }
int	CPacket::GetDataSizeWithLANHeader(void) { return _iDataSize + eLAN_HEADER_SIZE; }
int	CPacket::GetDataSizeWithWANHeader(void) { return _iDataSize + eWAN_HEADER_SIZE; }

//////////////////////////////////////////////////////////////////////////
// ���� ������ ���.
//
// Parameters: ����.
// Return: (char *)���� ������.
//////////////////////////////////////////////////////////////////////////
char* CPacket::GetBufferPtr(void) { return _pchBuffer; }
char* CPacket::GetBufferReadPtr(void) { return _pchBuffer + _read; }
char* CPacket::GetLANHeaderPtr(void) { return _pchLanHeaderBuffer; }
char* CPacket::GetWANHeaderPtr(void) { return _pchWanHeaderBuffer; }



/* ============================================================================= */
// ������ �����ε�
/* ============================================================================= */

//////////////////////////////////////////////////////////////////////////
// �ֱ�.	�� ���� Ÿ�Ը��� ��� ����.
//////////////////////////////////////////////////////////////////////////
CPacket& CPacket::operator << (char chValue)
{
	*(_pchBuffer + _write) = (char)chValue;
	_write += sizeof(char);
	_iDataSize += sizeof(char);

	return *this;
}

CPacket& CPacket::operator << (unsigned char uchValue)
{
	*(_pchBuffer + _write) = (char)uchValue;
	_write += sizeof(unsigned char);
	_iDataSize += sizeof(unsigned char);

	return *this;
}

CPacket& CPacket::operator << (short shValue)
{
	*((short*)(_pchBuffer + _write)) = shValue;
	_write += sizeof(short);
	_iDataSize += sizeof(short);

	return *this;
}

CPacket& CPacket::operator << (unsigned short ushValue)
{
	*((unsigned short*)(_pchBuffer + _write)) = ushValue;
	_write += sizeof(unsigned short);
	_iDataSize += sizeof(unsigned short);

	return *this;
}

CPacket& CPacket::operator << (int iValue)
{
	*((int*)(_pchBuffer + _write)) = iValue;
	_write += sizeof(int);
	_iDataSize += sizeof(int);

	return *this;
}
CPacket& CPacket::operator << (long lValue)
{
	*((long*)(_pchBuffer + _write)) = lValue;
	_write += sizeof(long);
	_iDataSize += sizeof(long);

	return *this;
}

CPacket& CPacket::operator << (unsigned long ulValue)
{
	*((unsigned long*)(_pchBuffer + _write)) = ulValue;
	_write += sizeof(unsigned long);
	_iDataSize += sizeof(unsigned long);

	return *this;
}

CPacket& CPacket::operator << (float fValue)
{
	*((float*)(_pchBuffer + _write)) = fValue;
	_write += sizeof(float);
	_iDataSize += sizeof(float);

	return *this;
}

CPacket& CPacket::operator << (__int64 iValue)
{
	*((__int64*)(_pchBuffer + _write)) = iValue;
	_write += sizeof(__int64);
	_iDataSize += sizeof(__int64);

	return *this;
}

CPacket& CPacket::operator << (double dValue)
{
	*((double*)(_pchBuffer + _write)) = dValue;
	_write += sizeof(double);
	_iDataSize += sizeof(double);

	return *this;
}


//////////////////////////////////////////////////////////////////////////
// ����.	�� ���� Ÿ�Ը��� ��� ����.
//////////////////////////////////////////////////////////////////////////

CPacket& CPacket::operator >> (char& chValue)
{
	chValue = *(_pchBuffer + _read);
	_read++;
	_iDataSize--;

	return *this;
}

CPacket& CPacket::operator >> (unsigned char& uchValue)
{
	uchValue = *(_pchBuffer + _read);
	_read++;
	_iDataSize--;

	return *this;
}

CPacket& CPacket::operator >> (short& shValue)
{
	shValue = *((short*)(_pchBuffer + _read));
	_read += sizeof(short);
	_iDataSize -= sizeof(short);

	return *this;
}

CPacket& CPacket::operator >> (unsigned short& ushValue)
{
	ushValue = *((unsigned short*)(_pchBuffer + _read));
	_read += sizeof(unsigned short);
	_iDataSize -= sizeof(unsigned short);

	return *this;
}

CPacket& CPacket::operator >> (int& iValue)
{
	iValue = *((int*)(_pchBuffer + _read));
	_read += sizeof(int);
	_iDataSize -= sizeof(int);

	return *this;
}

CPacket& CPacket::operator >> (long& lValue)
{
	lValue = *((long*)(_pchBuffer + _read));
	_read += sizeof(long);
	_iDataSize -= sizeof(long);

	return *this;
}

CPacket& CPacket::operator >> (unsigned long& ulValue)
{
	ulValue = *((unsigned long*)(_pchBuffer + _read));
	_read += sizeof(unsigned long);
	_iDataSize -= sizeof(unsigned long);

	return *this;
}

CPacket& CPacket::operator >> (float& fValue)
{
	fValue = *((float*)(_pchBuffer + _read));
	_read += sizeof(float);
	_iDataSize -= sizeof(float);

	return *this;
}

CPacket& CPacket::operator >> (__int64& iValue)
{
	iValue = *((__int64*)(_pchBuffer + _read));
	_read += sizeof(__int64);
	_iDataSize -= sizeof(__int64);

	return *this;
}

CPacket& CPacket::operator >> (double& dValue)
{
	dValue = *((double*)(_pchBuffer + _read));
	_read += sizeof(double);
	_iDataSize -= sizeof(double);

	return *this;
}

#endif
