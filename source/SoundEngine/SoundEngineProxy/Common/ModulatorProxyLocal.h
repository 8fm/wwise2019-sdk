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

#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

#include "ObjectProxyLocal.h"
#include "IModulatorProxy.h"
#include "AkModulatorMgr.h"
#include "AkModulator.h"

class ModulatorProxyLocal : public ObjectProxyLocal
						, virtual public IModulatorProxy
{
public:
	ModulatorProxyLocal( IModulatorProxy::ProxyType in_proxyType, AkUniqueID in_id );
	virtual ~ModulatorProxyLocal();

	virtual void SetAkProp( AkModulatorPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax );
	virtual void SetAkProp( AkModulatorPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax );
	virtual void SetRTPC( AkRtpcID in_RtpcID, AkRtpcType in_RtpcType, AkRtpcAccum in_RtpcAccum, AkRTPC_ParameterID in_ParamID, AkUniqueID in_RtpcCurveID, AkCurveScaling in_eScaling, AkRTPCGraphPoint* in_paPoints = NULL, AkUInt32 in_uNumPoints = 0 );
	virtual void UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RtpcCurveID );
};

#endif
#endif // #ifndef AK_OPTIMIZED
