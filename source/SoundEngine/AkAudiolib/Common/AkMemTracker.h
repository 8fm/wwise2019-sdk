/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version:  Build: 
Copyright (c) 2006-2019 Audiokinetic Inc.
***********************************************************************/

#include <map>
#include <AK/Tools/Common/AkLock.h>

#ifdef AK_WIN
#include "dbghelp.h"
//#define AK_MEMDEBUG_BT
#define AK_MEMDEBUG_BT_SIZE 8
#define TRACE_MAX_FUNCTION_NAME_LENGTH 256
#endif

class AkMemTracker
{
	struct MemInfo
	{
#ifdef AK_MEMDEBUG_BT
		DWORD64 backtrace[AK_MEMDEBUG_BT_SIZE];
#else
		const char * szFile;
		int pos;
#endif
		AkMemPoolId poolId;
		size_t ulSize;

	};

	typedef std::map<void *, MemInfo> MemToInfo;

public:
	AkMemTracker()
	{
	}

	~AkMemTracker()
	{
	}

	void Add(AkMemPoolId poolId, void * ptr, size_t in_size, const char * szFile, int pos)
	{
		m_csLock.Lock();

		MemInfo & info = m_memToInfo[ptr];

		info.ulSize = in_size;
		info.poolId = poolId;
#ifdef AK_MEMDEBUG_BT
		memset(&info.backtrace, 0, AK_MEMDEBUG_BT_SIZE*sizeof(ULONG));
		CaptureStackBackTrace(2, AK_MEMDEBUG_BT_SIZE, (PVOID *)&info.backtrace[0], NULL);
#else
		info.szFile = szFile;
		info.pos = pos;
#endif
		m_csLock.Unlock();
	}

	void Remove(AkMemPoolId poolId, void * ptr)
	{
		m_csLock.Lock();
		MemToInfo::iterator info = m_memToInfo.find(ptr);
		if (info != m_memToInfo.end())
		{
			AKASSERT(info->second.poolId == poolId);
			m_memToInfo.erase(info);
		}
		m_csLock.Unlock();
	}

	void Print(const char * szMsg)
	{
		AKPLATFORM::OutputDebugMsg(szMsg);
#if defined(AK_WIN)
		// On Windows, OutputDebugMsg doesn't go to a standard output/error stream, making it invisible in Jenkins log. Remedy this here
		fprintf(stderr, szMsg);
#endif
	}

	void PrintMemoryLabel(MemToInfo::iterator info)
	{
		char szMsg[1024];

#ifdef AK_MEMDEBUG_BT
		sprintf(szMsg, "** Memory leak (%6i bytes) detected in Wwise sound engine. \n", info->second.ulSize);
		Print(szMsg);
		HANDLE process = GetCurrentProcess();
		SymInitialize(process, NULL, TRUE);
		SYMBOL_INFO *symbol = (SYMBOL_INFO *)alloca(sizeof(SYMBOL_INFO) + (TRACE_MAX_FUNCTION_NAME_LENGTH - 1) * sizeof(TCHAR));
		symbol->MaxNameLen = TRACE_MAX_FUNCTION_NAME_LENGTH;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		DWORD displacement;
		IMAGEHLP_LINE64 *line = (IMAGEHLP_LINE64 *)alloca(sizeof(IMAGEHLP_LINE64));
		line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		for (int i = 0; i < AK_MEMDEBUG_BT_SIZE; i++)
		{
			DWORD64 address = (DWORD64)(info->second.backtrace[i]);
			SymFromAddr(process, address, NULL, symbol);
			if (SymGetLineFromAddr64(process, address, &displacement, line))
			{
				sprintf(szMsg, "\t0x%0X: %s -- %s: line: %lu\n", symbol->Address, symbol->Name, line->FileName, line->LineNumber);
				Print(szMsg);
			}
			else
			{
				sprintf(szMsg, "\tSymGetLineFromAddr64 returned error code %lu.\n", GetLastError());
				Print(szMsg);
				sprintf(szMsg, "\t0x%0X: %s\n", symbol->Address, symbol->Name);
				Print(szMsg);
			}

		}
#else
		if (info->second.szFile == NULL)
		{
			sprintf(szMsg, "** Memory leak in Wwise pool %d: %p\t%6zu\t\tUnknown file\n", info->second.poolId, (void *)info->first, info->second.ulSize);
		}
		else
		{
			sprintf(szMsg, "** Memory leak in Wwise pool %d: %p\t%6zu\t%5i\t%s\n", info->second.poolId, (void *)info->first, info->second.ulSize, info->second.pos, info->second.szFile);
		}
		Print(szMsg);
#endif
	}

	void PrintMemoryLabel(void* ptr)
	{
		m_csLock.Lock();
		MemToInfo::iterator info = m_memToInfo.find(ptr);
		if (info != m_memToInfo.end())
			PrintMemoryLabel(info);
		m_csLock.Unlock();
	}

	void PrintMemoryLabels()
	{
		m_csLock.Lock();
		for (MemToInfo::iterator info = m_memToInfo.begin(); info != m_memToInfo.end(); ++info)
			PrintMemoryLabel(info);
		m_csLock.Unlock();
	}

	void DumpToFile(const char* strFileName /* = "AkMemDump.txt" */)
	{
#ifndef AK_MEMDEBUG_BT
		m_csLock.Lock();

		FILE* file = fopen(strFileName, "w");
		if (file != NULL)
		{
			fprintf(file, "Address\tSize\tLine\tFile\n");
			MemToInfo::iterator it = m_memToInfo.begin();
			while (it != m_memToInfo.end())
			{
				if (it->second.szFile == NULL)
					fprintf(file, "%p\t%6zu\t\tUnknown file\n", (void *)it->first, it->second.ulSize);
				else
					fprintf(file, "%p\t%6zu\t%5i\t%s\n", (void *)it->first, it->second.ulSize, it->second.pos, it->second.szFile);
				it++;
			}
			fclose(file);
		}
		else
		{
			char szMsg[1024];
			sprintf(szMsg, "*** Unable to dump memory stats.  Can't open file: %s\n", strFileName);
			Print(szMsg);
		}

		m_csLock.Unlock();
#endif
	}

	MemToInfo m_memToInfo;
	CAkLock m_csLock;
};
