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

#include "IMusicTransAwareProxy.h"

class IMusicSwitchProxy : 
	virtual public IMusicTransAwareProxy
{
	DECLARE_BASECLASS( IMusicTransAwareProxy );
public:

	virtual void ContinuePlayback( bool in_bContinuePlayback ) = 0;

	enum MethodIDs
	{
		MethodContinuePlayback = __base::LastMethodID,
	
		LastMethodID
	};
};

namespace MusicSwitchProxyCommandData
{
	struct ContinuePlayback : public ObjectProxyCommandData::CommandData
	{
		ContinuePlayback();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		bool m_bContinuePlayback;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
