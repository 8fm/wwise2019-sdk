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

#include "IMusicNodeProxy.h"

class ISegmentProxy : virtual public IMusicNodeProxy
{
	DECLARE_BASECLASS( IMusicNodeProxy );
public:

	virtual void Duration(
        AkReal64 in_fDuration               // Duration in milliseconds.
        ) = 0;

	virtual void StartPos(
        AkReal64 in_fStartPos               // PlaybackStartPosition.
        ) = 0;

	virtual void SetMarkers(
		AkMusicMarkerWwise*     in_pArrayMarkers, 
		AkUInt32                in_ulNumMarkers
		) = 0;

	enum MethodIDs
	{
		MethodDuration = __base::LastMethodID,
		MethodStartPos,
		MethodAddMarker,
		MethodSetMarkers,

		LastMethodID
	};
};

namespace MusicSegmentProxyCommandData
{
	struct Duration : public ObjectProxyCommandData::CommandData
	{
		Duration();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkReal64 m_fDuration;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct StartPos : public ObjectProxyCommandData::CommandData
	{
		StartPos();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkReal64 m_fStartPos;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetMarkers : public ObjectProxyCommandData::CommandData
	{
		SetMarkers();
		~SetMarkers();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkMusicMarkerWwise*     m_pArrayMarkers;
		AkUInt32                m_ulNumMarkers;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
