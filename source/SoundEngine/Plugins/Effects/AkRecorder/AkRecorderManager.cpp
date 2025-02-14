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

#include "AkRecorderManager.h"
#include "AkAudioLib.h"
#include "AkFileParserBase.h"	// For AK_WAVE_FORMAT_EXTENSIBLE
#include "AkRecorderFX.h"

#define RECORDER_BLOCK_SIZE				(128 * 1024)


//
// CAkRecorderManager::StreamData
//

CAkRecorderManager::StreamData::~StreamData()
{
	CAkRecorderManager* pRecorderMgr = CAkRecorderManager::Instance();
	AK::IAkPluginMemAlloc * pAllocator = pRecorderMgr->GetAllocator();

	if( buffers.Length() != 0 )
	{
		for( unsigned int i = 0; i < buffers.Length(); ++i )
			AK_PLUGIN_FREE( pAllocator, buffers[i] );
	}
	buffers.Term();

	if( pFreeBuffer )
		AK_PLUGIN_FREE( pAllocator, pFreeBuffer );

	if( key )
		key->Destroy();
}

bool CAkRecorderManager::StreamData::AddBuffer()
{
	CAkRecorderManager* pRecorderMgr = CAkRecorderManager::Instance();
	AK::IAkPluginMemAlloc * pAllocator = pRecorderMgr->GetAllocator();

	void* pBuffer = ( pFreeBuffer ) ? pFreeBuffer : (void*)AK_PLUGIN_ALLOC( pAllocator, RECORDER_BLOCK_SIZE );
	void** ppBuffer = buffers.AddLast();

	if ( pBuffer && ppBuffer )
	{
		*ppBuffer = pBuffer;
		pFreeBuffer = NULL;
		return true;
	}
	else
	{
		if( pFreeBuffer )
		{
			AK_PLUGIN_FREE( pAllocator, pBuffer );
			pFreeBuffer = NULL;
		}
		if( ppBuffer )
		{
			buffers.RemoveLast();
		}
		return false;
	}
}

void CAkRecorderManager::StreamData::RemoveBuffer( unsigned int in_buffer )
{
	if( in_buffer < buffers.Length() )
	{
		CAkRecorderManager* pRecorderMgr = CAkRecorderManager::Instance();
		AK::IAkPluginMemAlloc * pAllocator = pRecorderMgr->GetAllocator();

		if( pFreeBuffer )
			AK_PLUGIN_FREE( pAllocator, pFreeBuffer );

		pFreeBuffer = buffers[ in_buffer ];
		buffers.Erase( in_buffer );
	}
}

//
// CAkRecorderManager
//

CAkRecorderManager * CAkRecorderManager::pInstance = NULL;

CAkRecorderManager::CAkRecorderManager(AK::IAkPluginMemAlloc * in_pAllocator, AK::IAkGlobalPluginContext * in_pCtx)
	: m_pAllocator( in_pAllocator )
	, m_pStreamMgr(in_pCtx->GetStreamMgr())
	, m_pGlobalContext(in_pCtx)
	, m_cRef( 0 )
{
	AKASSERT( pInstance == NULL );
	pInstance = this;

	// Register the callbacks seperately to only time the rendering
	in_pCtx->RegisterGlobalCallback( AkPluginTypeEffect, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_RECORDER, CAkRecorderManager::BehavioralExtension, AkGlobalCallbackLocation_BeginRender | AkGlobalCallbackLocation_Term );
}

CAkRecorderManager::~CAkRecorderManager() 
{
	m_pGlobalContext->UnregisterGlobalCallback( CAkRecorderManager::BehavioralExtension, AkGlobalCallbackLocation_BeginRender | AkGlobalCallbackLocation_Term );

	m_streams.Term();

	pInstance = NULL;
}

