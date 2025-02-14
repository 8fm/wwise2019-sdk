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
// AkSound.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkSound.h"
#include "PrivateStructures.h"
#include "AkURenderer.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "ActivityChunk.h"
#include "AkMidiNoteCtx.h"
#include "AkRegistryMgr.h"
#include "AkBankMgr.h"

CAkSound::CAkSound( AkUniqueID in_ulID )
:CAkSoundBase( in_ulID )
{
}

CAkSound::~CAkSound()
{
}

CAkSound* CAkSound::Create( AkUniqueID in_ulID )
{
	CAkSound* pAkSound = AkNew(AkMemID_Structure, CAkSound( in_ulID ) );

	if( pAkSound && pAkSound->Init() != AK_Success )
	{
		pAkSound->Release();
		pAkSound = NULL;
	}
	
	return pAkSound;
}

AkNodeCategory CAkSound::NodeCategory()
{
	return AkNodeCategory_Sound;
}

AKRESULT CAkSound::PlayInternal( AkPBIParams& in_rPBIParams )
{
	//Handle MIDI event behaviour
	if ( in_rPBIParams.midiEvent.IsValid() )
	{
		AkMidiEventEx& midiEvent = in_rPBIParams.midiEvent;

		if (midiEvent.IsNoteOn())
		{
			MidiEventActionType eNoteOnAction = GetMidiNoteOnAction();

			// Set velocity RTPC for note-on events.
			AkRTPCKey rtpcKey(in_rPBIParams.pGameObj, 
				AK_INVALID_PLAYING_ID,	// <- The reason we do not set this, is to support live playing of keyboard though the authoring. 
										//		Without any special previsions each note played from a keyboard will have a new playing ID.  
										//		At this point the playing id possibly hasn't event been created, so the RTPC mgr will reject the value.
				midiEvent.GetNoteAndChannel(), in_rPBIParams.GetMidiTargetID() );
			g_pRTPCMgr->SetMidiParameterValue(AssignableMidiControl_Note, (AkReal32)midiEvent.NoteOnOff.byNote, rtpcKey);
			
			// Velocity is now set in the constructor of the PBI, so that each instance can have a unique velocity.

			AkReal32 fNoteFreq = AkMath::Pow( 2, ((AkReal32)midiEvent.NoteOnOff.byNote - 69) / 12 ) * 440;
			g_pRTPCMgr->SetMidiParameterValue(AssignableMidiControl_Frequency, fNoteFreq, rtpcKey);

			// Save the AkSound node, midi action, transition and the midi note/channel to be played or stopped later (on note off), if necessary.
			MidiEventActionType eNoteOffAction = GetMidiNoteOffAction();
			in_rPBIParams.pMidiNoteState->AttachSound(eNoteOffAction, this, midiEvent.GetNoteAndChannel());
		
			//Bail out if we not going to play the note.
			if ( eNoteOnAction != MidiEventActionType_Play )
			{
				return AK_RejectedByFilter;
			}
		}
	}
	
    AKRESULT eResult = CAkURenderer::Play( this, &m_Source, in_rPBIParams );

	return eResult;
}

void CAkSound::ExecuteAction( ActionParams& in_rAction )
{
	ExecuteActionInternal(in_rAction);
}

void CAkSound::ExecuteActionNoPropagate(ActionParams& in_rAction)
{
	if (in_rAction.eType == ActionParamType_Stop)
		CheckPauseTransition(in_rAction);

	ExecuteActionInternal(in_rAction);
}

void CAkSound::ExecuteActionExcept( ActionParamsExcept& in_rAction )
{
	CheckPauseTransition(in_rAction);
	ExecuteActionInternal(in_rAction);
}

void CAkSound::ExecuteActionInternal(ActionParams& in_rAction)
{
	if (IsPlaying())
	{
		switch (in_rAction.eType)
		{
		case ActionParamType_Seek:
			SeekSound((SeekActionParams&)in_rAction);
			break;
		default:
			ProcessCommand(in_rAction);
			break;
		}
	}
}

