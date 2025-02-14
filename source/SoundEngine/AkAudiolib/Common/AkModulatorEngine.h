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
// AkModulatorEngine.h
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#ifndef _MODULATOR_ENG_H_
#define _MODULATOR_ENG_H_

#include "AkModulatorParams.h"
#include "AkModulatorCtx.h"
#include "AudiolibDefs.h"
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/DSP/AkModulatorProcess.h>

//Size of contiguous arrays of parameters and output buffers 
#define MODULATOR_BATCH_SIZE 16

class CAkMidiNoteState;

////
// CAkModulatorBatch 
//
// Represents a set of parameters, and output values for each modulator context.  They are grouped together in 
//		batches of contiguous arrays to reduce dynamic memory allocation, and to provide greater cache coherency.
////
class CAkModulatorBatchBase
{
public:
	CAkModulatorBatchBase(): pNextItem(NULL) {}
	CAkModulatorBatchBase* pNextItem;
};

template<typename tParams, typename tOutput>
class CAkModulatorBatch: public CAkModulatorBatchBase
{
public:
	CAkModulatorBatch(): m_uBufferSizeNeeded(0), m_uBufferSizeAllocated(0), m_pBuffer(NULL) {}

	~CAkModulatorBatch()
	{
		FreeBuffers();
		m_params.Term();
		m_outputs.Term();
	}

	AKRESULT Init(AkUInt32 in_nReseve) 
	{
		if ( m_params.Reserve(in_nReseve) == AK_Success && m_outputs.Reserve(in_nReseve) == AK_Success )
			return AK_Success;
		else 
			return AK_InsufficientMemory;
	}

	AKRESULT AllocBuffers()
	{
		AKRESULT res = AK_Success;
		if (m_uBufferSizeNeeded > m_uBufferSizeAllocated)
		{
			FreeBuffers();
			m_pBuffer = (AkReal32*) AkMalign(AkMemID_Object, m_uBufferSizeNeeded * sizeof(AkReal32), 16 );
			if (m_pBuffer != NULL)
				m_uBufferSizeAllocated = m_uBufferSizeNeeded;
			else
				res = AK_InsufficientMemory;
		}
		return res;
	}

	void FreeBuffers()
	{
		if (m_pBuffer)
			AkFalign(AkMemID_Object, m_pBuffer);
		m_uBufferSizeAllocated = 0;
		m_pBuffer = NULL;
	}

	bool Full() const				{return m_params.Length() == m_params.Reserved();}
	bool LessThanHalfFull() const	{return m_params.Length() < m_params.Reserved()/2;}
	bool IsEmpty() const			{return m_params.Length() == 0;}

	typedef AkArray<tParams, const tParams&, AkArrayAllocatorAlignedSimd<AkMemID_Object> > ParamsArray;
	ParamsArray m_params;

	typedef AkArray<tOutput, const tOutput&, AkArrayAllocatorAlignedSimd<AkMemID_Object> > OutputsArray;
	OutputsArray m_outputs;

	AkUInt32 m_uBufferSizeNeeded;
	AkUInt32 m_uBufferSizeAllocated;
	AkReal32* m_pBuffer;
};

//Fill wrappers allow the correct fill function to be compile-time determined using the template classes
AkForceInline void AkEnvelopeParams::Fill(CAkModulatorCtx* in_pCtx) 
{ 
	static_cast<CAkEnvelopeCtx*>(in_pCtx)->FillParams(this); 
}

AkForceInline void AkLFOParams::Fill(CAkModulatorCtx* in_pCtx) 
{ 
	static_cast<CAkLFOCtx*>(in_pCtx)->FillParams(this); 
}

AkForceInline void AkTimeModParams::Fill(CAkModulatorCtx* in_pCtx)
{
	static_cast<CAkTimeModCtx*>(in_pCtx)->FillParams(this);
}

class CAkModulatorEngine
{
public:
	~CAkModulatorEngine();

	////
	AKRESULT ProcessModulators( AkUInt32 in_uFrames );
	////

	void CleanUpFinishedCtxs();

	void AddContext(CAkModulatorCtx* in_pCtx);

private:
	
	typedef AkListBare<CAkModulatorBatchBase,AkListBareNextItem,AkCountPolicyWithCount> AkListParamBatch;
	typedef AkListBare<CAkModulatorCtx,AkListBareNextItem,AkCountPolicyWithCount> AkListModulatorCtx;

	AkListModulatorCtx m_listCtx;

	AkListParamBatch m_envBatches;
	AkListParamBatch m_lfoBatches;
	AkListParamBatch m_timBatches;

private:

