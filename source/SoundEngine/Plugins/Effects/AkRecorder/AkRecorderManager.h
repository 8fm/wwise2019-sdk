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

#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkListBareLight.h>
#include "AkFileParserBase.h"

class CAkRecorderManager
{
public:
	CAkRecorderManager(AK::IAkPluginMemAlloc * in_pAllocator, AK::IAkGlobalPluginContext * in_pCtx);
	~CAkRecorderManager();

	AkForceInline void AddRef() { ++m_cRef; }
	AkForceInline void Release() { --m_cRef; }

	AK::IAkPluginMemAlloc * GetAllocator() { return m_pAllocator; }

	bool AddStream(AK::IAkStdStream * in_pStream, AkChannelConfig in_channelConfig, AkUInt32 in_uSampleRate, AkInt16 in_iFormat);
	bool Record(AK::IAkStdStream * in_pStream, void * in_pBuffer, AkUInt32 in_cBytes);
	void ReleaseStream(AK::IAkStdStream * in_pStream);

	static void BehavioralExtension(AK::IAkGlobalPluginContext *, AkGlobalCallbackLocation in_eLocation, void *);
	
	static CAkRecorderManager * Instance(AK::IAkPluginMemAlloc * in_pAllocator = NULL, AK::IAkGlobalPluginContext * in_pCtx = NULL);

private:

	enum StreamState
	{
		StreamState_WaitForBuffer,
		StreamState_BufferSent,
		StreamState_HeaderSent
	};

	struct AkArrayAllocRecorder
	{
		static AkForceInline void * Alloc( size_t in_uSize )
		{
			return CAkRecorderManager::Instance()->GetAllocator()->Malloc( in_uSize );
		}

		static AkForceInline void * ReAlloc(void * in_pAddress, size_t in_uOldSize, size_t in_uNewSize)
		{
			void* pNew = Alloc(in_uNewSize);
			if (pNew && in_pAddress)
			{
				memcpy(pNew, in_pAddress, in_uOldSize);
				Free(in_pAddress);
			}
			return pNew;
		}

		static AkForceInline void Free( void * in_pAddress )
		{
			CAkRecorderManager::Instance()->GetAllocator()->Free( in_pAddress );
		}
	};
	typedef AkArray< void*, void*, AkArrayAllocRecorder > BufferArray;

	struct StreamData
	{
		StreamData * pNextLightItem;

		StreamData() : key(NULL), uBufferPos(0), pFreeBuffer(NULL) {}
		~StreamData();

		bool AddBuffer();
		void RemoveBuffer( unsigned int in_buffer );

		AK::IAkStdStream * key;

		BufferArray buffers;
		AkUInt32 uBufferPos; // Current writing position in last buffer of array.
		void* pFreeBuffer;

		AkFileParser::Header hdr;
		StreamState eState;
		bool bReleased;
		AkInt16 iFormat;
	};

	typedef AkListBareLight<StreamData> Streams;

	AkForceInline StreamData * FindStreamData(AK::IAkStdStream * in_key)
	{
		for (Streams::Iterator it = m_streams.Begin(); it != m_streams.End(); ++it)
		{
			if ((*it)->key == in_key)
				return *it;
		}

		return NULL;
	}

	void Execute(AkGlobalCallbackLocation in_eLocation);

	AK::IAkPluginMemAlloc * m_pAllocator;
	AK::IAkStreamMgr * m_pStreamMgr;
	AK::IAkGlobalPluginContext *m_pGlobalContext;
	AkUInt32 m_cRef;

	Streams m_streams;

	static CAkRecorderManager * pInstance;
};
