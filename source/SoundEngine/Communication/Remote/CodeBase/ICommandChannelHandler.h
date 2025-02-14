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

/// \file
/// Declaration of ICommandChannelHandler interface
/// \sa
/// - \ref initialization_comm
/// - \ref framerendering_comm
/// - \ref termination_comm

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{
	namespace Comm
	{
		/// Command channel handler
		class ICommandChannelHandler
		{
		protected:
			/// Virtual destructor on interface to avoid warnings.
			virtual ~ICommandChannelHandler(){}

		public:

			virtual const AkUInt8* HandleExecute( const AkUInt8* in_pData, AkUInt32 & out_uReturnDataSize ) = 0;
		};
	}
}

