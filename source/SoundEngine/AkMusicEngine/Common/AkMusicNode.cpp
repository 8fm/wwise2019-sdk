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
// AkMusicNode.cpp
//
// The Music node is meant to be a parent of all playable music objects (excludes tracks).
// Has the definition of the music specific Play method.
// Defines the method for grid query (music objects use a grid, either their own, or that of their parent).
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkMusicNode.h"
#include "AkMusicRenderer.h"

#define QUARTER_NOTE_METER_VALUE    (4)



CAkMusicNode::CAkMusicNode( AkUniqueID in_ulID )
:CAkActiveParent<CAkParameterNode>( in_ulID )
,m_bOverrideParentMidiTempo( false )
,m_bOverrideParentMidiTarget( false )
,m_bOverrideParentGrid( false )
,m_bMidiTargetTypeBus( false )
,m_pStingers( NULL )
{
}

CAkMusicNode::~CAkMusicNode()
{
	FlushStingers();
}

void CAkMusicNode::FlushStingers()
{
	if( m_pStingers )
	{
		m_pStingers->Term();
		AkDelete(AkMemID_Structure, m_pStingers);
		m_pStingers = NULL;
	}
}

AKRESULT CAkMusicNode::PrepareData()
{
	AKRESULT eResult = PrepareMusicalDependencies();
	if( eResult == AK_Success )
	{
		eResult = CAkActiveParent<CAkParameterNode>::PrepareData();
		if( eResult != AK_Success )
		{
			UnPrepareMusicalDependencies();
		}
	}

	return eResult;
}

void CAkMusicNode::UnPrepareData()
{
	CAkActiveParent<CAkParameterNode>::UnPrepareData();
	UnPrepareMusicalDependencies();
}

AKRESULT CAkMusicNode::PrepareMusicalDependencies()
{
	AKRESULT eResult = AK_Success;
	if( m_pStingers )
	{
		StingerArray& rStingerArray = m_pStingers->GetStingerArray();

		for( StingerArray::Iterator iter = rStingerArray.Begin(); iter != rStingerArray.End(); ++iter )
		{
			eResult = PrepareNodeData( (*iter).SegmentID() );

			if( eResult != AK_Success )
			{
				// iterate to undo what has been done up to now
				for( StingerArray::Iterator iterFlush = rStingerArray.Begin(); iterFlush != iter; ++iterFlush )
				{
					UnPrepareNodeData( (*iterFlush).SegmentID() );
				}
				break;	
			}
		}
	}
	return eResult;
}

void CAkMusicNode::UnPrepareMusicalDependencies()
{
	if( m_pStingers )
	{
		StingerArray& rStingerArray = m_pStingers->GetStingerArray();

		for( StingerArray::Iterator iter = rStingerArray.Begin(); iter != rStingerArray.End(); ++iter )
		{
			UnPrepareNodeData( (*iter).SegmentID() );
		}
	}
}

AKRESULT CAkMusicNode::SetMusicNodeParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool /*in_bPartialLoadOnly*/ )
{
	//Read ID
	// We don't care about the ID, just skip it
	SKIPBANKDATA( AkUInt32, io_rpData, io_rulDataSize );

	// Read overrides
	AkUInt8 uFlags = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	m_bOverrideParentMidiTempo	=	(uFlags >> AkMusicNode_FlagBitPos_OverrideMidiTempo) & 1;
	m_bOverrideParentMidiTarget	=	(uFlags >> AkMusicNode_FlagBitPos_OverrideMidiTarget) & 1;
	m_bMidiTargetTypeBus =			(uFlags >> AkMusicNode_FlagBitPos_MidiTargetTypeBus) & 1;

	//ReadParameterNode
	AKRESULT eResult = SetNodeBaseParams( io_rpData, io_rulDataSize, false );
	if ( eResult != AK_Success )
		return eResult;

	eResult = SetChildren( io_rpData, io_rulDataSize );
	if ( eResult != AK_Success )
		return eResult;

	// Read meter info only if it's present
	AkMeterInfo l_meterInfo;
	l_meterInfo.fGridPeriod		= READBANKDATA( AkReal64, io_rpData, io_rulDataSize );        
	l_meterInfo.fGridOffset		= READBANKDATA( AkReal64, io_rpData, io_rulDataSize );         
	l_meterInfo.fTempo			= READBANKDATA( AkReal32, io_rpData, io_rulDataSize );             
	l_meterInfo.uTimeSigNumBeatsBar		= READBANKDATA( AkUInt8, io_rpData, io_rulDataSize ); 
	l_meterInfo.uTimeSigBeatValue		= READBANKDATA( AkUInt8, io_rpData, io_rulDataSize ); 

	if( READBANKDATA( AkUInt8, io_rpData, io_rulDataSize ) )// If override parent enabled
	{
		MeterInfo( &l_meterInfo );
	}

	AkUInt32 l_NumStingers			 = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	if( l_NumStingers )
	{
		CAkStinger* pStingers = (CAkStinger*)AkAlloc(AkMemID_Object, sizeof( CAkStinger )* l_NumStingers );
		if( !pStingers )
			return AK_Fail;
		for( AkUInt32 i = 0; i < l_NumStingers; ++i )
		{
			pStingers[i].m_TriggerID = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
			pStingers[i].m_SegmentID = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
			pStingers[i].m_SyncPlayAt = (AkSyncType)READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
			pStingers[i].m_uCueFilterHash = READBANKDATA( AkUniqueID, io_rpData, io_rulDataSize );
			pStingers[i].m_DontRepeatTime = READBANKDATA( AkInt32, io_rpData, io_rulDataSize );
			pStingers[i].m_numSegmentLookAhead = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
		}
		eResult = SetStingers( pStingers, l_NumStingers );
		AkFree(AkMemID_Object, pStingers );
	}
	else
	{
		eResult = SetStingers( NULL, 0 );
	}

	return eResult;
}

