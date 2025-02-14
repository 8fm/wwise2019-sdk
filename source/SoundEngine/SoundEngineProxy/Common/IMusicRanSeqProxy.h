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

class IMusicRanSeqProxy : virtual public IMusicTransAwareProxy
{
	DECLARE_BASECLASS( IMusicTransAwareProxy );
public:

	virtual void SetPlayList( AkMusicRanSeqPlaylistItem* in_pArrayItems, AkUInt32 in_NumItems ) = 0;

	enum MethodIDs
	{
		MethodSetPlayList = __base::LastMethodID,

		LastMethodID
	};
};

namespace MusicRanSeqProxyCommandData
{
	struct SetPlayList : public ObjectProxyCommandData::CommandData
	{
		SetPlayList();
		~SetPlayList();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_NumItems;
		AkMusicRanSeqPlaylistItem* m_pArrayItems;
	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
