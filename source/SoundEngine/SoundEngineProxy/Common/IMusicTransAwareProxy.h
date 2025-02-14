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

#include "AkPrivateTypes.h"
#include "IMusicNodeProxy.h"

#include "AkMusicStructs.h"

class IMusicTransAwareProxy : virtual public IMusicNodeProxy
{
	DECLARE_BASECLASS( IMusicNodeProxy );
public:

	virtual void SetRules(
		AkUInt32 in_NumRules,
		AkWwiseMusicTransitionRule* in_pRules
		) = 0;

	//
	// Method IDs
	//

	enum MethodIDs
	{
        MethodSetRules = __base::LastMethodID,

		LastMethodID
	};

};

namespace MusicTransAwareProxyCommandData
{
	struct SetRules : public ObjectProxyCommandData::CommandData
	{
		SetRules();
		~SetRules();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_NumRules;
		AkWwiseMusicTransitionRule* m_pRules;
	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
