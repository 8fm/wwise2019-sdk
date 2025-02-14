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
// PrivateStructures.h
//
// AkAudioLib Internal structures definitions
//
//////////////////////////////////////////////////////////////////////
#ifndef _PRIVATE_STRUCTURES_H_
#define _PRIVATE_STRUCTURES_H_

#include "AkModulatorPBIData.h"
#include "AkContinuationList.h"
#include "AkSettings.h"
#include "AkMath.h"

class AkExternalSourceArray
{
public:

	static AkExternalSourceArray* Create(AkUInt32 in_nCount, AkExternalSourceInfo* in_pSrcs);

	void AddRef(){m_cRefCount++;}
	void Release();
	AkUInt32 Count(){return m_nCount;}
	const AkExternalSourceInfo* Sources(){return m_pSrcs;}

private:
	AkUInt32 m_cRefCount;
	AkUInt32 m_nCount;
	AkExternalSourceInfo m_pSrcs[1];	//Variable size array.
};

class UserParams
{
public:
	UserParams()
	{
		m_PlayingID = 0;
		m_CustomParam.customParam = 0;
		m_CustomParam.ui32Reserved = 0;
		m_CustomParam.pExternalSrcs = NULL;
	}

	UserParams(const UserParams& in_rCopy)
	{
		m_CustomParam.pExternalSrcs = NULL;
		*this = in_rCopy;
	}

	~UserParams()
	{
		if (m_CustomParam.pExternalSrcs)
			m_CustomParam.pExternalSrcs->Release();
	}

	UserParams& operator=(const UserParams& in_rCopy)
	{
		Init(in_rCopy.m_PlayingID, in_rCopy.m_CustomParam);
		return *this;
	}

	void Init(AkPlayingID in_playingID, const AkCustomParamType& in_params)
	{
		m_PlayingID = in_playingID;
		m_CustomParam.customParam = in_params.customParam;
		m_CustomParam.ui32Reserved = in_params.ui32Reserved;
		
		SetExternalSources(in_params.pExternalSrcs);
	}

	void SetExternalSources(AkExternalSourceArray* in_pSources)
	{
		if (m_CustomParam.pExternalSrcs)
			m_CustomParam.pExternalSrcs->Release();
		
		if (in_pSources)
			in_pSources->AddRef();

		m_CustomParam.pExternalSrcs = in_pSources;
	}

	void SetPlayingID(AkPlayingID in_playingID){ m_PlayingID = in_playingID; }

	const AkCustomParamType& CustomParam() const {return m_CustomParam;}
	AkPlayingID				PlayingID() const {return m_PlayingID;}

private:
	AkCustomParamType m_CustomParam;
	AkPlayingID	m_PlayingID;
};

//----------------------------------------------------------------------------------------------------
// parameters needed for play and play&continue actions
//----------------------------------------------------------------------------------------------------
class CAkTransition;
class CAkTransition;
class CAkPath;

// Synchronisation type. Applies to Source transitions as well as to State changes.
enum AkSyncType
{
    SyncTypeImmediate		= 0,
    SyncTypeNextGrid		= 1,
    SyncTypeNextBar			= 2,
    SyncTypeNextBeat		= 3,
    SyncTypeNextMarker		= 4,
    SyncTypeNextUserMarker	= 5,
    SyncTypeEntryMarker		= 6,    // not used with SrcTransRules.
	SyncTypeExitMarker		= 7,
	SyncTypeExitNever		= 8,
	SyncTypeLastExitPosition= 9

#define NUM_BITS_SYNC_TYPE  (5)
};

enum AkPlaybackState
{
	PB_Playing,
		PB_Paused
};

enum ActionParamType
{
	ActionParamType_Stop		= 0,
	ActionParamType_Pause		= 1,
	ActionParamType_Resume		= 2,
	ActionParamType_Break		= 3, // == PlayToEnd
	ActionParamType_Seek		= 4,
	ActionParamType_Release	= 5
};

enum AkListenerOp
{
	AkListenerOp_Set,
	AkListenerOp_Add,
	AkListenerOp_Remove,
};

struct AkPathInfo
{
	CAkPath*	pPBPath;
	AkUniqueID	PathOwnerID;
};

struct ContParams
{
	// Default constructor
	ContParams()
		: pPlayStopTransition( NULL ) 
		, pPauseResumeTransition( NULL )
		, pPathInfo(NULL )
		, ulPauseCount( 0 )
	{}

	ContParams(
		CAkTransition*	in_pPlayStopTransition,
		CAkTransition*	in_pPauseResumeTransition,
		AkPathInfo*		in_pPathInfo,
		AkUInt32		in_ulPauseCount )
		: pPlayStopTransition(in_pPlayStopTransition)
		, pPauseResumeTransition(in_pPauseResumeTransition)
		, pPathInfo(in_pPathInfo)
		, ulPauseCount(in_ulPauseCount)
	{}

