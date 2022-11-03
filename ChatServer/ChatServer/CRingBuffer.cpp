#include <windows.h>
#include "CRingBuffer.h"

CRingBuffer::CRingBuffer(): _nowUseSize(0), _totalSize(BUF_SIZE_DEFAULT + 1)
{
	//_RingBuffer = new char[_totalSize];
	_RingBuffer = (char*)malloc(BUF_SIZE_DEFAULT + 1);
	_front = 0;
	_rear = 0;

	InitializeSRWLock(&_LockEnQ);
	InitializeSRWLock(&_LockDeQ);
}

CRingBuffer::~CRingBuffer()
{
	free(_RingBuffer);
	//if (_RingBuffer != NULL)
	//	delete [] _RingBuffer;
}

int	CRingBuffer::DirectEnqueueSize(void)
{
	//int front = _front;
	//int rear = _rear;

	if (_front > _rear)
	{
		return (_front - _rear - 1);
	}
	else // if (_front <= _rear)
	{
		if (_front == 0)
			return (BUF_SIZE_DEFAULT - _rear);
		else
			return (BUF_SIZE_DEFAULT + 1 - _rear);
	}
}

int	CRingBuffer::DirectDequeueSize(void)
{
	//int front = _front;
	//int rear = _rear;

	if (_front > _rear)
	{
		return (BUF_SIZE_DEFAULT + 1 - _front);
	}
	else // if (_front <= _rear)
	{
		return (_rear - _front);
	}
}

// 반환 값에 대하여 고민해보기
int CRingBuffer::Enqueue(char* chpData, int size)
{
	if (size <= 0)
		return 0;

	//int rear = _rear;
	//int front = _front;
	int retval = 0;
	int iEnQSize;

	// DirectEnqueueSize
	if (_front > _rear)
	{
		iEnQSize = _front - _rear - 1;
	}
	else // if (_front <= _rear)
	{
		if (_front == 0)
			iEnQSize = BUF_SIZE_DEFAULT - _rear;
		else
			iEnQSize = BUF_SIZE_DEFAULT + 1 - _rear;
	}

	if (size > iEnQSize)
	{
		memcpy_s(_RingBuffer + _rear, iEnQSize, chpData, iEnQSize);
		_rear = (_rear + iEnQSize) & BUF_SIZE_DEFAULT;

		retval = iEnQSize;

		size -= iEnQSize;

		if (_front > _rear)
		{
			iEnQSize = _front - _rear - 1;
		}
		else // if (_front <= _rear)
		{
			if (_front == 0)
				iEnQSize = BUF_SIZE_DEFAULT - _rear;
			else
				iEnQSize = BUF_SIZE_DEFAULT + 1 - _rear;
		}

		if (size > iEnQSize)
			size = iEnQSize;
	}

	memcpy_s(_RingBuffer + _rear, size, chpData + retval, size);
	_rear = (_rear + size) & BUF_SIZE_DEFAULT;
	retval += size;

	//_rear = rear;
	//InterlockedAdd64((LONG64*)&_nowUseSize, retval);
	_nowUseSize += retval;

	return retval;
}

int CRingBuffer::Dequeue(char* chpDest, int size)
{
	if (size <= 0)
		return 0;

	//int rear = _rear;
	//int front = _front;
	int retval = 0;
	int iDeQSize;

	if (_front > _rear)
		iDeQSize = BUF_SIZE_DEFAULT + 1 - _front;
	else // if (_front <= _rear)
		iDeQSize = _rear - _front;

	if (size > iDeQSize)
	{
		retval = iDeQSize;

		memcpy_s(chpDest, iDeQSize, _RingBuffer + _front, iDeQSize);
		_front = (_front + iDeQSize) & BUF_SIZE_DEFAULT;

		size -= iDeQSize;

		if (_front > _rear)
			iDeQSize = BUF_SIZE_DEFAULT + 1 - _front;
		else // if (_front <= _rear)
			iDeQSize = _rear - _front;

		if (size > iDeQSize)
			size = iDeQSize;
	}

	memcpy_s(chpDest + retval, size, _RingBuffer + _front, size);
	_front = (_front + size) & BUF_SIZE_DEFAULT;
	retval += size;

	//_front = front;
	//InterlockedAdd64((LONG64*)&_nowUseSize, -retval);
	_nowUseSize -= retval;

	return retval;
}

int CRingBuffer::Peek(char* chpDest, int size)
{
	if (size <= 0)
		return 0;

	int rear = _rear;
	int front = _front;
	int iDeQSize;
	int retval = 0;

	if (front > rear)
		iDeQSize = BUF_SIZE_DEFAULT + 1 - front;
	else // if (_front <= _rear)
		iDeQSize = rear - front;

	if (size > iDeQSize)
	{
		memcpy_s(chpDest, iDeQSize, _RingBuffer + front, iDeQSize);
		front = (front + iDeQSize) & BUF_SIZE_DEFAULT;
		retval = iDeQSize;

		size -= iDeQSize;

		if (front > rear)
			iDeQSize = BUF_SIZE_DEFAULT + 1 - front;
		else // if (_front <= _rear)
			iDeQSize = rear - front;

		if (size > iDeQSize)
			size = iDeQSize;
	}

	memcpy_s(chpDest + retval, size, _RingBuffer + front, size);
	return retval + size;
}

//--------------------------------------------------
// Rear 이동
// (-)값은 이동 되지 않는다.
// Parameter: (int)이동할 Rear 값
//--------------------------------------------------
void CRingBuffer::MoveRear(int size)
{
	//int rear = _rear;
	//rear = (rear + size) & BUF_SIZE_DEFAULT;
	_rear = (_rear + size) & BUF_SIZE_DEFAULT;

	//InterlockedAdd64((LONG64*)&_nowUseSize, size);
	_nowUseSize += size;
	//_rear = rear;
}

//--------------------------------------------------
// Front 이동
// (-)값은 이동 되지 않는다.
// Parameter: (int)이동할 Rear 값
//--------------------------------------------------
void CRingBuffer::MoveFront(int size)
{
	//int front = _front;
	//front = (front + size) & BUF_SIZE_DEFAULT;
	_front = (_front + size) & BUF_SIZE_DEFAULT;

	//InterlockedAdd64((LONG64*)&_nowUseSize, -size);
	_nowUseSize -= size;
	//_front = front;
}
