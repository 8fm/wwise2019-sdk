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
// AkModulatorCtx.h
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#ifndef _MODULATOR_CTX_H_
#define _MODULATOR_CTX_H_

#include "AkModulator.h"
#include "AkModulatorParams.h"
#include <AK/Tools/Common/AkArray.h>

////
// CAkModulatorCtx 
//
// Represents a running instance of a modulator.  In many cases there needs to be a unique context 
//		per game object or per midi note.  In some cases it is possible to share contexts (eg global LFOs)
////
class CAkModulatorCtx
{
public:
	CAkModulatorCtx();
	virtual ~CAkModulatorCtx();

	AKRESULT Trigger(CAkModulator* in_pModulator, const AkModulatorTriggerParams& in_params, CAkParameterNodeBase* in_pTargetNode, AkModulatorScope in_eScope);
	void TriggerRelease(AkUInt32 in_uFrameOffset);

	void Term();

	AkReal32 GetCurrentValue() const { return m_pOutput ? m_pOutput->m_fOutput : m_fLastOutput; }
	AkReal32 GetPreviousValue() const { return m_fLastOutput; }
	AkReal32 GetPeakValue() const { return m_pOutput ? m_pOutput->m_fPeak : m_fLastOutput; }

	//Called from RTPC manager when a param is updated.
	virtual bool SetParam(AkRTPC_ModulatorParamID in_paramID, AkReal32 in_fValue) = 0;

	//Called by the modulator engine before each process call.
	void FillParams(AkModulatorParams* out_pParams)
	{ 
		ValidateState(m_eState);
		out_pParams->m_eState = m_eState;
		out_pParams->m_uElapsedFrames = m_uElapsedFrames;
		out_pParams->m_uReleaseFrame = m_uReleaseFrame;
		out_pParams->m_uStartOffsetFrames = m_uStartOffsetFrames;
		out_pParams->m_fPreviousOutput = m_fLastOutput;
		out_pParams->m_uBufferSize = m_uBufferSizeNeeded;
	}

	CAkModulator* Modulator() const		{return m_pModulator;}
	AkModulatorType GetType() const		{AKASSERT(m_pModulator); return m_pModulator->GetType();}
	AkUniqueID GetModulatorID() const	{AKASSERT(m_pModulator); return m_pModulator->GetModulatorID();}

	AkUInt32 SetOutput(AkModulatorOutput* in_pOutput)
	{ 
		m_pOutput = in_pOutput; 
		return m_uBufferSizeNeeded;
	}

	AkReal32* GetOutputBuffer() const { return m_pOutput && m_uBufferSizeNeeded > 0 ? m_pOutput->m_pBuffer : NULL; }

	//Called before Process
	virtual void Tick(AkUInt32 in_uFrames)
	{
		if (!IsPaused())
		{
			if (m_pOutput)
			{
				if (m_pOutput->m_eNextState != AkModulatorState_Invalid)
				{
					m_eState = m_pOutput->m_eNextState;
					ValidateState(m_eState);
				}

				m_fLastOutput = m_pOutput->m_fOutput;
			}

			m_uElapsedFrames += in_uFrames;  
		}
	}

	bool IsFinished() const;

	bool IsTerminated() const { return m_pModulator == NULL; }
	
	bool IsPaused() const 
	{ 
		return m_eScope != AkModulatorScope_Global && m_iPausedCount >= m_iVoiceCount; 
	}

	void Pause()		
	{ 
		AKASSERT(m_iPausedCount >= 0); 
		m_iPausedCount++; 
		if (IsPaused() && m_pOutput != NULL)
		{
			m_fLastOutput = m_pOutput->m_fOutput;
			m_pOutput = NULL;
		}
	}

	void Resume()		
	{
		AKASSERT(m_iPausedCount > 0); 
		m_iPausedCount--; 
	}

