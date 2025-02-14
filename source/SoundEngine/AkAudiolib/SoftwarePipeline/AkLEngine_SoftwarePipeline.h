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
// AkLEngine_SoftWarePipeline.h
//
// Implementation of the Lower Audio Engine.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_LENGINE_SOFTWARE_PIPELINE_H_
#define _AK_LENGINE_SOFTWARE_PIPELINE_H_

#include "AkLEngine.h"
#include "AkVPL.h"
#include <float.h>

//-----------------------------------------------------------------------------
// Structs.
//-----------------------------------------------------------------------------

class CAkVPLSrcCbxNode;
class IAkRTPCSubscriberPlugin;

class AkHdrBus : public AkVPL
{
public:
	AkHdrBus(CAkBusCtx in_BusCtx);
	
	void ComputeHdrAttenuation();
	void SaveMaxHdrWindowTop(AkReal32 in_fWindowTop, CAkBus* in_pBus, AkGameObjectID in_BusObjectID);

	static void SendMaxHdrWindowTop();
	static void TermHdrWinTopMap();

	// Get computed global window top, in absolute (from the point of view of the top level node) dB SPL. 
	inline AkReal32 GetHdrWindowTop() { return m_fHdrWinTop; }

	// Voices present their effective SPL value to the HDR bus via this function. Once all the voices have done so,
	// the bus may compute the window top. The volume value is abolute (contains bus gains following this bus all the way down to the master out).
	inline void PushEffectiveVoiceVolume( AkReal32 in_fEffectiveVoiceVolume )
	{
		// Transform volume to scope of this bus.
		if ( in_fEffectiveVoiceVolume > m_fHdrMaxVoiceVolume )
			m_fHdrMaxVoiceVolume = in_fEffectiveVoiceVolume;
	}

	inline AkReal32 GetMaxVoiceWindowTop( AkReal32 in_fPeakVolume )
	{
		AkReal32 fOffset = DownstreamGainDB() + m_fThreshold;

		AkReal32 fPeakVolumeAboveThreshold = in_fPeakVolume - fOffset;
		AkReal32 fMaxVoiceWindowTop = fOffset;
		if ( fPeakVolumeAboveThreshold > 0 )
			fMaxVoiceWindowTop += ( m_fGainFactor * fPeakVolumeAboveThreshold );

		return fMaxVoiceWindowTop;
	}

protected:
	
	static void NotifyHdrWindowTop(AkReal32 in_fWindowTop, AkUniqueID in_GameParamID, AkGameObjectID in_BusObjectID);

	AkReal32				m_fHdrMaxVoiceVolume;	// Max voice volume. Target window top is based on this.
	AkReal32				m_fHdrWinTopState;		// Filtered HDR window top (linear value): used for game param only.
	AkReal32				m_fHdrWinTop;			// Actual window top level, in dB.
	AkReal32				m_fReleaseCoef;			// Release one-pole filter coefficient.
	AkReal32				m_fThreshold;			// Cached threshold value, in dB.
	AkReal32				m_fGainFactor;			// Gain factor above threshold (derived from ratio).

	struct MaxHdrWinTopIDs
	{
		bool operator<(const MaxHdrWinTopIDs& in_key) const
		{
			return this->gameParamID < in_key.gameParamID || (this->gameParamID == in_key.gameParamID && this->gameObjectID < in_key.gameObjectID);
		}
		bool operator>(const MaxHdrWinTopIDs& in_key) const
		{
			return this->gameParamID > in_key.gameParamID || (this->gameParamID == in_key.gameParamID && this->gameObjectID > in_key.gameObjectID);
		}

		bool operator==(const MaxHdrWinTopIDs& in_key) const
		{
			return this->gameParamID == in_key.gameParamID && this->gameObjectID == in_key.gameObjectID;
		}

		AkUniqueID gameParamID;
		AkGameObjectID gameObjectID;
	};

	struct MaxHdrWinTopInfo
	{
		MaxHdrWinTopInfo() : fMaxHdrWindowTop(-FLT_MAX){}
		~MaxHdrWinTopInfo(){}

		MaxHdrWinTopIDs key;
		AkReal32 fMaxHdrWindowTop;
	};

	/// Key policy for AkSortedKeyArray.
	struct AkGetMaxHdrWinTopInfo
	{
		/// Default policy.
		static AkForceInline MaxHdrWinTopIDs & Get(MaxHdrWinTopInfo & in_item)
		{
			return in_item.key;
		}
	};
	
	typedef AkSortedKeyArray<MaxHdrWinTopIDs, MaxHdrWinTopInfo, ArrayPoolLEngineDefault, AkGetMaxHdrWinTopInfo> MaxHdrWinTopMap;
	static MaxHdrWinTopMap m_mapMaxHdrWinTops;
};

#endif // _AK_LENGINE_SOFTWARE_PIPELINE_H_
