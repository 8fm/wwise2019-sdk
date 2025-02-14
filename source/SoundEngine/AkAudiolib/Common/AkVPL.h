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
// AkVPL.h
//
// Implementation of the Lower Audio Engine.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_H_
#define _AK_VPL_H_

#include "AkVPLMixBusNode.h"
#include "AkOutputMgr.h"

class AkVPL
{
public:
	AkVPL()
		: m_iDepth(INT_MAX)
		, m_bRecurseVisit(false)
		, m_bReferenced(false)
		, m_bIsHDR(false)
	{}

	~AkVPL()
	{
		if (IsTopLevelNode())
		{
			const AK::CAkBusCtx& busCtx = m_MixBus.GetBusContext();
			if (busCtx.HasBus() || busCtx.DeviceID() != AK_INVALID_GAME_OBJECT)
        	{
				AkDevice* pDevice = CAkOutputMgr::FindDevice(m_MixBus.GetBusContext());
				if(pDevice != NULL)
				{
					//WG-34023
					//We know this is a top level node, but this device might have an
					//extra mix node to mix hierarchies coming from different listeners.
					//If this is the case, the top node is the extra node, not the master bus.
					const AkVPL * pExtraNode = pDevice->GetExtraMixNode();
					if(pExtraNode == this || pExtraNode == NULL)
						pDevice->TopNodeDeleted();				
				}
			}
		}
	}

	bool IsTopLevelNode()
	{
		//Is is possible for a node to temporarily not have any connections when it is being destroyed, so also check the bus context.
		return !m_MixBus.HasConnections() && 
			( !m_MixBus.GetBusContext().HasBus() // <- Indicates an anonymous 'device mix' bus.
			|| m_MixBus.GetBusContext().GetBus()->ParentBus() == NULL);
	}

	AKRESULT Init(AK::CAkBusCtx in_busContext, AkVPL* in_pParent);
	inline const AK::CAkBusCtx & GetBusContext() { return m_MixBus.GetBusContext(); }

	inline AkReal32 DownstreamGainDB() { return m_MixBus.GetDownstreamGainDB(); }

	inline bool IsHDR() { return m_bIsHDR; }
	inline AkVPL * GetHdrBus()
	{
		// There can actually be more than one HDR bus, so we resolve in the following order.
		//	- (1) Me, 
		//  - (2) The direct connection to the parent with the same game object id as I,
		//	- (3) The first direct connection in the list.  

		if ( IsHDR() )
			return this;

		AkVPL * pHdrBus = NULL;
		for (AkMixConnectionList::Iterator it = m_MixBus.BeginConnection(); it != m_MixBus.EndConnection(); ++it)
		{
			AkMixConnection& conn = **it;
			if (conn.IsTypeDirect())
			{
				pHdrBus = (AkVPL*)conn.GetHdrBus();
				if (conn.GetListenerID() == m_MixBus.GetGameObjectID())
					break; // lets take this as the best match.
			}
		}

		return pHdrBus;
	}
	
	inline bool CanDestroy()
	{ 
		bool bCanDestroy = ( m_MixBus.GetState() != NodeStatePlay 
							&& !m_MixBus.HasInputs()
							&& !m_bReferenced );
		return bCanDestroy;
	}
	
	inline void OnFrameEnd()
	{ 
		m_bReferenced = false;
	}

	CAkVPLMixBusNode		m_MixBus;			// Mix bus node.

	AkInt32					m_iDepth;
	AkUInt8					m_bRecurseVisit : 1; // Recursive visit bit for DFS

	AkUInt8					m_bReferenced	:1;	// True when bus should be kept alive despite no connections or playback (such as during HDR window release).
	AkUInt8					m_bIsHDR		:1;	// True when bus is HDR (in such a case it is an AkHdrBus). 
	AkUInt8					m_bExpMode		:1;	// True when using exponential mode. HDR specific setting - garbage in standard VPLs. 

} AK_ALIGN_DMA;


#endif // _AK_VPL_H_

