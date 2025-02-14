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

#include "AkVorbisCodebookMgr.h"
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "AkPBI.h"
#include "codebook.h"

#include "AkVorbisCodec.h"


AkVorbisCodebookMgr g_VorbisCodebookMgr;

void Codebook::Term()
{
	if(csi.book_param)
	{
		for(int i = 0; i < csi.books; i++)
		{
			if(csi.book_param[i].dec_table)
				AkFree(AkMemID_Processing, csi.book_param[i].dec_table);
			if(csi.book_param[i].pResidueParams)
				AkFalign(AkMemID_Processing, csi.book_param[i].pResidueParams);
		}
		if(csi.book_param)
			AkFree(AkMemID_Processing, csi.book_param);
	}
	allocator.Term();
}

AkVorbisCodebookMgr::AkVorbisCodebookMgr()
{
}

AkVorbisCodebookMgr::~AkVorbisCodebookMgr()
{
#ifdef _DEBUG
	AKASSERT( m_codebooks.Length() == 0 );
#endif
}

codec_setup_info * AkVorbisCodebookMgr::Decodebook(
	AkVorbisSourceState & in_VorbisState, 
	CAkPBI * in_pPBI,		// For error monitoring
	ogg_packet *op
	)
{
	// Look for existing codebook

	AkAutoLock<CAkLock> codebookLock(m_codebookLock);

	Codebook * pCodebook = m_codebooks.Exists( in_VorbisState.VorbisInfo.uHashCodebook );
	if ( pCodebook )
	{
		++pCodebook->cRef;
		return &pCodebook->csi;
	}

	// Not found; create new codebook entry and decode

	pCodebook = AkNew(AkMemID_Processing, Codebook());
	if (!pCodebook)
		return NULL;

#ifdef AK_POINTER_64
	AkUInt32 uAllocSize = in_VorbisState.VorbisInfo.dwDecodeX64AllocSize;
#else
	AkUInt32 uAllocSize = in_VorbisState.VorbisInfo.dwDecodeAllocSize;
#endif	
	pCodebook->allocator.Init(uAllocSize);

	// Placed here to calm the PS3 compiler	
	int iResult;
	int channels = in_VorbisState.TremorInfo.channelConfig.uNumChannels;

	iResult = vorbis_info_init(
		&pCodebook->csi,
		in_VorbisState.VorbisInfo.uBlockSizes[0],
		in_VorbisState.VorbisInfo.uBlockSizes[1]);
	if ( iResult )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidAudioFileHeader, in_pPBI );
		goto error;
	}

	oggpack_buffer opb;
	oggpack_readinit(&opb,&(op->buffer));

	iResult = vorbis_unpack_books(
		pCodebook,
		channels,
		&opb);	
	if ( iResult )
	{
		MONITOR_SOURCE_ERROR(iResult == OV_NOMEM ? AK::Monitor::ErrorCode_NotEnoughMemoryToStart : AK::Monitor::ErrorCode_InvalidAudioFileHeader, in_pPBI);		
		goto error;
	}

	// Success

	++pCodebook->cRef;

	pCodebook->key = in_VorbisState.VorbisInfo.uHashCodebook;
	if ( ! m_codebooks.Set( pCodebook ) )
	{
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_NotEnoughMemoryToStart, in_pPBI);		
		goto error;
	}

	return &pCodebook->csi;

error:
	pCodebook->Term( );
	AkDelete( AkMemID_Processing, pCodebook );
	return NULL;
}

void AkVorbisCodebookMgr::ReleaseCodebook( AkVorbisSourceState & in_VorbisState )
{
	AkAutoLock<CAkLock> codebookLock(m_codebookLock);
	CodebookList::IteratorEx it = m_codebooks.FindEx( in_VorbisState.VorbisInfo.uHashCodebook );
	if ( it != m_codebooks.End() )
	{
		Codebook * pCodebook = *it;
		if ( --pCodebook->cRef <= 0 )
		{
			m_codebooks.Erase( it );

			pCodebook->Term( );
			AkDelete( AkMemID_Processing, pCodebook );
		}

		if (m_codebooks.Length() == 0)
			m_codebooks.Term();

		return;
	}

	AKASSERT( false && "Vorbis Codebook not found" );
}
