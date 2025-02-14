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
// AkModulatorCtx.cpp
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkModulatorCtx.h"
#include "AkMidiNoteCtx.h"
#include "AkAudioMgr.h"

CAkModulatorCtx::CAkModulatorCtx()
		: pNextItem(NULL)
		, m_pModulator(NULL)
		, m_pPbi(NULL)
		, m_pGameObj(NULL)
		, m_pMidiNoteState(NULL)
		, m_midiTargetID(AK_INVALID_UNIQUE_ID)
		, m_playingID(AK_INVALID_PLAYING_ID)
		, m_pOutput(NULL)
		, m_uStartOffsetFrames(0)
		, m_uReleaseFrame(INF_SUSTAIN)
		, m_uElapsedFrames(0)
		, m_iRefCount(0)
		, m_eState(AkModulatorState_New)
		, m_eScope(AkModulatorScope_Note)
		, m_fLastOutput(0.0f)
		, m_iVoiceCount(0)
		, m_uBufferSizeNeeded(0)
		, m_iPausedCount(0)
{
}

CAkModulatorCtx::~CAkModulatorCtx()
{
	Term();
	AKASSERT(m_iRefCount==0);
}

AKRESULT CAkModulatorCtx::Trigger(CAkModulator* in_pModulator, const AkModulatorTriggerParams& in_params, CAkParameterNodeBase* in_pTargetNode, AkModulatorScope in_eScope )
{
	//Note: It is possible for this to be called multiple times when triggering the same modulator.
	//      For Game Object scoping we don't want to terminate.
	//      The test was added for (retriggering) game object scoped time modulators, and we've removed that scoping on time mods for now, so remove the test.
	//      It didn't appear to cause problems for LFOs but why take the chance?
	if ((m_pMidiNoteState != in_params.pMidiNoteState || m_playingID != in_params.playingId) /*&& in_eScope != AkModulatorScope_GameObject*/)
	{
		Term();
	}

	AKASSERT(m_pModulator == NULL || in_pModulator == m_pModulator);
	m_pModulator = in_pModulator;

	m_eState = AkModulatorState_Triggered;
	m_uElapsedFrames = 0;
	m_uStartOffsetFrames = in_params.uFrameOffset;

	m_pGameObj = NULL;

	//Keep the channel that was passed in, even for globally- and game-object-scoped modulators.
	//	This allows the modulator to respond to channel-only midi cc messages.
	m_midiTargetID = in_params.midiTargetId;
	m_noteAndChannel = in_params.midiEvent.GetNoteAndChannel();
	m_noteAndChannel.note = AK_INVALID_MIDI_NOTE;

	AKASSERT( m_pModulator != NULL );
	m_eScope = in_eScope;
	if (m_eScope != AkModulatorScope_Global)
	{
		m_pGameObj = in_params.pGameObj;

		if (m_eScope != AkModulatorScope_GameObject)
		{
			if(m_eScope != AkModulatorScope_Note)
			{
				AKASSERT(m_eScope == AkModulatorScope_Voice);
				m_pPbi = in_params.pPbi;
			}

			m_noteAndChannel = in_params.midiEvent.GetNoteAndChannel();
	
			//The note state is only valid if the modulator is scoped by note/container
			if (m_pMidiNoteState == NULL && in_params.pMidiNoteState != NULL)
			{
				m_pMidiNoteState = in_params.pMidiNoteState;
				m_pMidiNoteState->AddRef();
			}

			//The target nodes are only valid if the modulator is scoped by note/container
			AddTargetNode(in_pTargetNode);

			//The playing id is only valid if the modulator is scoped by note/container
			m_playingID = in_params.playingId;

			//If modulator is meant to be note/pid -scope, but no note/ch/pid is provided, mark ctx as game obj.		
			if (m_noteAndChannel.channel == AK_INVALID_MIDI_CHANNEL && m_noteAndChannel.note == AK_INVALID_MIDI_NOTE && m_playingID == AK_INVALID_PLAYING_ID && m_pPbi == NULL)
			{
				m_eScope = AkModulatorScope_GameObject;
				if (in_params.pPbi)
					m_PBIs.AddLast(in_params.pPbi);
			}
		}
		else
		{
			if (in_params.pPbi)
				m_PBIs.AddLast(in_params.pPbi);
		}

		//If modulator is meant to be game-obj-scope, but no game obj is provided, mark ctx as global.
		if (m_pGameObj == NULL)
		{
			m_eScope = AkModulatorScope_Global;
			m_PBIs.Term();
		}

	}


	AkModulatorParams* pParams = GetParamsPtr();
	m_pModulator->GetInitialParams( pParams, this );

	CalcBufferNeeded();

	m_uReleaseFrame = pParams->m_uReleaseFrame;
	
	InitializeOutput();

	return AK_Success;
}

