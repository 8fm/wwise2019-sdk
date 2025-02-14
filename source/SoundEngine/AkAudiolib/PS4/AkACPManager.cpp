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

#include "stdafx.h"
#include "AkACPManager.h"
#include "AkSrcBankAt9.h"
#include "AkSrcFileAt9.h"
#include "ajm/at9_decoder.h"
#include "AkAudioLibTimer.h"
#include "AkAudioMgr.h"

extern AkPlatformInitSettings g_PDSettings;
CAkACPManager CAkACPManager::s_ACPManager;

CAkACPManager::CAkACPManager()
	:m_AjmContextId(0)
	,m_pAcpBatchBuffer(nullptr)
	,m_batchId(SCE_AJM_BATCH_INVALID)
    ,m_bNeedAnotherPass(false)
{	
}

CAkACPManager::~CAkACPManager()
{	
}

AKRESULT CAkACPManager::Init()
{
	m_batchId = SCE_AJM_BATCH_INVALID;

	// Intialise the audio job manager
	AKVERIFY( sceAjmInitialize(0, &m_AjmContextId) == SCE_OK );

	// Load the atrac9 decoder
	AKVERIFY( sceAjmModuleRegister(m_AjmContextId, SCE_AJM_CODEC_AT9_DEC, 0) == SCE_OK );

	if (g_PDSettings.uLEngineAcpBatchBufferSize > 0)
	{
		m_pAcpBatchBuffer = (uint8_t*)AkAlloc( AkMemID_Processing, g_PDSettings.uLEngineAcpBatchBufferSize ); 
		if( m_pAcpBatchBuffer == NULL )
		{	
			AKASSERT(!"Failed allocating m_pAcpBatchBuffer in CAkACPManager");
			return AK_Fail;
		}
	}

#ifndef AK_OPTIMIZED
	memset(&m_stats,0,sizeof(m_stats));
#endif

	return AK_Success;
}

void CAkACPManager::Term()
{
	if (m_pAcpBatchBuffer != NULL)
	{
		AkFree( AkMemID_Processing, m_pAcpBatchBuffer );
	}

	AKVERIFY( sceAjmModuleUnregister(m_AjmContextId, SCE_AJM_CODEC_AT9_DEC) == SCE_OK );
	AKVERIFY( sceAjmFinalize(m_AjmContextId) == SCE_OK );
}

AKRESULT CAkACPManager::Register(CAkSrcBankAt9* pACPSrc)
{
	if ( pACPSrc->CreateHardwareInstance(m_AjmContextId) == AK_Success)
	{
		m_ACPBankSrc.AddFirst( pACPSrc );
		return AK_Success;
	}
	
	return AK_Fail;
}

AKRESULT CAkACPManager::Register(CAkSrcFileAt9* pACPSrc)
{
	if ( pACPSrc->CreateHardwareInstance(m_AjmContextId) == AK_Success)
	{
		m_ACPFileSrc.AddFirst( pACPSrc );
		return AK_Success;
	}
		
	return AK_Fail;
}

void CAkACPManager::Unregister(CAkSrcBankAt9* pACPSrc)
{
	m_ACPBankSrc.Remove(pACPSrc);
}

void CAkACPManager::Unregister(CAkSrcFileAt9* pACPSrc)
{
	m_ACPFileSrc.Remove(pACPSrc);
}

