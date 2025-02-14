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
// AkActionPlay.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ACTION_PLAY_H_
#define _ACTION_PLAY_H_

#include "AkAction.h"

struct AkCntrHistArray;

class CAkActionPlay : public CAkAction
{
public:
	//Thread safe version of the constructor
	static CAkActionPlay* Create(AkActionType in_eActionType, AkUniqueID in_ulID = 0);

protected:
	CAkActionPlay(AkActionType in_eActionType, AkUniqueID in_ulID);
	virtual ~CAkActionPlay();

public:
	//Execute the action
	//Must be called only by the audiothread
	//
	//Return - AKRESULT - AK_Success if all succeeded
	virtual AKRESULT Execute(
		AkPendingAction * in_pAction
		);

	AkBankID GetBankID(){ return m_bankID; }

	void SetBankID(AkBankID in_bankID){ m_bankID = in_bankID; }

	virtual void GetHistArray( AkCntrHistArray& out_rHistArray );

	virtual AKRESULT SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

	bool WasLoadedFromBank(){ return m_bWasLoadedFromBank; }

private:

	AkBankID	m_bankID;			// Used for the PrepareEvent mechanism, it tells from which bank to gather the hierarchy data.
};
#endif
