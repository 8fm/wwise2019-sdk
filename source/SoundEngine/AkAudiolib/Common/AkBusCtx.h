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
// AkBusCtx.h
//
// Definition of the Audio engine bus runtime context.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_BUS_CTX_H_
#define _AK_BUS_CTX_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSpeakerConfig.h>
#include "AkPrivateTypes.h"
#include "AkLEngineDefs.h"

class CAkBus;
class CAkRegisteredObj;

namespace AK
{
    //-----------------------------------------------------------------------------
    // CAkBusCtx interface.
	//  Acts as a 'mix bus key' to uniquely identify instances. If the bus has a upper-engine counterpart (CAkBus) then m_pBus is valid,
	//	  and m_gameObjectID is valid.  If the bus is anonymous, m_pBus is null, and m_deviceID is valid.  Anonymous bus instances are used 
	//    to preform a final mix to devices.
	//-----------------------------------------------------------------------------
    class CAkBusCtx
    {
    public:
		// Constructors 
		//	- It is important to zero out m_deviceID before setting m_gameObjectID, in the case that 
		//		sizeof(AkGameObjectID) < sizeof(AkOutputDeviceID), to not break operator==().
		CAkBusCtx() :m_pBus(NULL), m_deviceID(0) { m_gameObjectID = AK_INVALID_GAME_OBJECT; }
		CAkBusCtx(CAkBus* in_pBus, AkGameObjectID in_gameObjectID) :m_pBus(in_pBus), m_deviceID(0) { m_gameObjectID = in_gameObjectID; }
		CAkBusCtx(AkOutputDeviceID in_deviceID) : m_pBus(NULL), m_deviceID(in_deviceID) {}
				
		// Find next mixing bus in signal chain via hierarchy search.
		CAkBusCtx FindParentCtx() const;

	    // Sound parameters.
	    AkUniqueID		ID() const;
		inline AkGameObjectID	GameObjectID() const { return m_gameObjectID; }
		inline AkOutputDeviceID	DeviceID() const { AKASSERT(m_pBus == NULL); return m_deviceID; }
		inline bool operator==(const CAkBusCtx & in_other) const { return ((m_pBus == in_other.m_pBus) && (m_deviceID == in_other.m_deviceID)); }
		bool			IsTopBusCtx() const;
		AkChannelConfig	GetChannelConfig() const;

	    // Effects access.
		void			GetFXDataID( AkUInt32 in_uFXIndex, AkUInt32 in_uDataIndex, AkUniqueID& AkUniqueID) const;
		void			GetMixerPlugin( AkFXDesc& out_rFXInfo ) const;
		void			GetMixerPluginDataID( AkUInt32 in_uDataIndex, AkUInt32& out_rDataID ) const;
		bool			HasMixerPlugin() const;
		void			GetAttachedPropFX( AkFXDesc& out_rFXInfo ) const;

		inline void		SetBus(CAkBus* in_pBus, AkGameObjectID in_gameObjectID){ m_pBus = in_pBus; m_gameObjectID = in_gameObjectID; }
		inline CAkBus*	GetBus() const { return m_pBus; }
		inline bool		HasBus() const { return ( m_pBus != NULL ); }

		bool			IsAuxBus() const;

	protected:
		CAkBus* m_pBus;
		union {
			AkOutputDeviceID	m_deviceID; //For anonymous busses. Only valid if m_pBus==NULL.  
			AkGameObjectID		m_gameObjectID;
		};
    };

	// CAkBusCtx with a CAkRegisteredObj pointer that is resolved from the registry manager.
	class CAkBusCtxRslvd : public CAkBusCtx
	{
	public:
		CAkBusCtxRslvd(const CAkBusCtx& in_other) : CAkBusCtx(in_other), m_pGameObj(NULL) { Resolve(); }
		CAkBusCtxRslvd(const CAkBusCtxRslvd& in_other) : CAkBusCtx(in_other), m_pGameObj(NULL)  { Transfer(in_other.m_pGameObj); }
		CAkBusCtxRslvd() : CAkBusCtx(), m_pGameObj(NULL) {}
		CAkBusCtxRslvd(CAkBus* in_pBus, AkGameObjectID in_gameObjectID) : CAkBusCtx(in_pBus, in_gameObjectID), m_pGameObj(NULL) { Resolve(); }
		~CAkBusCtxRslvd();

		const CAkBusCtxRslvd& operator=(const CAkBusCtxRslvd& in_other)
		{
			*((CAkBusCtx*)this) = in_other;
			Transfer(in_other.m_pGameObj);
			return *this;
		}
		const CAkBusCtxRslvd& operator=(const CAkBusCtx& in_other)
		{
			*((CAkBusCtx*)this) = in_other;
			Resolve();
			return *this;
		}

		CAkRegisteredObj* GetGameObjPtr() const { return m_pGameObj; }
		bool IsValid() const { return m_pGameObj != NULL; }

		void GetFX(AkUInt32 in_uFXIndex, AkFXDesc& out_rFXInfo) const;

	protected:
		void Resolve();
		void Transfer(CAkRegisteredObj* m_pGameObj);

		CAkRegisteredObj* m_pGameObj;
	};
}

#endif // _AK_BUS_CTX_H_
