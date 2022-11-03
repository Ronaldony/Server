#ifndef __TEXT_PARSER_UNICODE_H__
#define __TEXT_PARSER_UNICODE_H__

#define ASCII_SPACE		0x20
#define ASCII_BACKSPACE	0x08
#define ASCII_TAP		0x09
#define ASCII_CR		0x0D
#define ASCII_LF		0x0A

#define ASCII_SPACE		0x20
#define ASCII_BACKSPACE	0x08
#define ASCII_TAP		0x09
#define ASCII_CR		0x0D
#define ASCII_LF		0x0A

class CParserUnicode
{
public:
	CParserUnicode();
	~CParserUnicode();
	bool LoadFile(const wchar_t*);
	bool GetValue(const wchar_t*, int*);					// int->T·Î º¯È¯
	bool GetString(const wchar_t*, wchar_t* wstr);

private:
	wchar_t*_fileData;
	int _filePos;
	int _fileSize;

	bool GetNextWord(wchar_t*, int*);
	bool SkipNonCommand(void);
};


#endif