void CAkMusicNode::MeterInfo(
    const AkMeterInfo * in_pMeterInfo         // Music grid info. NULL if inherits that of parent.
    )
{
    if (!in_pMeterInfo)
    {
        m_bOverrideParentGrid = false;
        return;
    }

    m_bOverrideParentGrid = true;

    AKASSERT( in_pMeterInfo->fTempo > 0
			&& in_pMeterInfo->uTimeSigBeatValue > 0 
			&& in_pMeterInfo->uTimeSigNumBeatsBar > 0 
			&& in_pMeterInfo->fGridPeriod > 0 );

	m_grid.fTempo = in_pMeterInfo->fTempo;
    m_grid.uBeatDuration = AkTimeConv::SecondsToSamplesRoundedUp( ( 60.0 / (AkReal64) in_pMeterInfo->fTempo ) * ( (AkReal64)QUARTER_NOTE_METER_VALUE / (AkReal64)in_pMeterInfo->uTimeSigBeatValue ) );
    m_grid.uBarDuration = m_grid.uBeatDuration * in_pMeterInfo->uTimeSigNumBeatsBar;
    m_grid.uGridDuration = AkTimeConv::MillisecondsToSamplesRoundedUp(in_pMeterInfo->fGridPeriod);
    m_grid.uGridOffset = AkTimeConv::MillisecondsToSamplesRoundedUp(in_pMeterInfo->fGridOffset);
}

const AkMusicGrid & CAkMusicNode::GetMusicGrid()
{
	if ( !m_bOverrideParentGrid )
	{
		if( Parent() )
		{
			return static_cast<CAkMusicNode*>( Parent() )->GetMusicGrid();
		}
		// Error message to pad the known situation where the user can add a transition segment that is not part of the
		// same music tree, then explicitely exclude the parent of the transition segment.( WG-19847 )
		MONITOR_ERRORMSG( AKTEXT("Missing music node parent. Make sure your banks using music structure are completely loaded.") );
	}
	return m_grid;
}

// Music implementation of game triggered actions handling ExecuteAction(): 
// For Stop/Pause/Resume, call the music renderer, which searches among its
// contexts (music renderer's contexts are the "top-level" contexts).
// Other actions (actions on properties) are propagated through node hierarchy.
void CAkMusicNode::ExecuteAction( ActionParams& in_rAction )
{
	// Note: temporarily add ref as CAkMusicRenderer::Stop() may destroy this.
	AddRef();
	
	ExecuteActionInternal(in_rAction);
		 
	CAkActiveParent<CAkParameterNode>::ExecuteAction( in_rAction );
	
	Release();
}

void CAkMusicNode::ExecuteActionNoPropagate(ActionParams& in_rAction)
{	
	AddRef();
	ExecuteActionInternal(in_rAction);
	CAkActiveParent<CAkParameterNode>::ExecuteActionNoPropagate(in_rAction);
	Release();
}

