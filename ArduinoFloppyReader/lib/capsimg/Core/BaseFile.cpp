#include "stdafx.h"
#include "debugmsg.h"



CBaseFile::CBaseFile()
{
	Clear();
}

CBaseFile::~CBaseFile()
{
}

// clear file settings
void CBaseFile::Clear()
{
	fileopen=0;
	filemode=0;
}
