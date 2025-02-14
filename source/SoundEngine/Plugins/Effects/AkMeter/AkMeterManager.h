
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

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkLock.h>

#include "AkMeterFX.h"

class CAkMeterManager
{
public:
	CAkMeterManager( AK::IAkPluginMemAlloc * in_pAllocator, AK::IAkGlobalPluginContext * in_pCtx);
	~CAkMeterManager();

	void Register(CAkMeterFX * in_pFX);
	void Unregister(CAkMeterFX * in_pFX);

	static void BehavioralExtension( 
		AK::IAkGlobalPluginContext * in_pContext,	///< Engine context.
		AkGlobalCallbackLocation in_eLocation,		///< Location where this callback is fired.
		void * in_pCookie							///< User cookie passed to AK::SoundEngine::RegisterGlobalCallback().
	);

	static CAkMeterManager * Instance(AK::IAkPluginMemAlloc * in_pAllocator, AK::IAkGlobalPluginContext * in_pCtx);

private:
	typedef AkListBare<CAkMeterFX, AkListBareNextItem, AkCountPolicyWithCount,AkLastPolicyWithLast> Meters;

	void Execute();

	AK::IAkPluginMemAlloc * m_pAllocator;
	AK::IAkGlobalPluginContext *m_pGlobalContext;

	Meters m_meters;
	CAkLock	m_RegistrationLock;

	static CAkMeterManager * pInstance;
};