void CAkModulatorCtx::TriggerRelease(AkUInt32 in_uFrameOffset)	
{ 
	if (m_uReleaseFrame == INF_SUSTAIN) // otherwise we are in auto release mode so ignore manual release.
	{
		m_uReleaseFrame = m_uElapsedFrames + in_uFrameOffset;
		if (m_eState != AkModulatorState_Finished)
			m_eState = AkModulatorState_ReleaseTriggered; 
	}
}

void CAkModulatorCtx::Term()
{ 
	m_eState = AkModulatorState_Finished;
	
	if (m_pModulator != NULL)
	{
		bool bStopOnTermination = m_pModulator->StopWhenFinished();
		if (bStopOnTermination)
		{
			if ( m_eScope == AkModulatorScope_Voice  && m_pPbi )
			{
				TransParams transParams;
				transParams.TransitionTime = 0;
				transParams.eFadeCurve = AkCurveInterpolation_Linear;
				m_pPbi->_Stop(transParams, true);
			}
			else if (m_eScope == AkModulatorScope_GameObject && !m_PBIs.IsEmpty())
			{
				TransParams transParams;
				transParams.TransitionTime = 0;
				transParams.eFadeCurve = AkCurveInterpolation_Linear;
				for (AkModulatorCtxPBIList::Iterator it = m_PBIs.Begin(); it != m_PBIs.End(); ++it)
					(*it)->_Stop(transParams, true);
			}
			else if ( m_eScope == AkModulatorScope_Note && !m_arTargetNodes.IsEmpty())
			{
				if (m_pMidiNoteState != NULL) 
				{
					// Modulator was started by a MIDI note.  Use the state saved in m_pMidiNoteState to determine what needs stopping.

					//Stop all playing PBI's
					for( AkMidiNotePBIList::Iterator it = m_pMidiNoteState->GetPBIList().Begin(); 
						it != m_pMidiNoteState->GetPBIList().End(); ++it )
					{
						MidiActionStruct<CAkPBI*>& midiAction= *it;
						if (midiAction.data->GetModulatorData().HasModulationSource(this))
						{
							TransParams transParams;
							transParams.TransitionTime = 0;
							transParams.eFadeCurve = AkCurveInterpolation_Linear;
							midiAction.data->_Stop(transParams, true);
						}
					}

					//Stop all pending actions.
					for( AkMidiNoteActionList::Iterator it = m_pMidiNoteState->GetActionList().Begin(); it != m_pMidiNoteState->GetActionList().End(); ++it )
					{
						MidiActionStruct<CAkActionPlayAndContinue*>& midiAction= *it;
						for ( TargetNodeList::Iterator it2 = m_arTargetNodes.Begin(); it2 != m_arTargetNodes.End(); ++it2 )
						{
							if (midiAction.data->HasModulator(*this))
							{
								g_pAudioMgr->MidiNoteOffExecuted(midiAction.data, *it2, MidiEventActionType_Stop /*Allways stop*/);
							}
						}
					}
				}
				else
				{
					for ( TargetNodeList::Iterator it = m_arTargetNodes.Begin(); it != m_arTargetNodes.End(); ++it )
					{
						CAkParameterNodeBase* pTargetNode = *it;
						// Modulator was started by a play action.
						//TBD: Should we stop pending actions or not?  If there are multiple play actions in an event this is not desirable
						if (g_pAudioMgr != NULL)
							g_pAudioMgr->StopPendingAction( pTargetNode, m_pGameObj, m_playingID );
						pTargetNode->Stop( m_pGameObj, m_playingID );
					}
				}
			}
		}

		m_pModulator = NULL;
	}
	
	m_pPbi = NULL;
	m_PBIs.Term();
	m_playingID = AK_INVALID_PLAYING_ID;
	m_midiTargetID = AK_INVALID_UNIQUE_ID;
	m_noteAndChannel = AkMidiNoteChannelPair();

	while( !m_arTargetNodes.IsEmpty() )
	{
		CAkParameterNodeBase* pTargetNode = m_arTargetNodes.Last();
		m_arTargetNodes.RemoveLast();
		pTargetNode->Release();
	}
	m_arTargetNodes.Term();

	m_pGameObj = NULL;
	
	if ( m_pMidiNoteState != NULL )
	{
		// Necessary local copy because the term may be called again before the ctx is deleted !!
		CAkMidiNoteState* pMidiNoteState = m_pMidiNoteState;
		m_pMidiNoteState = NULL;
		pMidiNoteState->Release();
	}

	m_pOutput = NULL;

	AKASSERT( GetRTPCKey() == AkRTPCKey() );
}

