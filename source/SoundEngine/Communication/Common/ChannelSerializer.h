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

#include "Serializer.h"

#ifndef AK_CHANNEL_SERIALIZER_WRITER
	#if defined(PROXYCENTRAL_CONNECTED)
		#include "ALBytesMem.h"
		#define AK_CHANNEL_SERIALIZER_WRITER AK::ALWriteBytesMem
	#else
		#include "AKBase/BytesMem.h"
		#define AK_CHANNEL_SERIALIZER_WRITER WriteBytesMem
	#endif
#endif

namespace AK
{
	typedef Serializer<AK_CHANNEL_SERIALIZER_WRITER, SwapPolicy> ChannelSerializer;
	typedef Deserializer<SwapPolicy> ChannelDeserializer;
}
