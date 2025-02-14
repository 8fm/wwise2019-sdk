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

#include <AK/SoundEngine/Common/AkTypes.h>
#include "CommandData.h"

class IStateMgrProxy
{
public:
	virtual void AddStateGroup( AkStateGroupID in_groupID ) const = 0;
	virtual void RemoveStateGroup( AkStateGroupID in_groupID ) const = 0;
	
	virtual void AddStateTransition( AkStateGroupID in_groupID, AkStateID in_stateID1, AkStateID in_stateID2, AkTimeMs in_transitionTime, bool in_bIsShared ) const = 0;
	virtual void RemoveStateTransition( AkStateGroupID in_groupID, AkStateID in_stateID1, AkStateID in_stateID2, bool in_bIsShared ) const = 0;
	virtual void ClearStateTransitions( AkStateGroupID in_groupID ) const = 0;
	virtual void SetDefaultTransitionTime( AkStateGroupID in_groupID, AkTimeMs in_transitionTime ) const = 0;

	virtual void SetState( AkStateGroupID in_groupID, AkStateID in_stateID ) const = 0;
	virtual AkStateID GetState( AkStateGroupID in_groupID ) const = 0;

	enum MethodIDs
	{
		MethodAddStateGroup = 1,
		MethodRemoveStateGroup,
		MethodAddStateTransition,
		MethodRemoveStateTransition,
		MethodClearStateTransitions,
		MethodSetDefaultTransitionTime,
		MethodSetState,
		MethodGetState,
        
		LastMethodID
	};
};

namespace StateMgrProxyCommandData
{
	struct CommandData : public ProxyCommandData::CommandData
	{
		CommandData();
		CommandData( AkUInt16 in_methodID );

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( ProxyCommandData::CommandData );
	};

	struct AddStateGroup : public CommandData
	{
		AddStateGroup();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;

		DECLARE_BASECLASS( CommandData );
	};

	struct RemoveStateGroup : public CommandData
	{
		RemoveStateGroup();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;

		DECLARE_BASECLASS( CommandData );
	};

	struct AddStateTransition : public CommandData
	{
		AddStateTransition();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;
		AkStateID m_stateID1;
		AkStateID m_stateID2;
		AkTimeMs m_transitionTime;
		bool m_bIsShared;

		DECLARE_BASECLASS( CommandData );
	};

	struct RemoveStateTransition : public CommandData
	{
		RemoveStateTransition();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;
		AkStateID m_stateID1;
		AkStateID m_stateID2;
		bool m_bIsShared;

		DECLARE_BASECLASS( CommandData );
	};

	struct ClearStateTransitions : public CommandData
	{
		ClearStateTransitions();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetDefaultTransitionTime : public CommandData
	{
		SetDefaultTransitionTime();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;
		AkTimeMs m_transitionTime;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetState : public CommandData
	{
		SetState();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;
		AkStateID m_stateID;

		DECLARE_BASECLASS( CommandData );
	};

	struct GetState : public CommandData
	{
		GetState();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkStateGroupID m_groupID;

		DECLARE_BASECLASS( CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
