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
// AkActionResume.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ACTION_RESUME_H_
#define _ACTION_RESUME_H_

#include "AkActionActive.h"

class CAkParameterNodeBase;

class CAkActionResume : public CAkActionActive
{
public:
	//Thread safe version of the constructor
	static CAkActionResume* Create(AkActionType in_eActionType, AkUniqueID in_ulID = 0);

	//Execute the Action
	//Must be called only by the audiothread
	//
	// Return - AKRESULT - AK_Success if all succeeded
	virtual AKRESULT Execute(
		AkPendingAction * in_pAction
		);

	bool IsMasterResume(){ return m_bIsMasterResume != 0; }
	void IsMasterResume( bool in_bIsMasterResume ){ m_bIsMasterResume = in_bIsMasterResume ? 1 : 0; }

	bool ApplyToStateTransitions() const { return m_bApplyToStateTransitions != 0; }
	void ApplyToStateTransitions( bool in_bApplyToStateTransitions ) { m_bApplyToStateTransitions = in_bApplyToStateTransitions ? 1 : 0; }

	bool ApplyToDynamicSequence() const { return m_bApplyToDynamicSequence != 0; }
	void ApplyToDynamicSequence( bool in_bApplyToDynamicSequence ) { m_bApplyToDynamicSequence = in_bApplyToDynamicSequence ? 1 : 0; }

	virtual AKRESULT SetActionSpecificParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

	virtual void SetActionActiveParams( ActionParams& io_params ) const;

protected:
	CAkActionResume(AkActionType in_eActionType, AkUniqueID in_ulID);
	virtual ~CAkActionResume();

	AkUInt8 m_bIsMasterResume:1;
	AkUInt8 m_bApplyToStateTransitions:1;
	AkUInt8 m_bApplyToDynamicSequence:1;
};
#endif