// Music implementation of game triggered actions handling ExecuteAction(): 
// For Stop/Pause/Resume, call the music renderer, which searches among its
// contexts (music renderer's contexts are the "top-level" contexts).
// Other actions (actions on properties) are propagated through node hierarchy.
void CAkMusicNode::ExecuteActionExcept( 
	ActionParamsExcept& in_rAction 
	)
{
    // Note: temporarily add ref as CAkMusicRenderer::Stop() may destroy this.
	AddRef();

	ExecuteActionInternal(in_rAction);
		 
	CAkActiveParent<CAkParameterNode>::ExecuteActionExcept( in_rAction );
	
	Release();
}

void CAkMusicNode::ExecuteActionInternal(ActionParams& in_rAction)
{
	// Catch Stop/Pause/Resume actions.
	switch (in_rAction.eType)
	{
	case ActionParamType_Stop:
		CAkMusicRenderer::Stop(this, in_rAction.pGameObj, in_rAction.transParams, in_rAction.playingID);
		break;
	case ActionParamType_Pause:
		CAkMusicRenderer::Pause(this, in_rAction.pGameObj, in_rAction.transParams, in_rAction.playingID);
		break;
	case ActionParamType_Resume:
		CAkMusicRenderer::Resume(this, in_rAction.pGameObj, in_rAction.transParams, in_rAction.bIsMasterResume, in_rAction.playingID);
		break;
	default:
		break;
	}
}

// Block some parameters notifications (pitch)
void CAkMusicNode::ParamNotification( NotifParams& in_rParams )
{
	if ( in_rParams.eType != RTPC_Pitch )
		CAkActiveParent<CAkParameterNode>::ParamNotification( in_rParams );
}

// Block some RTPC notifications (playback speed)
void CAkMusicNode::RecalcNotificationWithID( bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID )
{
	if ( in_rtpcID == RTPC_PlaybackSpeed )
		CAkMusicRenderer::MusicNotification( in_bLiveEdit );
	else
		CAkActiveParent<CAkParameterNode>::RecalcNotification( in_bLiveEdit );
}

void CAkMusicNode::RecalcNotificationWithBitArray( bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray )
{
	if ( in_bitArray.IsSet( RTPC_PlaybackSpeed ) )
		CAkMusicRenderer::MusicNotification( in_bLiveEdit );

	AkRTPCBitArray masked( in_bitArray );
	masked.Mask( RTPC_PlaybackSpeed, false );

	if ( ! masked.IsEmpty() )
		CAkActiveParent<CAkParameterNode>::RecalcNotification( in_bLiveEdit );
}

void CAkMusicNode::PushParamUpdate(AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpcKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue, AkRTPCExceptionChecker* in_pExceptCheck)
{
	if (in_ParamID == RTPC_PlaybackSpeed)
		CAkMusicRenderer::MusicNotification(false);
	else
		CAkParameterNode::PushParamUpdate(in_ParamID, in_rtpcKey, in_fValue, in_fDeltaValue, in_pExceptCheck);
}

void CAkMusicNode::SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax )
{
	AkDeltaMonitorObjBrace braceObj(ID());
#ifdef AKPROP_TYPECHECK
	AKASSERT( typeid(AkReal32) == *g_AkPropTypeInfo[ in_eProp ] );
#endif

	// Notify music renderer for relevant properties.
	if ( in_eProp == AkPropID_PlaybackSpeed )
	{
		AkReal32 fDelta = in_fValue - m_props.GetAkProp(AkPropID_PlaybackSpeed, 0.0f).fValue;
		if ( fDelta != 0.0f )
		{
			m_props.SetAkProp(AkPropID_PlaybackSpeed, in_fValue );
			CAkMusicRenderer::MusicNotification( false );
		}
	}
	
	CAkParameterNode::SetAkProp( in_eProp, in_fValue, in_fMin, in_fMax );
}

// Set the value of a property at the node level (int)
void CAkMusicNode::SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax )
{
	CAkParameterNode::SetAkProp( in_eProp, in_iValue, in_iMin, in_iMax );
}

// Music notifications. Forward to music renderer.
void CAkMusicNode::MusicNotification(
	AkReal32			in_RTPCValue,	// Value
	AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a positioning ID.
	CAkRegisteredObj *	in_GameObj		// Target Game Object
	)
{
	// Notify only if necessary.
	if( in_ParameterID == RTPC_PlaybackSpeed )
		CAkMusicRenderer::MusicNotification( false );
}

// Pull music parameters from the hierarchy.
void CAkMusicNode::GetMusicParams(
	AkMusicParams & io_musicParams,
	const AkRTPCKey& in_rtpcKey
	)
{
	GetPropAndRTPCAndStateMult( io_musicParams.fPlaybackSpeed, AkPropID_PlaybackSpeed, in_rtpcKey );

	if(m_pParentNode != NULL)
	{
		// Parent node has to be a music node
		((CAkMusicNode*)m_pParentNode)->GetMusicParams( io_musicParams, in_rtpcKey );
	}
}