void CAkSound::SeekSound( const SeekActionParams & in_rActionParams )
{
	if ( in_rActionParams.bIsSeekRelativeToDuration )
	{
		AkReal32 fSeekPercent = in_rActionParams.fSeekPercent;
		
		// Clamp to [0,1].
		if ( fSeekPercent < 0 )
			fSeekPercent = 0;
		else if ( fSeekPercent > 1 )
			fSeekPercent = 1;

		if( IsActivityChunkEnabled() )
		{
			AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
			for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
			{
				CAkPBI * l_pPBI = (CAkPBI *)*iter;
				if( CheckObjAndPlayingID( in_rActionParams.pGameObj, l_pPBI->GetGameObjectPtr(), in_rActionParams.playingID, l_pPBI->GetPlayingID() ) )
				{
					l_pPBI->SeekPercent( fSeekPercent, in_rActionParams.bSnapToNearestMarker );
				}
			}
		}
	}
	else
	{
		AkTimeMs iSeekPosition = in_rActionParams.iSeekTime;
		
		// Negative seek positions are not supported on sounds: clamped to 0.
		if ( iSeekPosition < 0 )
			iSeekPosition = 0;

		if( IsActivityChunkEnabled() )
		{
			AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
			for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
			{
				CAkPBI * l_pPBI = (CAkPBI *)*iter;
				if( CheckObjAndPlayingID( in_rActionParams.pGameObj, l_pPBI->GetGameObjectPtr(), in_rActionParams.playingID, l_pPBI->GetPlayingID() ) )
				{
					l_pPBI->SeekTimeAbsolute( iSeekPosition, in_rActionParams.bSnapToNearestMarker );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Name: Category
// Desc: Returns the category of the sound.
//
// Parameters: None.
//
// Return: AkObjectCategory.
//-----------------------------------------------------------------------------
AkObjectCategory CAkSound::Category()
{
	return ObjCategory_Sound;
}

//-----------------------------------------------------------------------------
// Name: SetInitialValues
// Desc: Sets the initial Bank source.
//
// Return: Ak_Success:          Initial values were set.
//         AK_InvalidParameter: Invalid parameters.
//         AK_Fail:             Failed to set the initial values.
//-----------------------------------------------------------------------------
AKRESULT CAkSound::SetInitialValues( AkUInt8* in_pData, AkUInt32 in_ulDataSize, CAkUsageSlot* /*in_pUsageSlot*/, bool in_bIsPartialLoadOnly )
{
	AKOBJECT_TYPECHECK(AkNodeCategory_Sound);

	AKRESULT eResult = AK_Success;

	//Read ID
	// We don't care about the ID, just read/skip it
	SKIPBANKDATA(AkUInt32, in_pData, in_ulDataSize);

	//Read Source info
	if(eResult == AK_Success)
	{
		AkBankSourceData oSourceInfo;
		eResult = CAkBankMgr::LoadSource(in_pData, in_ulDataSize, oSourceInfo);
		if (eResult != AK_Success)
			return eResult;

		if (oSourceInfo.m_pParam == NULL)
		{
			//This is a file source, specified by ID.
			SetSource( oSourceInfo.m_PluginID, oSourceInfo.m_MediaInfo );
		}
		else
		{
			//This is a plugin
			SetSource( oSourceInfo.m_MediaInfo.sourceID );
		}
	}

	//ReadParameterNode
	eResult = SetNodeBaseParams(in_pData, in_ulDataSize, in_bIsPartialLoadOnly);

	if( in_bIsPartialLoadOnly )
	{
		//Partial load has been requested, probable simply replacing the actual source created by the Wwise on load bank.
		return eResult;
	}

	CHECKBANKDATASIZE( in_ulDataSize, eResult );

	return eResult;
}

AKRESULT CAkSound::PrepareData()
{
	return m_Source.PrepareData();
}

void CAkSound::UnPrepareData()
{
	m_Source.UnPrepareData();
}

AKRESULT CAkSound::GetAudioParameters(AkSoundParams &io_Parameters, AkMutedMap& io_rMutedMap, const AkRTPCKey& in_rtpcKey, AkPBIModValues* io_pRanges, AkModulatorsToTrigger* out_pModulators, bool in_bDoBusCheck /*= true*/, CAkParameterNodeBase* in_pStopAtNode)
{
	AkDeltaMonitorObjBrace braceDelta(key);
	AKRESULT res = CAkParameterNode::GetAudioParameters(io_Parameters, io_rMutedMap, in_rtpcKey, io_pRanges, out_pModulators, in_bDoBusCheck, in_pStopAtNode );

	// MIDI
	if ( in_rtpcKey.MidiChannelNo() != AK_INVALID_MIDI_CHANNEL )
	{
		AkInt32 iRootNote = 0;
		if( IsMidiNoteTracking( iRootNote ) )
		{
			AkReal32 notePitchScale = ((AkReal32)(in_rtpcKey.MidiNoteNo()) - (AkReal32)iRootNote) * 100.f;
			io_Parameters.AddToPitch(notePitchScale, AkDelta_BaseValue);
		}
	}

	return res;
}

void CAkSound::GatherSounds(AkSoundArray& io_aActiveSounds, AkSoundArray& io_aInactiveSounds, AkGameSyncArray& io_aGameSyncs, bool in_bIsActive, CAkRegisteredObj* in_pGameObj, AkUInt32 in_uUpdateGameSync, AkUInt32 in_uNewGameSyncValue)
{
	if (GetSrcTypeInfo()->mediaInfo.Type == SrcTypeFile)
	{
		if (in_bIsActive)
		{
			io_aActiveSounds.AddLast( GetSrcTypeInfo() );
		}
		else
		{
			io_aInactiveSounds.AddLast( GetSrcTypeInfo() );
		}
	}
}

void CAkSound::SetNonCachable(bool in_bNonCachable)
{ 
	m_Source.GetSrcTypeInfo()->mediaInfo.bNonCachable = in_bNonCachable; 
}
