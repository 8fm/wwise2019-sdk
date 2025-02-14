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
// AkDynamicSequencePBI.h
//
//////////////////////////////////////////////////////////////////////
#ifndef AKDYNAMICSEQUENCEPBI_H
#define AKDYNAMICSEQUENCEPBI_H

#include "AkContinuousPBI.h"
#include <AK/SoundEngine/Common/AkDynamicSequence.h> // for DynamicSequenceType in the SDK

class CAkDynamicSequencePBI : public CAkContinuousPBI
{
public:
	CAkDynamicSequencePBI(
		AkPBIParams&				in_params,
		CAkSoundBase*				in_pSound,			// Sound associated to the PBI (NULL if none).	
		CAkSource*					in_pSource,
		const PriorityInfoCurrent&	in_rPriority,
		AK::SoundEngine::DynamicSequence::DynamicSequenceType in_eDynamicSequenceType
		 );

	virtual ~CAkDynamicSequencePBI();

	virtual void Term( bool in_bFailedToInit );

	// Get the information about the next sound to play in a continuous system
	virtual void PrepareNextToPlay( bool in_bIsPreliminary );

private:

	bool IsPlaybackCompleted();

	AKRESULT PlayNextElement( AkUniqueID in_nextElementID, AkTimeMs in_delay );

	bool m_bRequestNextFromDynSeq;
	AK::SoundEngine::DynamicSequence::DynamicSequenceType m_eDynamicSequenceType;

	AkUniqueID m_startingNode;
	void* m_pDynSecCustomInfo;
	/*
	AkUniqueID LastNodeIDSelected() const { return m_queuedItem.audioNodeID; }
	void* LastCustomInfo() const { return m_queuedItem.pCustomInfo; }
	*/
};

#endif // AKDYNAMICSEQUENCEPBI_H
