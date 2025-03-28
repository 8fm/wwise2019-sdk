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
// AkMusicBank.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _MUSIC_BANK_H_
#define _MUSIC_BANK_H_

class CAkUsageSlot;
namespace AkBank
{
	struct AKBKSubHircSection;
}

namespace AkMusicBank
{
	AKRESULT LoadBankItem( const AkBank::AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot, AkUInt32 in_dwBankID );
	//Need to stop MIDI notes properly when a bank is unloaded
	void UnloadBankSlot(CAkUsageSlot* in_pUsageSlot);
}

#endif //_MUSIC_BANK_H_