AkObjectCategory CAkMusicNode::Category()
{
	return ObjCategory_MusicNode;
}

AKRESULT CAkMusicNode::SetStingers( CAkStinger* in_pStingers, AkUInt32 in_NumStingers )
{
	if( !in_NumStingers )
	{
		FlushStingers();
	}
	else
	{
		if( !m_pStingers )
		{
			m_pStingers = AkNew(AkMemID_Structure, CAkStingers );
			if( !m_pStingers )
				return AK_Fail;
			if( m_pStingers->GetStingerArray().Reserve( in_NumStingers ) != AK_Success )
				return AK_Fail;
		}
		else
		{
			m_pStingers->RemoveAllStingers();
		}
		for( AkUInt32 i = 0; i < in_NumStingers; ++i )
		{
			if( !m_pStingers->GetStingerArray().AddLast( *( in_pStingers++ ) ) )
				return AK_Fail;
		}
	}
	return AK_Success;
}

void CAkMusicNode::GetStingers( CAkStingers* io_pStingers )
{
	//  Example usage:
	//
	//	CAkMusicNode::CAkStingers Stingers;
	//  GetStingers( &Stingers );
	//  ...
	//	Use Stingers 
	//	...
	//  Stingers.Term()

	AKASSERT( io_pStingers );// the caller must provide the CAkMusicNode::CAkStingers object
	if( Parent() )
	{
		static_cast<CAkMusicNode*>( Parent() )->GetStingers( io_pStingers );
	}
	if( m_pStingers )
	{
		StingerArray& rStingerArray = m_pStingers->GetStingerArray();
		StingerArray& rStingerArrayIO = io_pStingers->GetStingerArray();

		for( StingerArray::Iterator iter = rStingerArray.Begin(); iter != rStingerArray.End(); ++iter )
		{
			CAkStinger& stinger = *iter;

			StingerArray::Iterator iterIO = rStingerArrayIO.Begin();
			while( iterIO != rStingerArrayIO.End() )
			{
				CAkStinger& stingerIO = *iterIO;
				if( stingerIO.m_TriggerID == stinger.m_TriggerID )
				{
					iterIO = rStingerArrayIO.EraseSwap( iterIO );
				}
				else
				{
					++iterIO;
				}
			}

			rStingerArrayIO.AddLast( stinger );
		}
	}
}

void CAkMusicNode::GetMidiTargetNode( bool & r_bOverrideParent, AkUniqueID & r_uMidiTargetId, bool & r_bIsMidiTargetBus )
{
	r_bOverrideParent = m_bOverrideParentMidiTarget != 0;
	r_uMidiTargetId = (AkUniqueID)m_props.GetAkProp( AkPropID_MidiTargetNode, (AkInt32)AK_INVALID_UNIQUE_ID ).iValue;
	r_bIsMidiTargetBus = m_bMidiTargetTypeBus != 0;
}

void CAkMusicNode::GetMidiTempoSource( bool & r_bOverrideParent, AkMidiTempoSource & r_eTempoSource )
{
	r_bOverrideParent = m_bOverrideParentMidiTempo != 0;
	r_eTempoSource = (AkMidiTempoSource)m_props.GetAkProp( AkPropID_MidiTempoSource, (AkInt32)AkMidiTempoSource_Hierarchy ).iValue;
}

void CAkMusicNode::GetMidiTargetTempo( bool & r_bOverrideParent, AkReal32 & r_fTargetTempo )
{
	r_bOverrideParent = m_bOverrideParentGrid != 0;
	r_fTargetTempo = m_bOverrideParentGrid ? m_grid.fTempo : 0.f;
}

void CAkMusicNode::SetOverride( AkMusicOverride in_eOverride, bool in_bValue )
{
	AkUInt8 uValue = in_bValue ? 1 : 0;
	switch ( in_eOverride )
	{
	case AkMusicOverride_MidiTempo: m_bOverrideParentMidiTempo = uValue; break;
	case AkMusicOverride_MidiTarget: m_bOverrideParentMidiTarget = uValue; break;
	default: break;
	}
}

void CAkMusicNode::SetFlag( AkMusicFlag in_eFlag, bool in_bValue )
{
	AkUInt8 uValue = in_bValue ? 1 : 0;
	switch( in_eFlag )
	{
	case AkMusicFlag_MidiTargetIsBus: m_bMidiTargetTypeBus = uValue; break;
	}
}
