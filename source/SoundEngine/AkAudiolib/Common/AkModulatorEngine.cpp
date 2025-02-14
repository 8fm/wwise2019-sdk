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

//////////////////////////////////////////////////////////////////////
//
// AkModulatorEngine.cpp
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkModulatorCtx.h"
#include "AkModulatorEngine.h"
#include <math.h>

CAkModulatorEngine::~CAkModulatorEngine()
{
	while (!m_listCtx.IsEmpty())
	{
		AkListModulatorCtx::IteratorEx ctxIt = m_listCtx.BeginEx();
		CAkModulatorCtx* pCtx = *ctxIt;
		m_listCtx.Erase( ctxIt );

		pCtx->Term();
		pCtx->Release();
	}

	m_listCtx.Term();
	TermBatch<AkEnvelopeParams,AkEnvelopeOutput>( m_envBatches );
	TermBatch<AkLFOParams,AkLFOOutput>( m_lfoBatches );
	TermBatch<AkTimeModParams,AkTimeModOutput>( m_timBatches );
}

void CAkModulatorEngine::AddContext(CAkModulatorCtx* in_pCtx)		
{
#ifndef AK_OPTIMIZED
	for( AkListModulatorCtx::Iterator ctxIt2 = m_listCtx.Begin(); ctxIt2 != m_listCtx.End(); ++ctxIt2 )
	{
		AKASSERT((*ctxIt2) != in_pCtx);
	}
#endif
	in_pCtx->AddRef();
	m_listCtx.AddLast(in_pCtx);
}

AKRESULT CAkModulatorEngine::ProcessModulators( AkUInt32 in_uFrames )
{
	AKRESULT res = AK_Success;

	AkListModulatorCtx::Iterator processListEnd;

	{
		ResetBatch<AkLFOParams,AkLFOOutput>(m_lfoBatches.First());
		ResetBatch<AkEnvelopeParams,AkEnvelopeOutput>(m_envBatches.First());
		ResetBatch<AkTimeModParams, AkTimeModOutput>(m_timBatches.First());

		AkListParamBatch::IteratorEx lfoIt = m_lfoBatches.BeginEx();
		AkListParamBatch::IteratorEx envIt = m_envBatches.BeginEx();
		AkListParamBatch::IteratorEx timIt = m_timBatches.BeginEx();

		AkListModulatorCtx::Iterator ctxIt = m_listCtx.Begin();

		while (ctxIt != m_listCtx.End())
		{
			CAkModulatorCtx* pCtx = *ctxIt;

			AKASSERT(!pCtx->IsTerminated());
			pCtx->Tick(in_uFrames);

			if (!pCtx->IsPaused())
			{
				switch (pCtx->GetType())
				{
				case AkModulatorType_LFO:
					res = AddToBatch<AkLFOParams, AkLFOOutput>(m_lfoBatches, lfoIt, pCtx);
					break;
				case AkModulatorType_Envelope:
					res = AddToBatch<AkEnvelopeParams, AkEnvelopeOutput>(m_envBatches, envIt, pCtx);
					break;
				case AkModulatorType_Time:
					res = AddToBatch<AkTimeModParams, AkTimeModOutput>(m_timBatches, timIt, pCtx);
					break;
				default:
					AKASSERT(false);
				}
			}
			
			if (res == AK_Success)
				++ctxIt;
			else
				break;
		}
		
		// save where we ended the list, in case of memory failure.
		processListEnd = ctxIt;

		//At this point, the AkListParamBatch iterators will be pointing to the last used batch.
		//Erase any batches that exist in the list after this, as they will be empty.
		CleanUpBatches<AkLFOParams,AkLFOOutput>(m_lfoBatches, lfoIt);
		CleanUpBatches<AkEnvelopeParams,AkEnvelopeOutput>(m_envBatches, envIt);
		CleanUpBatches<AkTimeModParams, AkTimeModOutput>(m_timBatches, envIt);
	}

	// Must allocate output buffers in a second pass! (output data from previous batch processing is
	// used during the AddToBatch calls).
	{
		AkListParamBatch::Iterator lfoIt = m_lfoBatches.Begin();
		AkListParamBatch::Iterator envIt = m_envBatches.Begin();
		AkListParamBatch::Iterator timIt = m_timBatches.Begin();

		AkListModulatorCtx::Iterator ctxIt = m_listCtx.Begin();

		while( ctxIt != processListEnd )
		{
			CAkModulatorCtx* pCtx = *ctxIt;

			if (!pCtx->IsPaused())
			{
				switch (pCtx->GetType())
				{
				case AkModulatorType_LFO:
				{
					PrepareBatchOutput<AkLFOParams, AkLFOOutput>(m_lfoBatches, lfoIt, pCtx);
					break;
				}
				case AkModulatorType_Envelope:
				{
					PrepareBatchOutput<AkEnvelopeParams, AkEnvelopeOutput>(m_envBatches, envIt, pCtx);
					break;
				}
				case AkModulatorType_Time:
				{
					PrepareBatchOutput<AkTimeModParams, AkTimeModOutput>(m_timBatches, timIt, pCtx);
					break;
				}
				default:
					AKASSERT(false);
				}
			}
			else
			{
				(*ctxIt)->SetOutput(NULL);
			}

			++ctxIt;
		}

		while (ctxIt != m_listCtx.End())
		{
			(*ctxIt)->SetOutput(NULL);// <- did not add modulator to a batch, due to mem failure.
			++ctxIt;
		}
	}

	ProcessBatch< CAkLFOProcess, AkLFOParams, AkLFOOutput >( m_lfoBatches.First(), in_uFrames);	
	ProcessBatch< CAkEnvelopeProcess, AkEnvelopeParams, AkEnvelopeOutput >( m_envBatches.First(), in_uFrames);
	ProcessBatch< CAkTimeModProcess, AkTimeModParams, AkTimeModOutput >( m_timBatches.First(), in_uFrames);

	return res;
}

void CAkModulatorEngine::CleanUpFinishedCtxs()
{
	// ** Two pass clean up.  Term() may end up calling back into this function
	//  (if the ctx released a node that subscribes to a modulator that is also released)
	AkListModulatorCtx toErase;
	{
		AkListModulatorCtx::IteratorEx ctxIt = m_listCtx.BeginEx();
		while (ctxIt != m_listCtx.End())
		{
			CAkModulatorCtx* pCtx = *ctxIt;
			if (pCtx->IsFinished())
			{
				ctxIt = m_listCtx.Erase(ctxIt);
				toErase.AddFirst(pCtx);
			}
			else
				++ctxIt;
		}
	}
	{
		AkListModulatorCtx::IteratorEx ctxIt = toErase.BeginEx();
		while (ctxIt != toErase.End())
		{
			CAkModulatorCtx* pCtx = *ctxIt;
			++ctxIt;
			AKASSERT(pCtx->IsFinished());
			pCtx->Term();
			pCtx->Release();
		}
	}
}


