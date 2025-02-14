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

#include "IObjectProxy.h"
#include "AkRTPC.h"
#include "AkModulatorProps.h"

class IModulatorProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
	virtual void SetAkProp( AkModulatorPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax ) = 0;
	virtual void SetAkProp( AkModulatorPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax ) = 0;
	virtual void SetRTPC( AkRtpcID in_RtpcID, AkRtpcType in_RtpcType, AkRtpcAccum in_RtpcAccum, AkRTPC_ParameterID in_ParamID, AkUniqueID in_RtpcCurveID, AkCurveScaling in_eScaling, AkRTPCGraphPoint* in_pArrayConversion = NULL, AkUInt32 in_ulConversionArraySize = 0 ) = 0;
	virtual void UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RtpcCurveID ) = 0;

	enum ProxyType
	{
		ProxyType_Lfo = 0,
		ProxyType_Envelope,
		ProxyType_Time
	};

	enum MethodIDs
	{
		MethodSetRTPC = __base::LastMethodID,
		MethodUnsetRTPC,
		MethodSetAkPropF,
		MethodSetAkPropI,

		LastMethodID
	};
};

namespace ModulatorProxyCommandData
{
	struct SetRTPC : public ObjectProxyCommandData::CommandData, public ProxyCommandData::CurveData<AkRTPCGraphPoint>
	{
		SetRTPC();
		~SetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_rtpcID;
		AkRtpcType m_rtpcType;
		AkRtpcAccum m_rtpcAccum;
		AkRTPC_ParameterID m_paramID;
		AkUniqueID m_rtpcCurveID;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct UnsetRTPC : public ObjectProxyCommandData::CommandData
	{
		UnsetRTPC();
		~UnsetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRTPC_ParameterID m_paramID;
		AkUniqueID m_rtpcCurveID;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData4
		< 
		IModulatorProxy::MethodSetAkPropF,
		AkUInt32, // AkPropID
		AkReal32,
		AkReal32,
		AkReal32
		> SetAkPropF;

	typedef ObjectProxyCommandData::CommandData4
		< 
		IModulatorProxy::MethodSetAkPropI,
		AkUInt32, // AkPropID
		AkInt32,
		AkInt32,
		AkInt32
		> SetAkPropI;
}

#endif // #ifndef AK_OPTIMIZED
