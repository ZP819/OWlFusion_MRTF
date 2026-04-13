#pragma once
class OWlEncryptFile
{
public:
	OWlEncryptFile();
	~OWlEncryptFile();

public:
	void EnyptFile(const char* defilename);
	void DeyptFile(const char* enfilename, bool isFileShow = false);

	void DelFile(const char* filename);
};

