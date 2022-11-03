#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "TextParset_Unicode.h"

// CParserUnicode 생성자
CParserUnicode::CParserUnicode() : _fileData(nullptr), _filePos(0), _fileSize(0)
{
	// 문자 구분자 저장
}
// CParserUnicode 소멸자
CParserUnicode::~CParserUnicode()
{
	if (_fileData != nullptr)
		free(_fileData);
}
// -----------------------------------------------------
// 지정된 파일을 열어 버퍼에 저장
// 인자: (const char*)
// -----------------------------------------------------
bool CParserUnicode::LoadFile(const wchar_t* file)
{
	if (NULL != _fileData)
		delete [] _fileData;

	// 파일 오픈
	FILE* fp;
	errno_t error = _wfopen_s(&fp, file, L"rb,ccs=UTF-16LE");
	if ((error != NULL) || (fp == NULL))
	{
		wprintf(L"fopen_s error\n");
		return false;
	}

	// 파일 크기 구하기
	fseek(fp, 0, SEEK_END);
	_fileSize = ftell(fp) / sizeof(wchar_t);
	fseek(fp, 0, SEEK_SET);
	
	_fileData = new wchar_t[_fileSize];

	// 버퍼 저장
	fread(_fileData, _fileSize, 2, fp);
	
	fclose(fp);

	return true;
}
// -----------------------------------------------------
// key에 해당하는 value를 구한다.
// 인자: (const char*)Key, (int)value
// -----------------------------------------------------
bool CParserUnicode::GetValue(const wchar_t* key, int* value)
{
	wchar_t strPos[100];
	int length = 0;

	_filePos = 1;	// UTF-16LE BOM Skip
	memset(&strPos, 0, sizeof(strPos));

	// Key 단어 찾기
	while (GetNextWord(strPos, &length))
	{
		if (0 == wcscmp(strPos, key))
		{
			memset(&strPos, 0, sizeof(strPos));
			length = 0;
			// '=' 찾기
			if (GetNextWord(strPos, &length))
			{
				if (0 == wcscmp(strPos, L"="))
				{
					memset(&strPos, 0, sizeof(strPos));
					length = 0;
					// Value 찾기
					if (GetNextWord(strPos, &length))
					{
						*value = _wtoi(strPos);
						return true;
					}
				}
			}
		}
		memset(&strPos, 0, sizeof(strPos));
		length = 0;
	}
	return false;
}

// -----------------------------------------------------
// key에 해당하는 value를 구한다.
// 인자: (const char*)Key, (int)value
// -----------------------------------------------------
bool CParserUnicode::GetString(const wchar_t* key, wchar_t* wstr)
{
	wchar_t strPos[100];
	int length = 0;

	_filePos = 1;	// UTF-16LE BOM Skip
	memset(&strPos, 0, sizeof(strPos));

	// Key 단어 찾기
	while (GetNextWord(strPos, &length))
	{
		if (0 == wcscmp(strPos, key))
		{
			memset(&strPos, 0, sizeof(strPos));
			length = 0;
			// '=' 찾기
			if (GetNextWord(strPos, &length))
			{
				if (0 == wcscmp(strPos, L"="))
				{
					memset(&strPos, 0, sizeof(strPos));
					length = 0;
					// Value 찾기
					if (GetNextWord(strPos, &length))
					{
						memcpy_s(wstr, (length - 2) * sizeof(wchar_t), &strPos[1], (length - 2) * sizeof(wchar_t));
						//*value = _wtoi(strPos);
						return true;
					}
				}
			}
		}
		memset(&strPos, 0, sizeof(strPos));
		length = 0;
	}
	return false;
}

// -----------------------------------------------------
// 지정된 파일을 열어 버퍼에 저장
// 인자: (const char*)
// -----------------------------------------------------
bool CParserUnicode::GetNextWord(wchar_t* pos, int* length)
{
	*length = 0;

	// 파일 최대 크기 검사
	if (_fileSize <= _filePos)
		return false;

	// 구분자 스킵
	if (SkipNonCommand() == false)
		return false;
	
	// 파일 내 문자열 위치 및 길이 저장
	wchar_t* filepos = &_fileData[_filePos];

	while ((_fileData[_filePos] != ASCII_SPACE) && (_fileData[_filePos] != ASCII_TAP)
		&& (_fileData[_filePos] != ASCII_LF) && (_fileData[_filePos] != ASCII_CR))
	{
		_filePos++;
		(*length)++;
		// 파일 최대 크기 도달
		if (_fileSize <= _filePos)
			break;
	}

	memcpy(pos, filepos, (size_t)(*length) * sizeof(wchar_t));

	return true;
}
// -----------------------------------------------------
// 지정된 파일을 열어 버퍼에 저장
// 인자: (const char*)
// -----------------------------------------------------
bool CParserUnicode::SkipNonCommand(void)
{
	bool ret = false;

	while (1)
	{
		// 한 줄 주석 처리
		if ((_fileData[_filePos] == '/') && (_fileData[_filePos + 1] == '/'))
		{
			_filePos += 2;
			while (_fileData[_filePos++] != '\n');
			continue;
		}

		// 여러 줄 주석 처리
		if ((_fileData[_filePos] == '/') && (_fileData[_filePos + 1] == '*'))
		{
			_filePos += 2;
			while ((_fileData[_filePos] != '*') || (_fileData[_filePos + 1] != '/'))
				_filePos++;
			_filePos += 2;
			continue;
		}

		// ':', 소괄호 중괄호 {} 처리
		if ((_fileData[_filePos] == ':') || (_fileData[_filePos] == '{') || (_fileData[_filePos] == '}'))
		{
			_filePos++;
			while (_fileData[_filePos++] != '\n');
			continue;
		}

		// 연속 적인 주석에 대한 처리
	/*	if (_fileData[_filePos] == '/')
			continue;*/

		// 다른 문자 처리
		wchar_t data = _fileData[_filePos];
		if ((data != ASCII_CR) && (data != ASCII_LF) && (data != ASCII_TAP) 
			&& (data != ASCII_BACKSPACE) && (data != ASCII_SPACE))
			return true;

		_filePos++;
	}

	return false;
}