	AkUInt32 AddRef() { return ++m_iRefCount; }
	AkUInt32 Release() 
	{ 
		AkUInt32 refCount = --m_iRefCount; 
		if (m_iRefCount==0)
		{	
			AkDelete(AkMemID_Object, this);
		}

		return refCount;
	}

	AkUInt32 GetElapsedFrames() const {return m_uElapsedFrames;}
	AkUInt32 GetReleaseFrame() const {return m_uReleaseFrame;}

	void SetScope( AkModulatorScope in_eScope );
	AkModulatorScope GetScope() const {return m_eScope;}

	const CAkRegisteredObj* GetGameObj() const {return m_pGameObj;}
	const AkMidiNoteChannelPair GetNoteAndChannel() const {return m_noteAndChannel;}

	void IncrVoiceCount() {++m_iVoiceCount;}
	void DecrVoiceCount() {--m_iVoiceCount;}

	AkRTPCKey GetRTPCKey() const
	{
		AkRTPCKey key;
		key.GameObj() = m_pGameObj;
		key.PlayingID() = m_playingID;
		key.MidiTargetID() = m_midiTargetID;
		key.MidiChannelNo() = m_noteAndChannel.channel;
		key.MidiNoteNo() = m_noteAndChannel.note;
		key.PBI() = m_pPbi;
		return key;
	}

	void RemovePbiRef(){ m_pPbi = NULL; m_PBIs.Term(); }

	void MarkFinished() { m_eState = AkModulatorState_Finished; }

public:
	CAkModulatorCtx* pNextItem;

protected:

	void AddTargetNode(CAkParameterNodeBase* in_pTargetNode);

	//Gets a pointer to the ctx's local params struct
	virtual AkModulatorParams* GetParamsPtr() = 0;

	virtual void CalcBufferNeeded() = 0;
	virtual void InitializeOutput() = 0;

	CAkModulator* m_pModulator;
	
	// For voice scope
	CAkPBI*	m_pPbi;
	// List of PBIs for game object scope
	typedef AkArray< CAkPBI*, CAkPBI* > AkModulatorCtxPBIList;
	AkModulatorCtxPBIList m_PBIs;

	
	typedef	AkArray< CAkParameterNodeBase*, CAkParameterNodeBase*, ArrayPoolDefault > TargetNodeList;
	TargetNodeList m_arTargetNodes;

	CAkRegisteredObj* m_pGameObj;
	CAkMidiNoteState* m_pMidiNoteState;
	AkMidiNoteChannelPair m_noteAndChannel;

	AkUniqueID m_midiTargetID;
	AkPlayingID m_playingID;

	AkModulatorOutput* m_pOutput;

	AkUInt32 m_uStartOffsetFrames;
	AkUInt32 m_uReleaseFrame;
	AkUInt32 m_uElapsedFrames;

	AkInt32 m_iRefCount;

	AkModulatorState m_eState;
	AkModulatorScope m_eScope;

	// The parameter update are expected to be incremental in some cases, so we need to store the previous value.
	AkReal32 m_fLastOutput;

	AkInt32 m_iVoiceCount;

	AkUInt32 m_uBufferSizeNeeded;

	AkInt32 m_iPausedCount;
};

//
//
class CAkLFOCtx: public CAkModulatorCtx
{
public:
	CAkLFOCtx(): m_bDspParamsDirty(false) {}
	virtual ~CAkLFOCtx(){};

