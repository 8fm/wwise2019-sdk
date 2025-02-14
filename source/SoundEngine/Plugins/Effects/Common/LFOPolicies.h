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

#ifndef _AKLFO_POLICIES_H_
#define _AKLFO_POLICIES_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkFPUtilities.h>
#include "LFO.h"


// ---------------------------------------------------------------------
// LFO Polarity policies. 
// Waveform synthesis methods.
// Usage: Instantiate an LFO<> with either one of these polarity policies,
//			whether you want an unipolar or a bipolar LFO.
// ---------------------------------------------------------------------


// Efficiently sets __in__ to its remaining. 
// This only works if __in__ is always [0, 2[.
#define AK_LFO_REM( __in__ ) \
	AK_FPSetValGTE( __in__, 1.f, __in__, ( __in__ - 1.f ) )

namespace DSP
{

	static AkForceInline AkReal32 QuickSin(const AkReal32 x)
	{
		const AkReal32 x2 = x * x;
		return x * ( 0.999996615908002f + x2 * ( -1.66648283818951e-1f + x2 * ( 8.30632522715989e-3f + x2 * -1.83636539769468e-4f ) ) );
	}

	// ---------------------------------------------------------------------
	// Unipolar policy. 
	// Instantiating an LFO decorated (at compile-time) with this class 
	// will change its behavior to generate unipolar waveforms;
	// ---------------------------------------------------------------------
	class Unipolar
	{
	public:

		static AkForceInline AkReal32 ScaleSin(const AkReal32 x) { return (x + 1.f)*0.5f;}

		static AkForceInline AkReal32 ScaleTriangle(const AkReal32 x) {return 2.f * x;}

		static AkForceInline AkReal32 SquareAmp(const AkReal32) {return 0.f;}

		static AkForceInline AkReal32 ScaleSawUp(const AkReal32 x) { return x; }

		static AkForceInline AkReal32 ScaleSawDown(const AkReal32 x) { return 1.f - x; }
	};


	// ---------------------------------------------------------------------
	// Bipolar policy. 
	// Instantiating an LFO decorated (at compile-time) with this class 
	// will change its behavior to generate bipolar waveforms;
	// ---------------------------------------------------------------------
	class Bipolar
	{
	public:

		static AkForceInline AkReal32 ScaleSin(const AkReal32 x) { return x;}

		static AkForceInline AkReal32 ScaleTriangle(const AkReal32 x) {return (4.f * x - 1.f);}

		static AkForceInline AkReal32 SquareAmp(const AkReal32 x) {return -x;}

		static AkForceInline AkReal32 ScaleSawUp(const AkReal32 x) { return 2.f * x - 1.f; }

		static AkForceInline AkReal32 ScaleSawDown(const AkReal32 x) { return 1.f - 2.f * x; }
	};
}

#endif // _AKLFO_POLICIES_H_