bool CAkModulatorCtx::IsFinished() const 
{
	return IsTerminated()
		||
		m_eState == AkModulatorState_Finished 
		||
		(m_eScope <= AkModulatorScope_Note && m_iVoiceCount == 0);
}

void CAkModulatorCtx::AddTargetNode(CAkParameterNodeBase* in_pTargetNode)
{
	// Usually there is only one target node but some container (continuous) types replace the target node before they are played.  This means that 
	//	we can have more that one target node with the same modulator context.  We need to keep track of all of them so that we can stop them if necessary.
	
	if ( in_pTargetNode != NULL )
	{
		bool bSet = true;

		for ( TargetNodeList::Iterator it = m_arTargetNodes.Begin(); it != m_arTargetNodes.End(); ++it )
		{
			if (in_pTargetNode == *it)
			{
				bSet = false;
				break;
			}
		}

		if (bSet)
		{
			if ( m_arTargetNodes.AddLast(in_pTargetNode) != NULL )
				in_pTargetNode->AddRef();
		}
	}
}

bool CAkLFOCtx::SetParam(AkRTPC_ModulatorParamID in_paramID, AkReal32 in_fValue)
{
	switch (in_paramID)
	{
	case RTPC_ModulatorLfoDepth:
		m_params.m_fDepth = (in_fValue/100.f); 
		return true;
	case RTPC_ModulatorLfoAttack:
		m_params.m_uAttack = AkTimeConv::SecondsToSamples( in_fValue );; 
		return true;
	case RTPC_ModulatorLfoFrequency: 
		m_params.m_DspParams.fFrequency = in_fValue; 
		m_bDspParamsDirty = true;
		CalcBufferNeeded();
		return true;
	case RTPC_ModulatorLfoWaveform:
		m_params.m_DspParams.eWaveform = (DSP::LFO::Waveform)(AkUInt32)in_fValue;
		m_bDspParamsDirty = true;
		return true;
	case RTPC_ModulatorLfoSmoothing:
		m_params.m_DspParams.fSmooth = (in_fValue/100.f);
		m_bDspParamsDirty = true;
		return true;
	case RTPC_ModulatorLfoPWM:
		m_params.m_DspParams.fPWM = (in_fValue/100.f);
		m_bDspParamsDirty = true;
		return true;
	default:
		return false;
	}
}

