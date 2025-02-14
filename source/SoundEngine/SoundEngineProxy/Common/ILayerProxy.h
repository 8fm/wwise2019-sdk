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

class ILayerProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:

	//
	// RTPC
	//

	virtual void SetRTPC(
		AkRtpcID in_RTPC_ID,
		AkRtpcType in_RTPCType,
		AkRtpcAccum in_RTPCAccum,
		AkRTPC_ParameterID in_ParamID,
		AkUniqueID in_RTPCCurveID,
		AkCurveScaling in_eScaling,
		AkRTPCGraphPoint* in_pArrayConversion = NULL,
		AkUInt32 in_ulConversionArraySize = 0
	) = 0;

	virtual void UnsetRTPC(
		AkRTPC_ParameterID in_ParamID,
		AkUniqueID in_RTPCCurveID
	) = 0;

	//
	// Child Associations
	//

	virtual void SetChildAssoc(
		AkUniqueID in_ChildID,
		AkRTPCGraphPoint* in_pCrossfadingCurve,	// NULL if none
		AkUInt32 in_ulCrossfadingCurveSize		// 0 if none
	) = 0;

	virtual void UnsetChildAssoc(
		AkUniqueID in_ChildID 
	) = 0;

	virtual void SetCrossfadingRTPC(
		AkRtpcID in_rtpcID,
		AkRtpcType in_rtpcType
	) = 0;

	//
	// Method IDs
	//

	enum MethodIDs
	{
        MethodSetRTPC = __base::LastMethodID,
		MethodUnsetRTPC,
		MethodSetChildAssoc,
		MethodUnsetChildAssoc,
		MethodSetCrossfadingRTPC,

		LastMethodID
	};
};

namespace LayerProxyCommandData
{
	//
	// SetRTPC
	//

	struct SetRTPC : public ObjectProxyCommandData::CommandData, public ProxyCommandData::CurveData<AkRTPCGraphPoint>
	{
		SetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCID;
		AkRtpcType m_RTPCType;
		AkRtpcAccum m_RTPCAccum;
		AkRTPC_ParameterID m_paramID;
		AkUniqueID m_RTPCCurveID;

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	//
	// UnsetRTPC
	//

	struct UnsetRTPC : public ObjectProxyCommandData::CommandData
	{
		UnsetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRTPC_ParameterID m_paramID;
		AkUniqueID m_RTPCCurveID;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	//
	// SetChildAssoc
	//

	struct SetChildAssoc : public ObjectProxyCommandData::CommandData
	{
		SetChildAssoc();
		~SetChildAssoc();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_ChildID;
		AkRTPCGraphPoint* m_pCrossfadingCurve;
		AkUInt32 m_ulCrossfadingCurveSize;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	//
	// UnsetChildAssoc
	//

	struct UnsetChildAssoc : public ObjectProxyCommandData::CommandData
	{
		UnsetChildAssoc();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_ChildID;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	//
	// SetCrossfadingRTPC
	//

	struct SetCrossfadingRTPC : public ObjectProxyCommandData::CommandData
	{
		SetCrossfadingRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_rtpcID;
		AkRtpcType m_rtpcType;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
