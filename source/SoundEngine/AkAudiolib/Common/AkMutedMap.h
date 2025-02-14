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
// AkMutedMap.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _MUTED_MAP_H_
#define _MUTED_MAP_H_

#include <AK/Tools/Common/AkKeyArray.h>
#include "AkDeltaMonitor.h"

struct AkMutedMapItem
{
	void* m_Identifier;
    AkUInt32 m_bIsGlobal        :1;
    AkUInt32 m_bIsPersistent    :1; // True if it comes from a higher level context: do not remove on PBI::RefreshParameters()
	AkDeltaType m_eReason		:6;

	bool operator<(const AkMutedMapItem& in_sCompareTo) const
	{
		if( m_Identifier == in_sCompareTo.m_Identifier )
		{
			return m_bIsGlobal < in_sCompareTo.m_bIsGlobal;
		}
		return m_Identifier < in_sCompareTo.m_Identifier;
	}

	bool operator==( const AkMutedMapItem& in_sCompareTo ) const
	{
		return( m_Identifier == in_sCompareTo.m_Identifier && m_bIsGlobal == in_sCompareTo.m_bIsGlobal );
	}	
};

class AkMutedMap: public CAkKeyArray<AkMutedMapItem, AkReal32>
{
public:

	bool MuteNotification( AkReal32 in_fMuteRatio, AkMutedMapItem& in_rMutedItem, bool in_bPrioritizeGameObjectSpecificItems)
	{
		if ( in_bPrioritizeGameObjectSpecificItems )
		{
			// Mute notifications never apply to persistent mute items.
			AKASSERT( !in_rMutedItem.m_bIsPersistent );

			// Search the "opposite" entry for this identifier (i.e. if we're setting
			// a global entry, let's search for a non-global entry, and vice-versa)
			AkMutedMapItem searchItem;
			searchItem.m_Identifier = in_rMutedItem.m_Identifier;
			searchItem.m_bIsGlobal = ! in_rMutedItem.m_bIsGlobal;
			searchItem.m_bIsPersistent = false; 			

			if ( Exists( searchItem ) )
			{
				if ( in_rMutedItem.m_bIsGlobal )
				{
					// We already have a non-global entry for this
					// identifier. Since we were asked to prioritize
					// game object-specific entries, we will simply
					// ignore the new info.
					return false;
				}
				else
				{
					// We have a global entry for this identifier. Since
					// we were asked to prioritize game object-specific
					// entries, we must remove it and replace it with the
					// new, game object-specific entry (below).
					Unset( searchItem );
				}
			}
		}

		// There's no point in keeping an unmuted entry, except if we're asked
		// to prioritize game object-specific items, in which case we must
		// keep it to make sure it doesn't get replaced by a global entry later.
		if (in_fMuteRatio == AK_UNMUTED_RATIO && (!in_bPrioritizeGameObjectSpecificItems || in_rMutedItem.m_bIsGlobal))		
			Unset(in_rMutedItem);		
		else		
			Set(in_rMutedItem, in_fMuteRatio);			
		
		AKASSERT(in_rMutedItem.m_eReason != AkDelta_None);
		return true;
	}
};

#endif
