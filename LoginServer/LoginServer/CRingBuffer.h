#ifndef __CRINGBUFFER_H__
#define __CRINGBUFFER_H__
//#include <windows.h>

class CRingBuffer
{
public:
	enum
	{
		// �ݵ��!! ���� ������� ��� ��Ʈ�� 1�� ä���� ���θ� ������ ��
		// ��� �� ������������ ����
		BUF_SIZE_DEFAULT = 0x1FFF
	};
	CRingBuffer();
	~CRingBuffer();

	int GetBufferSize(void);
	LONG64 GetUseSize(void);
	int GetFreeSize(void);

	int	DirectEnqueueSize(void);
	int	DirectDequeueSize(void);

	// ��ȯ ���� ���Ͽ� ����غ���
	int Enqueue(char* chpData, int size);
	int Dequeue(char* chpDest, int size);
	int Peek(char* chpDest, int size);		// �������� ��� �б��

	void MoveRear(int size);
	void MoveFront(int size);

	void ClearBuffer(void);

	// ���� ����� ���� ��¿ �� ���� ť ������ ��ȯ
	char* GetBufferPtr(void);
	char* GetFrontBufferPtr(void);
	char* GetRearBufferPtr(void);
	//void Resize(int size);	// �ϴ� ����

	// ����ȭ ��ü �Լ�
	void LockSharedEnQ(void);
	void UnlockSharedEnQ(void);
	void LockExclusiveEnQ(void);
	void UnlockExclusiveEnQ(void);

	void LockSharedDeQ(void);
	void UnlockSharedDeQ(void);
	void LockExclusiveDeQ(void);
	void UnlockExclusiveDeQ(void);

private:
	// ��Ƽ ������� ����ȭ ��ü
	SRWLOCK _LockEnQ;
	SRWLOCK _LockDeQ;
	LONG64 alignas(64) _nowUseSize;
	int _totalSize;
	int _front;
	int _rear;
	char* _RingBuffer;

};

inline int CRingBuffer::GetBufferSize(void) { return (BUF_SIZE_DEFAULT + 1); }

inline LONG64 CRingBuffer::GetUseSize(void) { return _nowUseSize; }

inline int CRingBuffer::GetFreeSize(void) { return (int)(BUF_SIZE_DEFAULT - _nowUseSize); }

inline char* CRingBuffer::GetFrontBufferPtr(void) { return (_RingBuffer + _front); }

inline char* CRingBuffer::GetRearBufferPtr(void) { return (_RingBuffer + _rear); }

inline char* CRingBuffer::GetBufferPtr(void) { return (_RingBuffer); }

inline void CRingBuffer::ClearBuffer(void)
{
	_front = 0;
	_rear = 0;
	_nowUseSize = 0;
}

inline void CRingBuffer::LockSharedEnQ(void) { AcquireSRWLockShared(&_LockEnQ); }
inline void CRingBuffer::UnlockSharedEnQ(void) { ReleaseSRWLockShared(&_LockEnQ); }
inline void CRingBuffer::LockExclusiveEnQ(void) { AcquireSRWLockExclusive(&_LockEnQ); }
inline void CRingBuffer::UnlockExclusiveEnQ(void) { ReleaseSRWLockExclusive(&_LockEnQ); }

inline void CRingBuffer::LockSharedDeQ(void) { AcquireSRWLockShared(&_LockDeQ); }
inline void CRingBuffer::UnlockSharedDeQ(void) { ReleaseSRWLockShared(&_LockDeQ); }
inline void CRingBuffer::LockExclusiveDeQ(void) { AcquireSRWLockExclusive(&_LockDeQ); }
inline void CRingBuffer::UnlockExclusiveDeQ(void) { ReleaseSRWLockExclusive(&_LockDeQ); }

#endif



