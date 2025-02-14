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


#include "stdafx.h"

#include "Serializer.h"
#include "AkEndianByteSwap.h"

#ifdef DEBUG_NEW
	#ifdef _DEBUG
		#define new DEBUG_NEW
	#endif
#endif

namespace AK
{
	AkInt16 SwapPolicy::Swap(const AkInt16& in_rValue) const
	{
		return m_bSwapEndian ? AK::EndianByteSwap::WordSwap(in_rValue) : in_rValue;
	}

	AkUInt16 SwapPolicy::Swap(const AkUInt16& in_rValue) const
	{
		return m_bSwapEndian ? AK::EndianByteSwap::WordSwap(in_rValue) : in_rValue;
	}

	AkInt32 SwapPolicy::Swap(const AkInt32& in_rValue) const
	{
		return m_bSwapEndian ? AK::EndianByteSwap::DWordSwap(in_rValue) : in_rValue;
	}

	AkUInt32 SwapPolicy::Swap(const AkUInt32& in_rValue) const
	{
		return m_bSwapEndian ? AK::EndianByteSwap::DWordSwap(in_rValue) : in_rValue;
	}

	AkInt64 SwapPolicy::Swap(const AkInt64& in_rValue) const
	{
		AkInt64 liSwapped;
		liSwapped = m_bSwapEndian ? AK::EndianByteSwap::Int64Swap(in_rValue) : in_rValue;
		return liSwapped;
	}

	AkUInt64 SwapPolicy::Swap(const AkUInt64& in_rValue) const
	{
		AkUInt64 uiSwapped;
		uiSwapped = m_bSwapEndian ? AK::EndianByteSwap::Int64Swap(in_rValue) : in_rValue;
		return uiSwapped;
	}

	AkReal32 SwapPolicy::Swap(const AkReal32& in_rValue) const
	{
		AkReal32 retVal = in_rValue;

		if (m_bSwapEndian)
			AK::EndianByteSwap::Swap((AkUInt8*)&in_rValue, 4, (AkUInt8*)&retVal);

		return retVal;
	}
}
