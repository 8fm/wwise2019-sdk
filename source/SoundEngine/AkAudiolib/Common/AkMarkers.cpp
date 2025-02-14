/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

#include "stdafx.h"
#include "AkMarkers.h"
#include "AkPlayingMgr.h"
#include "AudiolibDefs.h"

CAkMarkers::CAkMarkers()
	: m_pMarkers( NULL )
{
	m_hdrMarkers.uNumMarkers = 0;  // Markers header.
}

CAkMarkers::~CAkMarkers()
{
	AKASSERT( m_pMarkers == NULL );
}

AKRESULT CAkMarkers::Allocate( AkUInt32 in_uNumMarkers )
{
	AKASSERT( in_uNumMarkers > 0 );
	m_hdrMarkers.uNumMarkers = in_uNumMarkers;
	
	m_pMarkers = (AkAudioMarker*)AkAlloc( AK_MARKERS_POOL_ID, sizeof(AkAudioMarker) * in_uNumMarkers );
	if ( !m_pMarkers )
	{
		// Could not allocate enough cue points.
		m_hdrMarkers.uNumMarkers = 0;
		return AK_InsufficientMemory;
	}
	return AK_Success;
}

void CAkMarkers::Free()
{
	// Clean markers.
    if ( m_pMarkers )
    {
		for( AkUInt32 i=0; i<m_hdrMarkers.uNumMarkers; i++ )
		{
			if( m_pMarkers[i].strLabel )
			{
				AkFree( AK_MARKERS_POOL_ID, m_pMarkers[i].strLabel );
				m_pMarkers[i].strLabel = NULL;
			}
		}

        AkFree( AK_MARKERS_POOL_ID, m_pMarkers );
        m_pMarkers = NULL;
    }
	m_hdrMarkers.uNumMarkers = 0;
}

AKRESULT CAkMarkers::SetLabel( AkUInt32 in_idx, char * in_psLabel, AkUInt32 in_uStrSize )
{
	AKASSERT( in_uStrSize > 0 );
	char * strLabel = (char*)AkAlloc( AK_MARKERS_POOL_ID, in_uStrSize + 1 );
	if ( strLabel )
	{
		AKPLATFORM::AkMemCpy( strLabel, in_psLabel, in_uStrSize );
		strLabel[in_uStrSize] = '\0'; //append final NULL character
		m_pMarkers[in_idx].strLabel = strLabel;
		return AK_Success;
	}
	return AK_InsufficientMemory;
}

void CAkMarkers::SaveMarkersForBuffer(
	CAkMarkersQueue * in_pQueue,
	CAkPBI* in_pCtx,
	AkPipelineBuffer & io_buffer, 
	AkUInt32 in_ulBufferStartPos,
	AkUInt32 in_ulNumFrames)
{
	if ( NeedMarkerNotification( in_pCtx ) )
	{
		AkUInt32 uStartPos = 0;
		AkUInt32 uLength = 0;

		for( unsigned int i = 0; i < m_hdrMarkers.uNumMarkers; i++ )
		{
			if( ( m_pMarkers[i].dwPosition >= in_ulBufferStartPos ) &&
				( m_pMarkers[i].dwPosition < in_ulBufferStartPos + in_ulNumFrames ) )
			{
				if (uLength == 0)
				{
					uStartPos = i;
				}
				uLength++;
			}
		}

		if (uLength > 0)
		{
			AkMarkerNotificationRange range = in_pQueue->Front();
			AkUInt32 prevLength = range.uLength;
			AKRESULT eResult = in_pQueue->Enqueue(m_pMarkers + uStartPos, uLength, in_ulBufferStartPos, in_pCtx);
			if (eResult == AK_Success)
			{
				io_buffer.uPendingMarkerIndex = prevLength;
				io_buffer.uPendingMarkerLength = uLength;
			} // else markers are lost, too bad
		}
	}
}

void CAkMarkers::NotifyRelevantMarkers( CAkPBI * in_pCtx, AkUInt32 in_uStartSample, AkUInt32 in_uStopSample )
{
	if( NeedMarkerNotification( in_pCtx ) )
	{
		for( unsigned int i = 0; i < m_hdrMarkers.uNumMarkers; i++ )
		{
			if( ( m_pMarkers[i].dwPosition >= in_uStartSample ) &&
				( m_pMarkers[i].dwPosition < in_uStopSample ) )
			{
				g_pPlayingMgr->NotifyMarker( in_pCtx, &m_pMarkers[i] );
			}
		}
	}
}

const AkAudioMarker * CAkMarkers::GetClosestMarker( AkUInt32 in_uPosition ) const
{
	AkAudioMarker * pMarker = NULL;
	AkUInt32 uSmallestDistance = 0;

	// Note: we have no guarantee that the markers are sorted.
	for( unsigned int i = 0; i < m_hdrMarkers.uNumMarkers; i++ )
	{
		AkUInt32 uDistance = abs( (int)( m_pMarkers[i].dwPosition - in_uPosition ) );
		if ( !pMarker 
			|| uDistance < uSmallestDistance )
		{
			pMarker = &m_pMarkers[i];
			uSmallestDistance = uDistance;
		}
	}

	return pMarker;
}

void CAkMarkers::TransferRelevantMarkers( 
	AkMarkerNotificationRange in_range,
	AkPipelineBuffer*         in_pInputBuffer,
	AkPipelineBuffer*         io_pBuffer,
	AkUInt32                  in_ulBufferStartOffset, 
	AkUInt32                  in_ulNumFrames
	)
{
	if (in_pInputBuffer->uPendingMarkerLength > 0)
	{
		AkUInt32 uLength = 0;

		AKASSERT(in_pInputBuffer->uPendingMarkerIndex + in_pInputBuffer->uPendingMarkerLength <= in_range.uLength);

		for (AkUInt32 i = 0; i < in_pInputBuffer->uPendingMarkerLength; i++)
		{
			AkMarkerNotification* pMarker = in_range.pMarkers + in_pInputBuffer->uPendingMarkerIndex + i;
			if( ( pMarker->uOffsetInBuffer >= in_ulBufferStartOffset ) &&
				( pMarker->uOffsetInBuffer < (in_ulBufferStartOffset + in_ulNumFrames) ) )
			{
				pMarker->uOffsetInBuffer = 0; // From now on, force transfers to subsequent buffers
				uLength++;
			}
			pMarker++;
		}

		// Transfer the pending markers from input to output
		in_pInputBuffer->uPendingMarkerIndex += uLength;
		in_pInputBuffer->uPendingMarkerLength -= uLength;
		io_pBuffer->uPendingMarkerLength += uLength;
	}
}
