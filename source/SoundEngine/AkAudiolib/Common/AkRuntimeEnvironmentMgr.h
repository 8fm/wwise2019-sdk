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
#include <AK/SoundEngine/Common/IAkProcessorFeatures.h>

namespace AK
{
	struct AkProcessorSupportInfo
	{
		AkUInt32 uSIMDProcessorSupport;
		AkUInt32 uL2CacheSize;
		AkUInt32 uCacheLineSize;

		AkProcessorSupportInfo() 
		{
			uSIMDProcessorSupport = 0;
			uL2CacheSize = 0;
			uCacheLineSize = 0;
		}
	};
	
	class AkRuntimeEnvironmentMgr : public IAkProcessorFeatures
	{
	public:
		AKSOUNDENGINE_API static AkRuntimeEnvironmentMgr * Instance();
		virtual ~AkRuntimeEnvironmentMgr(){};

		// Initializes (or re-initializes) the CPUID detection
		void Initialize();

		// Provide a bitfield of flags to explicitly disable runtime detection for
		void DisableSIMDSupport(AkSIMDProcessorSupport in_eSIMD);

		virtual bool GetSIMDSupport(AkSIMDProcessorSupport in_eSIMD);
		virtual AkUInt32 GetCacheSize();
		virtual AkUInt32 GetCacheLineSize();

	protected:
		AkRuntimeEnvironmentMgr();	
		
		AkProcessorSupportInfo ProcessorInfo;	
	};
}
