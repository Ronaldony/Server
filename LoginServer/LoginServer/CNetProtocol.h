#ifndef __CNET_PROTOCOL_H__
#define __CNET_PROTOCOL_H__

//#include <windows.h>

// 네트워크 라이브러리 헤더
#pragma pack(push, 1)
struct st_NetHeader
{
	BYTE Code;		// 쓰레기 코드
	WORD Len;		// Payload 길이
	BYTE RandKey;	// 랜덤 키(패킷 키)
	BYTE CheckSum;	// Payload 체크섬 결과
};
#pragma pack(pop)
#endif