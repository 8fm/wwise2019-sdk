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
// AkGen3DParams.cpp
//
// alessard
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkGen3DParams.h"
#include "AkPathManager.h"
#include "AkDefault3DParams.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>

static const AkVector defaultpos = { AK_DEFAULT_SOUND_POSITION_X, AK_DEFAULT_SOUND_POSITION_Y, AK_DEFAULT_SOUND_POSITION_Z };

AkPositioningParams::AkPositioningParams()
	: m_pAttenuation(NULL)
{
}

AkPositioningParams::~AkPositioningParams()
{
	ReleaseAttenuation();
}

void AkPositioningParams::ReleaseAttenuation()
{
	if (m_pAttenuation)
	{
		m_pAttenuation->Release();
		m_pAttenuation = NULL;
	}
}

Ak3DAutomationParams::Ak3DAutomationParams()
	: m_ID(AK_INVALID_UNIQUE_ID)
	, m_Position(defaultpos)
	, m_ePathMode(AkStepSequence)
	, m_TransitionTime(0)
	, m_pArrayVertex(NULL)
	, m_ulNumVertices(0)
	, m_pArrayPlaylist(NULL)
	, m_ulNumPlaylistItem(0)
	, m_bIsLooping(false)
{
}

Ak3DAutomationParams::~Ak3DAutomationParams()
{
}

void CAk3DAutomationParams::SetTransition(AkTimeMs in_TransitionTime)
{
	m_Params.m_TransitionTime = in_TransitionTime;
	UpdateTransitionTimeInVertex();
}

AKRESULT CAk3DAutomationParams::SetPathPlayList(CAkPath* in_pPath, AkPathState* in_pState)
{
	return g_pPathManager->SetPathsList(in_pPath, 
		m_Params.m_pArrayPlaylist,
		m_Params.m_ulNumPlaylistItem,
		m_Params.m_ePathMode,
		m_Params.m_bIsLooping,
		in_pState);
}

bool CAk3DAutomationParams::PathIsDifferent(
	AkPathVertex*           in_pArrayVertex,
	AkUInt32                 in_ulNumVertices,
	AkPathListItemOffset*   in_pArrayPlaylist,
	AkUInt32                 in_ulNumPlaylistItem
	)
{
	if (m_Params.m_ulNumVertices != in_ulNumVertices)
		return true;
	if (m_Params.m_ulNumPlaylistItem != in_ulNumPlaylistItem)
		return true;

	AkPathVertex * pCurInVertex = in_pArrayVertex;
	AkPathVertex * pCurVertex = m_Params.m_pArrayVertex;

	for (AkUInt32 i = 0; i < in_ulNumPlaylistItem; ++i)
	{
		if (m_Params.m_pArrayPlaylist[i].iNumVertices != in_pArrayPlaylist[i].iNumVertices)
			return true;

		if (m_Params.m_pArrayPlaylist[i].iNumVertices)
		{
			for (AkInt32 iVert = 0; iVert < m_Params.m_pArrayPlaylist[i].iNumVertices-1; ++iVert)
			{
				if (pCurInVertex->Vertex.X != pCurVertex->Vertex.X|| 
					pCurInVertex->Vertex.Y != pCurVertex->Vertex.Y|| 
					pCurInVertex->Vertex.Z != pCurVertex->Vertex.Z|| 
					pCurInVertex->Duration != pCurVertex->Duration)
				{
					return true;
				}

				++pCurInVertex;
				++pCurVertex;
			}

			// Purposely NOT comparing "in_pArrayVertex->Duration" for the last point. 
			// The duration will be updated after in UpdateTransitionTimeInVertex and is wrong to compare if the path is the same or not. 
			if (pCurInVertex->Vertex.X != pCurVertex->Vertex.X ||
				pCurInVertex->Vertex.Y != pCurVertex->Vertex.Y ||
				pCurInVertex->Vertex.Z != pCurVertex->Vertex.Z)
			{
				return true;
			}

			++pCurInVertex;
			++pCurVertex;
		}
	}

	return false;
}

void CAk3DAutomationParams::UpdateTransitionTimeInVertex()
{
	for( AkUInt32 uli = 0; uli < m_Params.m_ulNumPlaylistItem; ++uli )
	{
		if(m_Params.m_pArrayPlaylist[uli].iNumVertices > 0)
		{
			m_Params.m_pArrayPlaylist[uli].pVertices[m_Params.m_pArrayPlaylist[uli].iNumVertices - 1].Duration = m_Params.m_TransitionTime;
		}
	}
}

#ifndef AK_OPTIMIZED
void CAk3DAutomationParams::InvalidatePaths()
{
	// Not the owner of m_pArrayVertex, m_pArrayPlaylist memory: can be simply cleared.
	m_Params.m_pArrayVertex = NULL;
	m_Params.m_ulNumVertices = 0;
	m_Params.m_pArrayPlaylist = NULL;
	m_Params.m_ulNumPlaylistItem = 0;
}
#endif

CAk3DAutomationParamsEx::~CAk3DAutomationParamsEx()
{
	ClearPaths();
	FreePathInfo();
}

