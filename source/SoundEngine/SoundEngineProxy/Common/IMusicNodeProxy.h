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

#include "IParameterNodeProxy.h"

#include "AkMusicStructs.h"

class IMusicNodeProxy : virtual public IParameterNodeProxy
{
	DECLARE_BASECLASS( IParameterNodeProxy );
public:

	virtual void MeterInfo(
		bool in_bIsOverrideParent,
        const AkMeterInfo& in_MeterInfo
        ) = 0;

	virtual void SetStingers( 
		CAkStinger* in_pStingers, 
		AkUInt32 in_NumStingers
		) = 0;

	virtual void SetOverride( 
		AkUInt8 in_eOverride, 
		AkUInt8 in_uValue
		) = 0;

	virtual void SetFlag( 
		AkUInt8 in_eFlag, 
		AkUInt8 in_uValue
		) = 0;

	//
	// Method IDs
	//

	enum MethodIDs
	{
        MethodMeterInfo = __base::LastMethodID,
		MethodSetStingers,
		MethodSetOverride,
		MethodSetFlag,

		LastMethodID
	};
};

namespace MusicNodeProxyCommandData
{
	struct MeterInfo : public ObjectProxyCommandData::CommandData
	{
		MeterInfo();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		bool m_bIsOverrideParent;
		AkMeterInfo m_MeterInfo;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetStingers : public ObjectProxyCommandData::CommandData
	{
		SetStingers();
		~SetStingers();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		CAkStinger*	m_pStingers;
		AkUInt32	m_NumStingers;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData2
	< 
		IMusicNodeProxy::MethodSetOverride,
		AkUInt8,
		AkUInt8
	> SetOverride;

	typedef ObjectProxyCommandData::CommandData2
		< 
		IMusicNodeProxy::MethodSetFlag,
		AkUInt8,
		AkUInt8
		> SetFlag;
}

#endif // #ifndef AK_OPTIMIZED
