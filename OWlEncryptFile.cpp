#include "stdafx.h"
#include "OWlEncryptFile.h"


OWlEncryptFile::OWlEncryptFile()
{
}


OWlEncryptFile::~OWlEncryptFile()
{
}

void OWlEncryptFile::EnyptFile(const char* defilename)
{
	bool isfileValid = SPxIsFile(defilename);
	if (!isfileValid)
	{
		return;
	}
	
	string enfilename = string(defilename);
	int pos = enfilename.rfind(".");
	enfilename = enfilename.insert(pos, "e");
	
	UINT32 readedBytes = 0;
	std::ifstream fin(defilename, ios::in | std::ios::binary);
	unsigned char s;

	if (!fin) {
		return;
	}
	while (fin.read((char *)&s, sizeof(s))) { //一直读到文件结束
		readedBytes++; //看刚才读了多少字节
	}
	fin.close();


	std::ifstream fin1(defilename, ios::in | std::ios::binary);
	char *szBuf = new  char[readedBytes + 1];
	fin1.read(szBuf, readedBytes);
	fin1.close();
	//求A
	UINT8 A = readedBytes % 256;

	//求B
	unsigned char dataNumber = 0;
	for (int i = 0; i < readedBytes; i++)
	{
		dataNumber += (unsigned char)szBuf[i];
	}
	UINT8 B = dataNumber % 256;

	//异或
	for (int i = 0; i <readedBytes; i++)
	{
		unsigned char a = (unsigned char)(314 * A ^ 2 + 159 * B ^ 3 + 265 * i ^ 3 + 358 * i ^ 2 + 979 * i + 19750801) % 256;
		szBuf[i] = (unsigned char)(a^szBuf[i]);
	}
	szBuf[readedBytes] = B;

	ofstream fileEn;
	fileEn.open(enfilename, ios::out | std::ios::binary);
	fileEn.write((const char*)szBuf, sizeof(char) * (readedBytes + 1));
	fileEn.close();

	delete[] szBuf;//xr原来没有delete

	//加密完成后，将原来解密的文件删除，即加密文件和解密文件不能同时存在
	CFile TempFile;
	TempFile.Remove(CString(defilename));


	int zp = 10;

}

void OWlEncryptFile::DelFile(const char* filename)
{
	bool isfileValid = SPxIsFile(filename);
	if (!isfileValid)
	{
		return;
	}

	CFile TempFile;
	TempFile.Remove(CString(filename));
}

void OWlEncryptFile::DeyptFile(const char* enfilename, bool isFileShow)
{
	bool isfileValid = SPxIsFile(enfilename);
	if (!isfileValid)
	{
		return;
	}

	string defilename = string(enfilename);
	int pos = defilename.rfind(".");
	defilename = defilename.erase(pos - 1, 1);

	

	UINT32 readedBytes = 0;
	std::ifstream fin(enfilename, ios::in | std::ios::binary);
	unsigned char s;

	if (!fin) {
		return;
	}
	while (fin.read((char *)&s, sizeof(s))) { //一直读到文件结束
		readedBytes++; //看刚才读了多少字节
	}
	fin.close();


	std::ifstream fin1(enfilename, ios::in | std::ios::binary);
	char *szBuf = new  char[readedBytes];
	fin1.read(szBuf, readedBytes);
	fin1.close();
	//求A
	UINT8 A = (readedBytes - 1) % 256;
	//求B,最后一个数是B的值
	UINT8 B = szBuf[readedBytes - 1];

	//异或
	for (int i = 0; i < readedBytes - 1; i++)
	{
		unsigned char a = (unsigned char)(314 * A ^ 2 + 159 * B ^ 3 + 265 * i ^ 3 + 358 * i ^ 2 + 979 * i + 19750801) % 256;
		szBuf[i] = (unsigned char)(a^szBuf[i]);
	}

	ofstream fileDe;
	fileDe.open(defilename, ios::out | std::ios::binary);
	fileDe.write((const char*)szBuf, readedBytes - 1);
	fileDe.close();

	if (!isFileShow)
	{
		bool ret = SetFileAttributes(CString(defilename.c_str()), FILE_ATTRIBUTE_HIDDEN);
		int zp = 10;
	}

	delete[] szBuf;//xr原来没有delete

	int zp = 10;
}