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

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkCommon.h"

class CAkBehavioralCtx;

class CAkSpatialAudioVoice
{
	CAkSpatialAudioVoice();
	AKRESULT Init(CAkBehavioralCtx* in_pOwner);
public:

	static CAkSpatialAudioVoice* Create(CAkBehavioralCtx* in_pOwner);
	
	~CAkSpatialAudioVoice() { Term(); }
	
	void Term();

	// Must be called when spatial audio params on the parameter node have changed, specifically, the return values of HasReflections() and HasDiffraction().
	void ParamsUpdated();

	void GetAuxSendsValues(AkAuxSendArray & io_arAuxSends, AkReal32 in_fVol, AkReal32 in_fLPF, AkReal32 in_fHPF) const;

	bool HasReflections() const;
	bool HasDiffraction() const;
	AkReal32 GetMaxDistance() const;
	AkUniqueID GetReflectionsAuxBus() const;

    CAkSpatialAudioVoice* pNextItem;
	
private:
    CAkBehavioralCtx* m_pOwner;
    
public:
	AkUniqueID m_trackingReflectionsAuxBus;

	bool m_bTrackingReflections;
	bool m_bTrackingDiffraction;
};