void CAkACPManager::SubmitDecodeBatch()
{
	AK_INSTRUMENT_SCOPE( "CAkACPManager::SubmitDecodeBatch" );
	
	SceAjmBatchError ajmBatchError;

	m_bNeedAnotherPass = false;

	if (!m_ACPBankSrc.IsEmpty() || !m_ACPFileSrc.IsEmpty())
	{
		Wait();

		bool decodingJobFound = false;

		sceAjmBatchInitialize(m_pAcpBatchBuffer, g_PDSettings.uLEngineAcpBatchBufferSize, &m_batchInfo );

		for ( auto it = m_ACPBankSrc.Begin(); it != m_ACPBankSrc.End(); ++it )
		{
			CAkSrcBankAt9* pACPSrc = (*it);

			if (pACPSrc->DataNeeded())
			{
				pACPSrc->InitIfNeeded(&m_batchInfo);

				if ( pACPSrc->CreateDecodingJob(&m_batchInfo ) == AK_Success )
				{
					decodingJobFound = true;

                    if (pACPSrc->DataNeeded())
                    {
                        m_bNeedAnotherPass = true;
                    }
				}
			}
		}

		for ( auto it = m_ACPFileSrc.Begin(); it != m_ACPFileSrc.End(); ++it )
		{
			CAkSrcFileAt9* pACPSrc = (*it);

			if (pACPSrc->DataNeeded())
			{
				pACPSrc->UpdateStreamBuffer(); // fill stream buffer if needed
				if (pACPSrc->IsDataReady())
				{
					pACPSrc->InitIfNeeded(&m_batchInfo);

					if ( pACPSrc->CreateDecodingJob(&m_batchInfo ) == AK_Success )
					{
						decodingJobFound = true;

                        if (pACPSrc->DataNeeded())
                        {
                            m_bNeedAnotherPass = true;
                        }
					}
				}
			}
		}

		if (decodingJobFound)
		{
#ifndef AK_OPTIMIZED
			AKVERIFY( sceAjmBatchJobGetStatistics(&m_batchInfo, ((float)AK_NUM_VOICE_REFILL_FRAMES/(float)AK_CORE_SAMPLERATE), &m_stats) == SCE_OK);
#endif
			// Run all jobs
			SceAjmBatchError ajmBatchError;
			int result = sceAjmBatchStart(m_AjmContextId, &m_batchInfo, SCE_AJM_PRIORITY_GAME_DEFAULT, &ajmBatchError, const_cast<SceAjmBatchId *>(&m_batchId));
			if (result != SCE_OK)
			{
				for ( auto it = m_ACPBankSrc.Begin(); it != m_ACPBankSrc.End(); ++it )
				{
					CAkSrcBankAt9* pACPSrc = (*it);
					if (pACPSrc->DecodingStarted())
						pACPSrc->DecodingFailed();
				}

				for ( auto it = m_ACPFileSrc.Begin(); it != m_ACPFileSrc.End(); ++it )
				{
					CAkSrcFileAt9* pACPSrc = (*it);
					if (pACPSrc->DecodingStarted())
						pACPSrc->DecodingFailed();
				}
			}
		}
	}
}


bool CAkACPManager::Wait(unsigned int in_uTimeoutMs/* = SCE_AJM_WAIT_INFINITE*/) 
{
	AK_INSTRUMENT_SCOPE( "CAkACPManager::Wait" );
	
 	bool bFinished = true;
	if (IsBusy()) // Perform initial test outside of critical section, to optimize common case.
	{
		AkAutoLock<CAkLock> lock(m_lockWait);
		if (!IsBusy()) // Necessary atomic version of the above test
			return true;

		SceAjmBatchError ajmBatchError;

		int result = sceAjmBatchWait(m_AjmContextId, m_batchId, in_uTimeoutMs, &ajmBatchError);
        bFinished = (result == SCE_OK || result == SCE_AJM_ERROR_MALFORMED_BATCH);

		if (result == SCE_AJM_ERROR_IN_PROGRESS)
		{
			AKASSERT( in_uTimeoutMs != SCE_AJM_WAIT_INFINITE );
			return false;
		}
		AKASSERT( result != SCE_AJM_ERROR_BUSY );
				
        if (bFinished)
        {
			// Update 
			for ( auto it = m_ACPBankSrc.Begin(); it != m_ACPBankSrc.End(); ++it )
			{
				CAkSrcBankAt9* pACPSrc = (*it);
				if (pACPSrc->DecodingStarted())
					pACPSrc->DecodingEnded();
			}
	
			for ( auto it = m_ACPFileSrc.Begin(); it != m_ACPFileSrc.End(); ++it )
			{
				CAkSrcFileAt9* pACPSrc = (*it);
				if (pACPSrc->DecodingStarted())
					pACPSrc->DecodingEnded();
			}
	
			m_batchId = SCE_AJM_BATCH_INVALID;
        }
		
		else
		{
			for ( auto it = m_ACPBankSrc.Begin(); it != m_ACPBankSrc.End(); ++it )
			{
				CAkSrcBankAt9* pACPSrc = (*it);
				if (pACPSrc->DecodingStarted())
					pACPSrc->DecodingFailed();
			}

			for ( auto it = m_ACPFileSrc.Begin(); it != m_ACPFileSrc.End(); ++it )
			{
				CAkSrcFileAt9* pACPSrc = (*it);
				if (pACPSrc->DecodingStarted())
					pACPSrc->DecodingFailed();
			}
		}
	}

    return bFinished;
}

