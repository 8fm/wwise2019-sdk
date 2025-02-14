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

#include "IParameterNodeProxy.h"

#include "AkSwitchCntr.h"

class CAkSwitchCntr;


class ISwitchContainerProxy : virtual public IParameterNodeProxy
{
public:

	virtual void SetSwitchGroup( AkUInt32 in_ulGroup, AkGroupType in_eGroupType ) = 0;

	virtual void SetDefaultSwitch( AkUInt32 in_switch ) = 0;

	virtual void ClearSwitches() = 0;

	virtual void AddSwitch( AkSwitchStateID in_switch ) = 0;
	virtual void RemoveSwitch( AkSwitchStateID in_switch ) = 0;

	virtual void AddNodeInSwitch(
		AkUInt32			in_switch,
		AkUniqueID		in_nodeID
		) = 0;

	virtual void RemoveNodeFromSwitch(
		AkUInt32			in_switch,
		AkUniqueID		in_nodeID
		) = 0;

	virtual void SetContinuousValidation( bool in_bIsContinuousCheck ) = 0;

	virtual void SetContinuePlayback( AkUniqueID in_NodeID, bool in_bContinuePlayback ) = 0;
	virtual void SetFadeInTime( AkUniqueID in_NodeID, AkTimeMs in_time ) = 0;
	virtual void SetFadeOutTime( AkUniqueID in_NodeID, AkTimeMs in_time ) = 0;
	virtual void SetOnSwitchMode( AkUniqueID in_NodeID, AkOnSwitchMode in_bSwitchMode ) = 0;
	virtual void SetIsFirstOnly( AkUniqueID in_NodeID, bool in_bIsFirstOnly ) = 0;



	enum MethodIDs
	{
		MethodSetSwitchGroup = IParameterNodeProxy::LastMethodID,
		MethodSetDefaultSwitch,
		MethodClearSwitches,
		MethodAddSwitch,
		MethodRemoveSwitch,
		MethodAddNodeInSwitch,
		MethodRemoveNodeFromSwitch,
		MethodSetContinuousValidation,
		MethodSetContinuePlayback,
		MethodSetFadeInTime,
		MethodSetFadeOutTime,
		MethodSetOnSwitchMode,
		MethodSetIsFirstOnly,

		LastMethodID
	};
};

namespace SwitchContainerProxyCommandData
{
	struct SetSwitchGroup : public ObjectProxyCommandData::CommandData
	{
		SetSwitchGroup();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_ulGroup;
		AkGroupType m_eGroupType;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetDefaultSwitch : public ObjectProxyCommandData::CommandData
	{
		SetDefaultSwitch();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_switch;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct ClearSwitches : public ObjectProxyCommandData::CommandData
	{
		ClearSwitches();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct AddSwitch : public ObjectProxyCommandData::CommandData
	{
		AddSwitch();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkSwitchStateID m_switch;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct RemoveSwitch : public ObjectProxyCommandData::CommandData
	{
		RemoveSwitch();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkSwitchStateID m_switch;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct AddNodeInSwitch : public ObjectProxyCommandData::CommandData
	{
		AddNodeInSwitch();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_switch;
		AkUniqueID m_nodeID;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct RemoveNodeFromSwitch : public ObjectProxyCommandData::CommandData
	{
		RemoveNodeFromSwitch();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_switch;
		AkUniqueID m_nodeID;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetContinuousValidation : public ObjectProxyCommandData::CommandData
	{
		SetContinuousValidation();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		bool m_bIsContinuousCheck;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetContinuePlayback : public ObjectProxyCommandData::CommandData
	{
		SetContinuePlayback();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_nodeID;
		bool m_bContinuePlayback;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetFadeInTime : public ObjectProxyCommandData::CommandData
	{
		SetFadeInTime();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_nodeID;
		AkTimeMs m_time;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetFadeOutTime : public ObjectProxyCommandData::CommandData
	{
		SetFadeOutTime();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_nodeID;
		AkTimeMs m_time;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetOnSwitchMode : public ObjectProxyCommandData::CommandData
	{
		SetOnSwitchMode();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_nodeID;
		AkOnSwitchMode m_bSwitchMode;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetIsFirstOnly : public ObjectProxyCommandData::CommandData
	{
		SetIsFirstOnly();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_nodeID;
		bool m_bIsFirstOnly;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