AKRESULT CAk3DAutomationParamsEx::SetPath(
	AkPathVertex*           in_pArrayVertex, 
	AkUInt32                 in_ulNumVertices, 
	AkPathListItemOffset*   in_pArrayPlaylist, 
	AkUInt32                 in_ulNumPlaylistItem 
	)
{
	AKRESULT eResult = AK_Success;

	ClearPaths();

	// If there is something valid
	if( ( in_ulNumVertices != 0 ) 
		&& ( in_ulNumPlaylistItem != 0 ) 
		&& ( in_pArrayVertex != NULL ) 
		&& ( in_pArrayPlaylist != NULL ) 
		)
	{
		AkUInt32 ulnumBytesVertexArray = in_ulNumVertices * sizeof( AkPathVertex );
		m_Params.m_pArrayVertex = (AkPathVertex*)AkAlloc( AkMemID_Structure, ulnumBytesVertexArray );

		if(m_Params.m_pArrayVertex)
		{
			AKPLATFORM::AkMemCpy( m_Params.m_pArrayVertex, in_pArrayVertex, ulnumBytesVertexArray );
			m_Params.m_ulNumVertices = in_ulNumVertices;

			AkUInt32 ulnumBytesPlayList = in_ulNumPlaylistItem * sizeof( AkPathListItem );
			m_Params.m_pArrayPlaylist = (AkPathListItem*)AkAlloc(AkMemID_Structure, ulnumBytesPlayList );

			if(m_Params.m_pArrayPlaylist)
			{
				m_Params.m_ulNumPlaylistItem = in_ulNumPlaylistItem;
				AkUInt32 verticesSize = in_ulNumPlaylistItem * sizeof(AkPathListItemOffset);

				AkUInt8 *pPlaylist = (AkUInt8*)in_pArrayPlaylist;

				for( AkUInt32 i = 0; i < in_ulNumPlaylistItem; ++i )
				{
					AkUInt32 ulOffset = READBANKDATA(AkUInt32, pPlaylist, verticesSize);
					m_Params.m_pArrayPlaylist[i].iNumVertices = READBANKDATA(AkInt32, pPlaylist, verticesSize);
					AKASSERT(sizeof(AkPathListItemOffset) == 8);	//If the struct is larger, this code needs to be adapted.

					//Applying the offset to the initial value
					if(in_pArrayPlaylist[i].ulVerticesOffset < in_ulNumVertices )
					{
						m_Params.m_pArrayPlaylist[i].pVertices = m_Params.m_pArrayVertex + ulOffset;
						m_Params.m_pArrayPlaylist[i].fRangeX = 0.f;
						m_Params.m_pArrayPlaylist[i].fRangeY = 0.f;
						m_Params.m_pArrayPlaylist[i].fRangeZ = 0.f;
					}
					else
					{
						AKASSERT( !"Trying to access out of range vertex" );
						eResult = AK_Fail;
						break;
					}
				}
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}
	else
	{
		eResult = AK_InvalidParameter;
	}

	UpdateTransitionTimeInVertex();

	return eResult;
}

void CAk3DAutomationParamsEx::FreePathInfo()
{
	m_PathState.pbPlayed = NULL;	
}

AKRESULT CAk3DAutomationParamsEx::UpdatePathPoint(
		AkUInt32 in_ulPathIndex,
		AkUInt32 in_ulVertexIndex,
		AkVector in_newPosition,
		AkTimeMs in_DelayToNext
		)
{
	AKRESULT eResult = AK_Success;

	AKASSERT( m_Params.m_pArrayVertex != NULL );
	AKASSERT( m_Params.m_pArrayPlaylist != NULL );
	if( ( m_Params.m_pArrayVertex != NULL )
		&& ( m_Params.m_pArrayPlaylist != NULL ) 
		&& ( in_ulPathIndex < m_Params.m_ulNumPlaylistItem )
		&& ( m_Params.m_pArrayPlaylist[in_ulPathIndex].iNumVertices > 0 )// Not useless, done to validate the signed unsigned cast that will be performed afterward
		)
	{
		if (in_ulVertexIndex < (AkUInt32)(m_Params.m_pArrayPlaylist[in_ulPathIndex].iNumVertices))
		{
			AkPathVertex& l_rPathVertex = m_Params.m_pArrayPlaylist[in_ulPathIndex].pVertices[in_ulVertexIndex];
			l_rPathVertex.Duration = in_DelayToNext;
			l_rPathVertex.Vertex = in_newPosition;
			UpdateTransitionTimeInVertex();
		}
		// else silently discard 'extra' update coming from WAL: see WALTransportManager::OnCurveAndPathNotification
	}
	else
	{
		eResult = AK_InvalidParameter;
		AKASSERT(!"It is useless to call UpdatePoints() on 3D Parameters if no points are set yet");
	}
	return eResult;
}

void CAk3DAutomationParamsEx::ClearPaths()
		{
	if (m_Params.m_pArrayVertex)
{
		AkFree(AkMemID_Structure, m_Params.m_pArrayVertex);
		m_Params.m_pArrayVertex = NULL;
}
	if (m_Params.m_pArrayPlaylist)
{
		AkFree(AkMemID_Structure, m_Params.m_pArrayPlaylist);
		m_Params.m_pArrayPlaylist = NULL;
}
	m_Params.m_ulNumVertices = 0;
	m_Params.m_ulNumPlaylistItem = 0;
}
