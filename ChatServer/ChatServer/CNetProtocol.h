#ifndef __CNET_PROTOCOL_H__
#define __CNET_PROTOCOL_H__

//#include <windows.h>

// ��Ʈ��ũ ���̺귯�� ���
#pragma pack(push, 1)
struct st_NetHeader
{
	BYTE Code;		// ������ �ڵ�
	WORD Len;		// Payload ����
	BYTE RandKey;	// ���� Ű(��Ŷ Ű)
	BYTE CheckSum;	// Payload üũ�� ���
};
#pragma pack(pop)
#endif