bool CAkRecorderManager::AddStream(AK::IAkStdStream * in_pStream, AkChannelConfig in_channelConfig, AkUInt32 in_uSampleRate, AkInt16 in_iFormat)
{
	StreamData * pStreamData;
	pStreamData = (StreamData*)AK_PLUGIN_NEW( m_pAllocator, StreamData() );
	if (!pStreamData)
		return false;

	m_streams.AddFirst(pStreamData);

	pStreamData->key = in_pStream;
	pStreamData->eState = StreamState_WaitForBuffer;
	pStreamData->iFormat = in_iFormat;
	pStreamData->bReleased = false;

	size_t uHeaderSize = AkFileParser::FillHeader(in_uSampleRate, AkFileParser::Int16, (AkFileParser::FormatType)in_iFormat, in_channelConfig, pStreamData->hdr);
	if (Record(in_pStream, &pStreamData->hdr, (AkUInt32) uHeaderSize))
	{
		// Reset data size (incremented in Record(), but header should not count).
		switch (pStreamData->iFormat)
		{
		case AkFileParser::WAV:
			pStreamData->hdr.wav.data.dwChunkSize = 0;
			break;
			//case AkFileParser::WEM:
		default:
			pStreamData->hdr.wem.data.dwChunkSize = 0;
			break;
		}
		return true;
	}
	return false;
}

bool CAkRecorderManager::Record(AK::IAkStdStream * in_pStream, void * in_pBuffer, AkUInt32 in_cBytes)
{
	StreamData * pStreamData = FindStreamData( in_pStream );
	if ( !pStreamData )
		return false;

	char * pInBuffer = (char *) in_pBuffer;

	while ( in_cBytes )
	{
		void * pBuffer;

		if ( pStreamData->uBufferPos == 0 )
		{
			if( ! pStreamData->AddBuffer() )
				return false;
		}
		pBuffer = pStreamData->buffers.Last();

		AkUInt32 uDataSize = AkMin( RECORDER_BLOCK_SIZE - pStreamData->uBufferPos, in_cBytes );

		memcpy( (char *) pBuffer + pStreamData->uBufferPos, pInBuffer, uDataSize );

		switch (pStreamData->iFormat)
		{
		case AkFileParser::WAV:
			pStreamData->hdr.wav.data.dwChunkSize += uDataSize;
			pStreamData->hdr.wav.RIFF.dwChunkSize = sizeof(AkWAVEFileHeaderStd) + pStreamData->hdr.wav.data.dwChunkSize - 8;
			break;
		//case AkFileParser::WEM:
		default:
			pStreamData->hdr.wem.data.dwChunkSize += uDataSize;
			pStreamData->hdr.wem.RIFF.dwChunkSize = sizeof(AkWAVEFileHeaderWem) + pStreamData->hdr.wem.data.dwChunkSize - 8;
			break;
		}

		pStreamData->uBufferPos += uDataSize;

		if ( pStreamData->uBufferPos == RECORDER_BLOCK_SIZE )
			pStreamData->uBufferPos = 0;

		in_cBytes -= uDataSize;
		pInBuffer += uDataSize;
	}


	return true;
}

void CAkRecorderManager::ReleaseStream( AK::IAkStdStream * in_pStream )
{
	StreamData * pStreamData = FindStreamData( in_pStream );
	if ( pStreamData )
		pStreamData->bReleased = true;
}

void CAkRecorderManager::BehavioralExtension(AK::IAkGlobalPluginContext *, AkGlobalCallbackLocation in_eLocation, void *)
{
	AKASSERT(pInstance);
	pInstance->Execute(in_eLocation);
}

CAkRecorderManager * CAkRecorderManager::Instance(AK::IAkPluginMemAlloc * in_pAllocator /*=NULL*/, AK::IAkGlobalPluginContext * in_pCtx /*= NULL*/)
{
	if ( !pInstance )
	{
		AKASSERT(in_pAllocator && in_pCtx);
		if (in_pAllocator && in_pCtx)
		{
			CAkRecorderManager * pRecorderManager;
			pRecorderManager = (CAkRecorderManager *)AK_PLUGIN_NEW(in_pAllocator, CAkRecorderManager(in_pAllocator, in_pCtx));
		}
	}

	return pInstance;
}

