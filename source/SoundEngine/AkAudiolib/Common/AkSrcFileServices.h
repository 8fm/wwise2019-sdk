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

//////////////////////////////////////////////////////////////////////
//
// AkSrcFileServices.h
//
// Services shared by streamed sources.
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_SRC_FILE_SERVICES_H_
#define _AK_SRC_FILE_SERVICES_H_

#include "AkCommon.h"

namespace AK
{
    namespace SrcFileServices
	{
		// Returns:
		// - AK_DataReady: Prebuffering is ready.
		// - AK_NoDataReady: Prebuffering is not ready.
		// - AK_Fail: Fatal IO error.
		inline AKRESULT IsPrebufferingReady( AK::IAkAutoStream * in_pStream, AkUInt32 in_uSizeLeft )
		{
			AkUInt32 uBuffering;
			AKRESULT eBufferingResult = in_pStream->QueryBufferingStatus( uBuffering );

			if ( eBufferingResult == AK_DataReady
				|| eBufferingResult == AK_NoDataReady )
			{
				if ( uBuffering + in_uSizeLeft >= in_pStream->GetNominalBuffering() )
				{
					// Returned "data ready" with buffering _above_ threshold.
					return AK_DataReady;
				}
				else
				{
					// Returned "data ready" with buffering _below_ threshold.
					return AK_NoDataReady;
				}
			}
			else if ( eBufferingResult == AK_NoMoreData )
			{
				// Returned "no more data". Prebuffering is ready.
				return AK_DataReady;
			}
			else 
			{
				// All other cases.
				return eBufferingResult;
			}
		}

		inline void GetBuffering( AK::IAkAutoStream * in_pStream, AkUInt32 in_uSizeLeft, AkBufferingInformation & out_bufferingInfo )
		{
			AkUInt32 uBuffering;
			AKRESULT eBufferingResult = in_pStream->QueryBufferingStatus( uBuffering );
			out_bufferingInfo.uBuffering = 0;
			if ( eBufferingResult != AK_Fail )
			{
				AkAutoStmHeuristics heuristics;
				in_pStream->GetHeuristics( heuristics );
				out_bufferingInfo.uBuffering = (AkUInt32)( ( uBuffering + in_uSizeLeft ) / heuristics.fThroughput );
				if ( eBufferingResult == AK_NoMoreData 
					|| ( ( uBuffering + in_uSizeLeft ) >= in_pStream->GetNominalBuffering() ) )
				{
					out_bufferingInfo.eBufferingState = AK_NoMoreData;
				}
				else
				{
					out_bufferingInfo.eBufferingState = AK_Success;
				}
			}
			else
			{
				out_bufferingInfo.eBufferingState = AK_Fail;
			}
		}
	}
}
#endif //_AK_SRC_FILE_SERVICES_H_