	template< typename tParams, typename tOutput >
	AKRESULT AddToBatch( AkListParamBatch& in_batchList, AkListParamBatch::IteratorEx& in_it, CAkModulatorCtx* in_pCtx )
	{
		CAkModulatorBatch<tParams,tOutput>* pOldBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(in_it.pItem);

		if( pOldBatch != NULL && pOldBatch->Full() )
		{
			++in_it;
		}

		if ( in_it.pItem == NULL ) //Need to allocate a new batch
		{
			CAkModulatorBatch<tParams,tOutput>* pNewBatch = (CAkModulatorBatch<tParams,tOutput> *) AkAlloc(AkMemID_Object,sizeof(CAkModulatorBatch<tParams,tOutput>));
			if ( pNewBatch ) 
			{	
				AkPlacementNew( pNewBatch ) CAkModulatorBatch<tParams,tOutput>();
				if (pNewBatch->Init(MODULATOR_BATCH_SIZE) != AK_Success)
				{
					AkDelete(AkMemID_Object, pNewBatch);
					return AK_InsufficientMemory;
				}
			}
			else
				return AK_InsufficientMemory;
		
			in_batchList.AddLast(pNewBatch);
			
			in_it.pItem = pNewBatch;
			in_it.pPrevItem = pOldBatch;
		}

		{
			CAkModulatorBatch<tParams,tOutput>* pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(in_it.pItem);

			tParams* params = pBatch->m_params.AddLast();
			AKASSERT(params);
			params->Fill(in_pCtx);
		}

		return AK_Success;
	}

	template< typename tParams, typename tOutput >
	void CleanUpBatches( AkListParamBatch& in_batchList, AkListParamBatch::IteratorEx& in_startAt )
	{
		if (in_startAt != in_batchList.End())
		{
			CAkModulatorBatch<tParams,tOutput>* pPrevBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(*in_startAt);
			if ( pPrevBatch && pPrevBatch->LessThanHalfFull() )
			{
				//if the last batch with elements in it is less than half full, delete the successive batches.
				CAkModulatorBatch<tParams,tOutput>* pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(pPrevBatch->pNextItem);
				while( pBatch )
				{
					AKASSERT(pBatch->IsEmpty());
					pBatch->m_params.RemoveAll();
					pBatch->m_outputs.RemoveAll();
					CAkModulatorBatch<tParams,tOutput>* pToDelete = pBatch;
					in_batchList.RemoveItem(pBatch,pPrevBatch);
					pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(pPrevBatch->pNextItem);
					AkDelete(AkMemID_Object,pToDelete);
				}

			}
		}
	}

	template< typename tParams, typename tOutput >
	void PrepareBatchOutput( AkListParamBatch& in_batchList, AkListParamBatch::Iterator& in_it, CAkModulatorCtx* in_pCtx )
	{
		CAkModulatorBatch<tParams,tOutput>* pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(in_it.pItem);

		tOutput* output = pBatch->m_outputs.AddLast();
		AKASSERT(output);
		pBatch->m_uBufferSizeNeeded += in_pCtx->SetOutput(output);

		if( pBatch->m_params.Length() == pBatch->m_outputs.Length() )
			++in_it;
	}

	template< typename tModProc, typename tParams, typename tOutput >
	static AKRESULT ProcessBatch(CAkModulatorBatchBase* in_batch, AkUInt32 in_uFrames)
	{
		CAkModulatorBatch<tParams,tOutput>* pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(in_batch);
		while( pBatch )
		{
			AKASSERT( pBatch->m_params.Length() == pBatch->m_outputs.Length() );

			if (AK_EXPECT_FALSE( pBatch->AllocBuffers() != AK_Success ))
				return AK_Fail;

			{
				typename CAkModulatorBatch<tParams,tOutput>::ParamsArray::Iterator paramsIt = pBatch->m_params.Begin();
				typename CAkModulatorBatch<tParams,tOutput>::ParamsArray::Iterator paramsEnd = pBatch->m_params.End();
				typename CAkModulatorBatch<tParams,tOutput>::OutputsArray::Iterator outputsIt = pBatch->m_outputs.Begin();

				AkReal32* pBuffer = pBatch->m_pBuffer;

				while( paramsIt != paramsEnd )
				{
					//Need to set the output buffer pointer to the correct offset.
					outputsIt.pItem->m_pBuffer = pBuffer;

					tModProc::Process( *paramsIt, in_uFrames, *outputsIt, outputsIt.pItem->m_pBuffer );

					pBuffer += paramsIt.pItem->m_uBufferSize;
					
					++paramsIt;
					++outputsIt;
				}
			}

			pBatch = (CAkModulatorBatch<tParams,tOutput>*)pBatch->pNextItem;
		}

		return AK_Success;
	}

	template< typename tParams, typename tOutput >
	static void ResetBatch(CAkModulatorBatchBase* in_batch)
	{
		CAkModulatorBatch<tParams,tOutput>* pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(in_batch);
		while( pBatch )
		{
			pBatch->m_uBufferSizeNeeded = 0;
			pBatch->m_params.RemoveAll();
			pBatch->m_outputs.RemoveAll();
			
			pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(pBatch->pNextItem);
		}
	}

	template< typename tParams, typename tOutput >
	static void TermBatch( AkListParamBatch& in_batchList )
	{
		CAkModulatorBatch<tParams,tOutput>* pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(in_batchList.First());
		while( pBatch )
		{
			pBatch->m_params.RemoveAll();
			pBatch->m_outputs.RemoveAll();
			CAkModulatorBatch<tParams,tOutput>* pToDelete = pBatch;
			pBatch = static_cast<CAkModulatorBatch<tParams,tOutput>*>(pBatch->pNextItem);
			AkDelete(AkMemID_Object,pToDelete);
		}
			
		in_batchList.Term();
	}
};

#endif