bool CAkEnvelopeCtx::SetParam(AkRTPC_ModulatorParamID in_paramID, AkReal32 in_fValue)
{
	switch (in_paramID)
	{
	case RTPC_ModulatorEnvelopeAttackTime:
		m_params.m_uAttack = AkTimeConv::SecondsToSamples( in_fValue );
		CalcBufferNeeded();
		return true;
	case RTPC_ModulatorEnvelopeAttackCurve:
		m_params.m_fCurve = (in_fValue/100.f);
		return true;
	case RTPC_ModulatorEnvelopeDecayTime:
		m_params.m_uDecay = AkTimeConv::SecondsToSamples( in_fValue );
		CalcBufferNeeded();
		return true;
	case RTPC_ModulatorEnvelopeSustainLevel:
		m_params.m_fSustain = (in_fValue/100.f);
		return true;
	case RTPC_ModulatorEnvelopeSustainTime:
		if (in_fValue >= 0)
		{
			m_uReleaseFrame = m_params.m_uAttack 
				+ m_params.m_uDecay + AkTimeConv::SecondsToSamples( in_fValue );
		}
		else
		{
			m_uReleaseFrame = INF_SUSTAIN;
		}
		return true;
	case RTPC_ModulatorEnvelopeReleaseTime:
		m_params.m_uRelease = AkTimeConv::SecondsToSamples( in_fValue );
		return true;
	default:
		return false;
	}
}

bool CAkTimeModCtx::SetParam(AkRTPC_ModulatorParamID in_paramID, AkReal32 in_fValue)
{
	switch (in_paramID)
	{
	case RTPC_ModulatorTimePlaybackSpeed:
		m_params.m_fPlaybackSpeed = in_fValue;
		return true;
	case RTPC_ModulatorTimeInitialDelay:
		return true; // Still need to return true even if don't use the incoming value
	default:
		return false;
	}
}

void CAkLFOCtx::CalcBufferNeeded()
{
	if (m_pModulator != NULL && m_pModulator->AutomatedParamSubscribed() && m_params.m_DspParams.fFrequency > (((AkReal32)DEFAULT_NATIVE_FREQUENCY)/((AkReal32)AK_NUM_VOICE_REFILL_FRAMES))/4.f )
		m_uBufferSizeNeeded = AK_NUM_VOICE_REFILL_FRAMES;
	else
		m_uBufferSizeNeeded = 0;
}

void CAkEnvelopeCtx::CalcBufferNeeded()
{
	AkUInt32 minSamples = AkMin( m_params.m_uAttack, m_params.m_uDecay );
	if (m_pModulator != NULL && m_pModulator->AutomatedParamSubscribed() && minSamples < (AkUInt32)(AK_NUM_VOICE_REFILL_FRAMES * 2))
		m_uBufferSizeNeeded = AK_NUM_VOICE_REFILL_FRAMES;
	else
		m_uBufferSizeNeeded = 0;
}

void CAkTimeModCtx::CalcBufferNeeded()
{
	if (m_pModulator != NULL && m_pModulator->AutomatedParamSubscribed())
		m_uBufferSizeNeeded = AK_NUM_VOICE_REFILL_FRAMES;
	else
		m_uBufferSizeNeeded = 0;
}

void CAkEnvelopeCtx::InitializeOutput()
{
	if ( m_params.m_fStartValue != 0.f )
	{
		// Ignore the start offset if start value is not zero.  
		//	The start value is computed from the previous output (at the end of the previous frame), 
		//	so it is inconsistent to have both.
		m_uStartOffsetFrames = 0;
	}

	//handle special start value cases for 0-length envelope segments
	if (m_params.m_uAttack == 0)
	{
		m_params.m_fStartValue = 1.0f;

		if (m_params.m_uDecay == 0)
			m_params.m_fStartValue = m_params.m_fSustain;
	}
}
