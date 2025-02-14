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

/// \file
/// Declaration of IProxyFrameworkConnected interface
/// \sa
/// - \ref initialization_comm
/// - \ref framerendering_comm
/// - \ref termination_comm

#include "IProxyFramework.h"
#include "ICommandChannelHandler.h"
#include "ICommunicationCentralNotifyHandler.h"

namespace AK
{
	namespace Comm
	{
		/// Connected proxy framework
		/// \sa
		/// - \ref initialization_comm
		/// - \ref framerendering_comm
		/// - \ref termination_comm
		class IProxyFrameworkConnected : public IProxyFramework
										, public ICommandChannelHandler
										, public ICommunicationCentralNotifyHandler
		{
		public:
			static IProxyFrameworkConnected * Create();

		protected:
			/// Virtual destructor on interface to avoid warnings.
			virtual ~IProxyFrameworkConnected(){}
		};
	}
}

#endif // #ifndef AK_OPTIMIZED
