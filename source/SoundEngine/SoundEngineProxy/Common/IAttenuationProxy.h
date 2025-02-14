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
#include "AkAttenuationMgr.h"

class IAttenuationProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
    virtual void SetAttenuationParams( AkWwiseAttenuation& in_rParams ) = 0;

	enum MethodIDs
	{
		MethodSetAttenuationParams = __base::LastMethodID,

		LastMethodID
	};
};

namespace AttenuationProxyCommandData
{
	struct SetAttenuationParams : public ObjectProxyCommandData::CommandData
	{
		SetAttenuationParams();
		~SetAttenuationParams();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		bool LoadRTPCArray( CommandDataSerializer &in_rSerializer, AkUInt32 &out_rNumRTPC, AkWwiseRTPCreg *& out_pRTPCArray );
		bool LoadCurveArray( CommandDataSerializer &in_rSerializer, AkUInt32 &out_uNumCurves, AkWwiseGraphCurve*& out_pCurves );
		
		AkWwiseAttenuation m_Params;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
