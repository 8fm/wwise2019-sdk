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

#ifndef _INTERNALLPFSTATE_H_
#define _INTERNALLPFSTATE_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include "BiquadFilter.h"
#include "AkSettings.h"

#define AKLPFNUMMEMORIES			(4)
#define BYPASSMAXVAL 0.1f			// Filter will be bypassed for values lower than this

struct AkBQFParams
{
	// Delay switching to bypass mode by this many update frames. 
	static const AkInt8 kFramesToBypass = 4;

	AkForceInline AkBQFParams()
	{
		Reset();
	}

	AkForceInline void Reset()
	{
		fCurrentPar = 0.f;
		fTargetPar = 0.f;
		uNumInterBlocks = NUMBLOCKTOREACHTARGET;
		iNextBypass = 0;
		bTargetDirty = true;
		bFirstSet = true;
		bBypass = true;
		bDcRemoved = true;
	}

	AkReal32    fCurrentPar;
	AkReal32	fTargetPar;

	AkUInt16	uNumInterBlocks;

	AkInt8		iNextBypass;

	bool        bTargetDirty;
	bool		bFirstSet;

	bool IsInterpolating()
	{
		return (uNumInterBlocks < NUMBLOCKTOREACHTARGET);
	}

	bool IsBypassed(){ return bBypass; }

	void SetBypassed(bool in_bIsBypassed)
	{
		if (!in_bIsBypassed)
			iNextBypass = 0;

		if (in_bIsBypassed != bBypass)
			bDcRemoved = false;

		bBypass = in_bIsBypassed;
	}

	bool IsDcRemoved() { return bDcRemoved; }
	void SetDcRemoved() { bDcRemoved = true; }

private:
	bool		bBypass;
	bool		bDcRemoved; // used to remove "DC offset" when transitioning from non-bypass to bypass
};

struct AkInternalBQFState
{
	DSP::BiquadFilterMultiSIMD m_LPF;
	DSP::BiquadFilterMultiSIMD m_HPF;
	
	AkBQFParams m_LPFParams;
	AkBQFParams m_HPFParams;

	AkChannelConfig	channelConfig;
} AK_ALIGN_DMA;

#endif // _INTERNALLPFSTATE_H_
