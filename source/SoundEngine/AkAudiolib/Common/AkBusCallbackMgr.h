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
// AkBusCallbackMgr.h
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <AK/Tools/Common/AkLock.h>
#include <AK/SoundEngine/Common/AkCallback.h>
#include <AK/Tools/Common/AkKeyArray.h>
#include "AkManualEvent.h"

class CAkBusCallbackMgr
{
public:

	CAkBusCallbackMgr();
	~CAkBusCallbackMgr();

public:

	AKRESULT SetVolumeCallback( AkUniqueID in_busID, AkBusCallbackFunc in_pfnCallback );
	AKRESULT SetMeteringCallback( AkUniqueID in_busID, AkBusMeteringCallbackFunc in_pfnCallback, AkMeteringFlags in_eMeteringFlags );

	// Safe way to actually do the callback
	// Same prototype than the callback itself, with the exception that it may actually not do the callback if the
	// event was cancelled
	bool DoVolumeCallback( AkUniqueID in_busID, AkSpeakerVolumeMatrixCallbackInfo& in_rCallbackInfo );
	bool DoMeteringCallback( AkUniqueID in_busID, AK::IAkMetering * in_pMetering, AkChannelConfig in_channelConfig );

	bool IsVolumeCallbackEnabled( AkUniqueID in_busID );
	AkMeteringFlags IsMeteringCallbackEnabled( AkUniqueID in_busID );

private:
	typedef CAkKeyArray<AkUniqueID, AkBusCallbackFunc> AkListCallbacks;
	AkListCallbacks m_ListCallbacks;

	struct MeterCallbackInfo
	{
		AkBusMeteringCallbackFunc	pCallback;
		AkMeteringFlags				eFlags;

		inline void Clear() { pCallback = NULL; eFlags = AK_NoMetering; }
	};
	typedef CAkKeyArray<AkUniqueID, MeterCallbackInfo> AkListMeteringCallbacks;
	AkListMeteringCallbacks m_ListMeteringCallbacks;

	CAkLock m_csLock;
};