	// Copy constructor, copy everything BUT THE spContList
	ContParams( ContParams* in_pFrom )
		: pPlayStopTransition( in_pFrom->pPlayStopTransition ) 
		, pPauseResumeTransition( in_pFrom->pPauseResumeTransition )
		, pPathInfo( in_pFrom->pPathInfo )
		, ulPauseCount( in_pFrom->ulPauseCount )
	{
		// DO NOT COPY spContList object.
	}

	CAkTransition*						pPlayStopTransition;		// the running play / stop transition
	CAkTransition*						pPauseResumeTransition;		// the running pause / resume transition
	AkPathInfo*							pPathInfo;					// the current path if any
	CAkSmartPtr<CAkContinuationList>	spContList;
	AkUInt32							ulPauseCount;
	CAkModCtxRefContainer				triggeredModulators;		// "Note/Event -scoped" Modulators that have been triggered by the first voice in a continuous sequence. Keep a ref to keep alive.
};

struct TransParamsBase // because we cannot have a structure with a constructor in a union, TransParams is the form with the constructor.
{
	AkTimeMs				TransitionTime;				// how long this should take
	AkCurveInterpolation	eFadeCurve;					// what shape it should have
	bool					bBypassInternalValueInterpolation;
};

struct TransParams : public TransParamsBase
{
	TransParams()
	{
		TransitionTime = 0;
		eFadeCurve = AkCurveInterpolation_Linear;
		bBypassInternalValueInterpolation = false;
	}

	TransParams( AkTimeMs in_TransitionTime, AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear, bool in_bBypassInternalValueInterpolation = false )
	{
		TransitionTime = in_TransitionTime;
		eFadeCurve = in_eFadeCurve;
		bBypassInternalValueInterpolation = in_bBypassInternalValueInterpolation;
	}
};

struct PlaybackTransition
{
	CAkTransition*		pvPSTrans;	// Play Stop transition reference.
	CAkTransition*		pvPRTrans;	// Pause Resume transition reference.
	PlaybackTransition()
		:pvPSTrans( NULL )
		,pvPRTrans( NULL )
	{}
};

struct AkMusicGrid
{
	AkMusicGrid() 
		: fTempo( 0.0 )
		, uBeatDuration( 0 )
		, uBarDuration( 0 )
		, uGridDuration( 0 )
		, uGridOffset( 0 ) {}
	AkReal32	fTempo;
    AkUInt32    uBeatDuration;      // Beat duration in samples.
    AkUInt32    uBarDuration;       // Bar duration in samples.
    AkUInt32    uGridDuration;      // Grid duration in samples.
    AkUInt32    uGridOffset;        // Grid offset in samples.
};

// Time conversion helpers.
// ---------------------------------------------------------
namespace AkTimeConv
{
	static inline AkInt32 MillisecondsToSamples( 
        AkReal64 in_milliseconds 
        )
    {
		return AkMath::Round64( in_milliseconds * (AkReal64)AK_CORE_SAMPLERATE / 1000.0 );
    }
	static inline AkInt32 MillisecondsToSamples( 
        AkTimeMs in_milliseconds 
        )
    {
		return (AkInt32)( (AkInt64)in_milliseconds * AK_CORE_SAMPLERATE / 1000 );
    }
	static inline AkInt32 MillisecondsToSamplesRoundedUp( 
        AkReal64 in_milliseconds 
        )
    {
		return (AkInt32)ceil( in_milliseconds * (AkReal64)AK_CORE_SAMPLERATE / 1000.0 );
    }
    static inline AkInt32 SecondsToSamples( 
        AkReal64 in_seconds 
        )
    {
        return AkMath::Round64( in_seconds * (AkReal64)AK_CORE_SAMPLERATE );
    }
	static inline AkInt32 SecondsToSamplesRoundedUp(
		AkReal64 in_seconds
	)
	{
		return (AkInt32)ceil(in_seconds * AK_CORE_SAMPLERATE);
	}
    static inline AkTimeMs SamplesToMilliseconds( 
        AkInt32 in_samples
        )
    {
		return (AkTimeMs)AkMath::Round64( 1000 * (AkReal64)in_samples / (AkReal64)AK_CORE_SAMPLERATE );
    }
	static inline AkReal32 SamplesToMsReal( // DO NOT USE for long time intervals!
		AkInt32 in_samples
		)
	{
		return ((AkReal32)in_samples) / ((AkReal32)AK_CORE_SAMPLERATE / (AkReal32)1000.0);
	}
	static inline AkReal64 SamplesToMsDbl( // DO NOT USE for long time intervals!
		AkInt32 in_samples
		)
	{
		return ((AkReal64)in_samples) / ((AkReal64)AK_CORE_SAMPLERATE / (AkReal64)1000.0);
	}
	static inline AkReal64 SamplesToSeconds( 
        AkInt32 in_samples
        )
    {
        return (AkReal64)in_samples / (AkReal64)AK_CORE_SAMPLERATE;
    }

	static inline AkInt32 ToShortRange( 
		AkInt64 in_uNumSamples 
		)
	{
		AKASSERT( in_uNumSamples >= AK_INT_MIN && in_uNumSamples < AK_INT_MAX );
		return ((AkInt32)in_uNumSamples);
	}
}

#endif
