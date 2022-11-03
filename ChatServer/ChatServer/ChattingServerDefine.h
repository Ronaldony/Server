#ifndef __CHATTING_SERVER_DEFINE_H__
#define __CHATTING_SERVER_DEFINE_H__

//------------------------------------------------------------
// 유저 섹터 정보
// 
//------------------------------------------------------------
struct st_SECTOR_POS
{
	int iX;
	int iY;
};

//------------------------------------------------------------
// 주변 섹터 정보
// 
//------------------------------------------------------------
struct st_SECTOR_AROUND
{
	int				Count;		// 주변 섹터 카운트
	st_SECTOR_POS	Around[9];	// 주변 섹터
};

//------------------------------------------------------------
// 섹터 값
// 
//------------------------------------------------------------
#define dfSECTOR_MASKING_X	0xFFFF0000
#define dfSECTOR_MASKING_Y	0xFFFF
#define dfSECTOR_DEFAULT	0xFFFF

#define dfSECTOR_MAX_X		51
#define dfSECTOR_MAX_Y		51

#endif