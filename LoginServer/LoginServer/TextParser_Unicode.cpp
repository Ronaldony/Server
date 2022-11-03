#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "TextParset_Unicode.h"

// CParserUnicode ������
CParserUnicode::CParserUnicode() : _fileData(nullptr), _filePos(0), _fileSize(0)
{
	// ���� ������ ����
}
// CParserUnicode �Ҹ���
CParserUnicode::~CParserUnicode()
{
	if (_fileData != nullptr)
		free(_fileData);
}
// -----------------------------------------------------
// ������ ������ ���� ���ۿ� ����
// ����: (const char*)
// -----------------------------------------------------
bool CParserUnicode::LoadFile(const wchar_t* file)
{
	if (NULL != _fileData)
		delete [] _fileData;

	// ���� ����
	FILE* fp;
	errno_t error = _wfopen_s(&fp, file, L"rb,ccs=UTF-16LE");
	if ((error != NULL) || (fp == NULL))
	{
		wprintf(L"fopen_s error\n");
		return false;
	}

	// ���� ũ�� ���ϱ�
	fseek(fp, 0, SEEK_END);
	_fileSize = ftell(fp) / sizeof(wchar_t);
	fseek(fp, 0, SEEK_SET);
	
	_fileData = new wchar_t[_fileSize];

	// ���� ����
	fread(_fileData, _fileSize, 2, fp);
	
	fclose(fp);

	return true;
}
// -----------------------------------------------------
// key�� �ش��ϴ� value�� ���Ѵ�.
// ����: (const char*)Key, (int)value
// -----------------------------------------------------
bool CParserUnicode::GetValue(const wchar_t* key, int* value)
{
	wchar_t strPos[100];
	int length = 0;

	_filePos = 1;	// UTF-16LE BOM Skip
	memset(&strPos, 0, sizeof(strPos));

	// Key �ܾ� ã��
	while (GetNextWord(strPos, &length))
	{
		if (0 == wcscmp(strPos, key))
		{
			memset(&strPos, 0, sizeof(strPos));
			length = 0;
			// '=' ã��
			if (GetNextWord(strPos, &length))
			{
				if (0 == wcscmp(strPos, L"="))
				{
					memset(&strPos, 0, sizeof(strPos));
					length = 0;
					// Value ã��
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
// key�� �ش��ϴ� value�� ���Ѵ�.
// ����: (const char*)Key, (int)value
// -----------------------------------------------------
bool CParserUnicode::GetString(const wchar_t* key, wchar_t* wstr)
{
	wchar_t strPos[100];
	int length = 0;

	_filePos = 1;	// UTF-16LE BOM Skip
	memset(&strPos, 0, sizeof(strPos));

	// Key �ܾ� ã��
	while (GetNextWord(strPos, &length))
	{
		if (0 == wcscmp(strPos, key))
		{
			memset(&strPos, 0, sizeof(strPos));
			length = 0;
			// '=' ã��
			if (GetNextWord(strPos, &length))
			{
				if (0 == wcscmp(strPos, L"="))
				{
					memset(&strPos, 0, sizeof(strPos));
					length = 0;
					// Value ã��
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
// ������ ������ ���� ���ۿ� ����
// ����: (const char*)
// -----------------------------------------------------
bool CParserUnicode::GetNextWord(wchar_t* pos, int* length)
{
	*length = 0;

	// ���� �ִ� ũ�� �˻�
	if (_fileSize <= _filePos)
		return false;

	// ������ ��ŵ
	if (SkipNonCommand() == false)
		return false;
	
	// ���� �� ���ڿ� ��ġ �� ���� ����
	wchar_t* filepos = &_fileData[_filePos];

	while ((_fileData[_filePos] != ASCII_SPACE) && (_fileData[_filePos] != ASCII_TAP)
		&& (_fileData[_filePos] != ASCII_LF) && (_fileData[_filePos] != ASCII_CR))
	{
		_filePos++;
		(*length)++;
		// ���� �ִ� ũ�� ����
		if (_fileSize <= _filePos)
			break;
	}

	memcpy(pos, filepos, (size_t)(*length) * sizeof(wchar_t));

	return true;
}
// -----------------------------------------------------
// ������ ������ ���� ���ۿ� ����
// ����: (const char*)
// -----------------------------------------------------
bool CParserUnicode::SkipNonCommand(void)
{
	bool ret = false;

	while (1)
	{
		// �� �� �ּ� ó��
		if ((_fileData[_filePos] == '/') && (_fileData[_filePos + 1] == '/'))
		{
			_filePos += 2;
			while (_fileData[_filePos++] != '\n');
			continue;
		}

		// ���� �� �ּ� ó��
		if ((_fileData[_filePos] == '/') && (_fileData[_filePos + 1] == '*'))
		{
			_filePos += 2;
			while ((_fileData[_filePos] != '*') || (_fileData[_filePos + 1] != '/'))
				_filePos++;
			_filePos += 2;
			continue;
		}

		// ':', �Ұ�ȣ �߰�ȣ {} ó��
		if ((_fileData[_filePos] == ':') || (_fileData[_filePos] == '{') || (_fileData[_filePos] == '}'))
		{
			_filePos++;
			while (_fileData[_filePos++] != '\n');
			continue;
		}

		// ���� ���� �ּ��� ���� ó��
	/*	if (_fileData[_filePos] == '/')
			continue;*/

		// �ٸ� ���� ó��
		wchar_t data = _fileData[_filePos];
		if ((data != ASCII_CR) && (data != ASCII_LF) && (data != ASCII_TAP) 
			&& (data != ASCII_BACKSPACE) && (data != ASCII_SPACE))
			return true;

		_filePos++;
	}

	return false;
}