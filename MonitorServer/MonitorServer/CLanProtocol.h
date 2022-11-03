#ifndef __CLAN_PROTOCOL_H__
#define __CLAN_PROTOCOL_H__

// 네트워크 라이브러리 헤더
#pragma pack(push, 1)
struct st_LanHeader
{
	WORD Len;		// Payload 길이
};
#pragma pack(pop)
#endif