	void FillParams( AkLFOParams* out_pParams )
	{ 
		AkLFOOutput* pLastOutput = static_cast<AkLFOOutput*>(m_pOutput);
		if (pLastOutput)
		{
			//Update the state from the last output.
			DSP::LFO::State state;
			pLastOutput->m_lfo.GetState(state);
			m_params.m_lfo.PutState(state);
			if (m_bDspParamsDirty)
			{
				m_params.m_lfo.SetParams(DEFAULT_NATIVE_FREQUENCY, m_params.m_DspParams);
				m_bDspParamsDirty = false;
			}
		}

		//*out_pParams = m_params;
		out_pParams->m_fDepth = m_params.m_fDepth;
		out_pParams->m_uAttack  = m_params.m_uAttack;
		out_pParams->m_fInitialPhase = m_params.m_fInitialPhase;

		out_pParams->m_DspParams.eWaveform = m_params.m_DspParams.eWaveform;
		out_pParams->m_DspParams.fFrequency = m_params.m_DspParams.fFrequency;
		out_pParams->m_DspParams.fSmooth = m_params.m_DspParams.fSmooth;
		out_pParams->m_DspParams.fPWM = m_params.m_DspParams.fPWM;

		out_pParams->m_lfo = m_params.m_lfo;

		CAkModulatorCtx::FillParams(out_pParams);
	};

	virtual bool SetParam(AkRTPC_ModulatorParamID in_paramID, AkReal32 in_fValue);

	virtual void CalcBufferNeeded();

	virtual void InitializeOutput(){}

protected:

	virtual AkModulatorParams* GetParamsPtr() 					{ return &m_params; }

	AkLFOParams m_params;
	bool m_bDspParamsDirty;
};

//
//
class CAkEnvelopeCtx: public CAkModulatorCtx
{
public:
	CAkEnvelopeCtx(): CAkModulatorCtx() {}
	virtual ~CAkEnvelopeCtx(){};

	void FillParams( AkEnvelopeParams* out_pParams )	
	{ 
		out_pParams->m_fStartValue = m_params.m_fStartValue;
		AKASSERT(out_pParams->m_fStartValue >=0 && out_pParams->m_fStartValue <= 1.0f);
		
		out_pParams->m_uAttack = m_params.m_uAttack;
		out_pParams->m_fCurve = m_params.m_fCurve;
		out_pParams->m_fSustain = m_params.m_fSustain;
		out_pParams->m_uDecay = m_params.m_uDecay;
		out_pParams->m_uRelease = m_params.m_uRelease;
		
		CAkModulatorCtx::FillParams(out_pParams);
	}

	virtual bool SetParam(AkRTPC_ModulatorParamID in_paramID, AkReal32 in_fValue);

	virtual void CalcBufferNeeded();

	virtual void InitializeOutput();

protected:
	virtual AkModulatorParams* GetParamsPtr()					{ return &m_params; }

	AkEnvelopeParams m_params;
};

class CAkTimeModCtx : public CAkModulatorCtx
{
public:
	CAkTimeModCtx() : CAkModulatorCtx(), m_bElapsedDirty(false) {}
	virtual ~CAkTimeModCtx(){};

	void FillParams(AkTimeModParams* out_pParams)
	{
		out_pParams->m_uDuration = m_params.m_uDuration;
		out_pParams->m_uLoopsDuration = m_params.m_uLoopsDuration;
		out_pParams->m_fPlaybackSpeed = m_params.m_fPlaybackSpeed;

		CAkModulatorCtx::FillParams(out_pParams);
	}

	virtual bool SetParam(AkRTPC_ModulatorParamID in_paramID, AkReal32 in_fValue);

	virtual void CalcBufferNeeded();

	virtual void InitializeOutput() {}

	virtual void Tick(AkUInt32 in_uFrames)
	{
		AkUInt32 frames = 0;
		if (m_params.m_iInitialDelay > 0)
			m_params.m_iInitialDelay -= in_uFrames;
		if (m_params.m_iInitialDelay <= 0)
		{
			frames = AkMath::RoundU(m_params.m_fPlaybackSpeed * (AkReal32)in_uFrames);
			if (!m_bElapsedDirty)
			{
				frames = 0;
				m_bElapsedDirty = true;
			}
		}
		CAkModulatorCtx::Tick(frames);
	}

protected:
	virtual AkModulatorParams* GetParamsPtr()					{ return &m_params; }

	AkTimeModParams m_params;
	bool m_bElapsedDirty;
};


#endif