void CAkRecorderManager::Execute(AkGlobalCallbackLocation in_eLocation)
{
	for ( Streams::IteratorEx it = m_streams.BeginEx(); it != m_streams.End(); )
	{
		StreamData * pStreamData = *it;
		AKRESULT eResult = AK_Success;

		if (  pStreamData->eState == StreamState_BufferSent )
		{
			AkStmStatus status = pStreamData->key->GetStatus();
			if ( status != AK_StmStatusPending )
			{
				if ( status == AK_StmStatusCompleted )
				{
					pStreamData->RemoveBuffer( 0 );

					if ( pStreamData->buffers.IsEmpty() && pStreamData->bReleased ) 
					{
						AkInt64 iRealOffset;
						pStreamData->key->SetPosition( 0, AK_MoveBegin, &iRealOffset );
						AkUInt32 uOutSize;

						switch (pStreamData->iFormat)
						{
						case AkFileParser::WAV:
							{
								AkReal32 fDeadline = (AkReal32)RECORDER_BLOCK_SIZE / pStreamData->hdr.wav.waveFmtExt.nAvgBytesPerSec * 1000.0f;
								eResult = pStreamData->key->Write(&pStreamData->hdr, sizeof(AkWAVEFileHeaderStd), false, AK_DEFAULT_PRIORITY, fDeadline, uOutSize);
							}
							break;
						//case AkFileParser::WEM:
						default:
							{
								AkReal32 fDeadline = (AkReal32)RECORDER_BLOCK_SIZE / pStreamData->hdr.wem.waveFmtExt.nAvgBytesPerSec * 1000.0f;
								eResult = pStreamData->key->Write(&pStreamData->hdr, sizeof(AkWAVEFileHeaderWem), false, AK_DEFAULT_PRIORITY, fDeadline, uOutSize);
							}
							break;
						}
						pStreamData->eState = StreamState_HeaderSent;
					}
					else
						pStreamData->eState = StreamState_WaitForBuffer;
				}
				else
				{
					eResult = AK_Fail;
				}
			}
		}

		if ( pStreamData->eState == StreamState_WaitForBuffer )
		{
			AkUInt32 cBuffers = pStreamData->buffers.Length();
			if ( cBuffers > 1 || ( cBuffers == 1 && pStreamData->bReleased ) )
			{
				AkUInt32 uOutSize = RECORDER_BLOCK_SIZE;
				if ( cBuffers == 1 && pStreamData->bReleased && pStreamData->uBufferPos != 0 )
					uOutSize = pStreamData->uBufferPos;

				AkReal32 fDeadline;
				switch (pStreamData->iFormat)
				{
				case AkFileParser::WAV:
					fDeadline = (AkReal32)RECORDER_BLOCK_SIZE / pStreamData->hdr.wav.waveFmtExt.nAvgBytesPerSec * 1000.0f;
					break;
				//case AkFileParser::WEM:
				default:
					fDeadline = (AkReal32)RECORDER_BLOCK_SIZE / pStreamData->hdr.wem.waveFmtExt.nAvgBytesPerSec * 1000.0f;
					break;
				}
				eResult = pStreamData->key->Write( pStreamData->buffers[0], uOutSize, false, AK_DEFAULT_PRIORITY, fDeadline, uOutSize);
				pStreamData->eState = StreamState_BufferSent;
			}
		}
		else if ( pStreamData->eState == StreamState_HeaderSent )
		{
			AkStmStatus status = pStreamData->key->GetStatus();
			if ( status != AK_StmStatusPending )
				eResult = AK_NoMoreData;
		}

		if ( eResult == AK_Success && in_eLocation != AkGlobalCallbackLocation_Term )
			++it;
		else
		{
			it = m_streams.Erase( it );
			AK_PLUGIN_DELETE( m_pAllocator, pStreamData );
		}
	}

	if ( m_streams.IsEmpty() && m_cRef == 0 )
	{
		AK_PLUGIN_DELETE( m_pAllocator, this );
	}
}
