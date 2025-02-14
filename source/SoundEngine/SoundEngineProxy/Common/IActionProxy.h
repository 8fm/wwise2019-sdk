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

#include "IObjectProxy.h"

#include "AkActions.h"

class IActionProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
	virtual void SetElementID( WwiseObjectIDext in_elementID ) = 0;
	virtual void SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax ) = 0;
	virtual void SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax ) = 0;
	virtual void CurveType( const AkCurveInterpolation in_eCurveType ) = 0;

	enum MethodIDs
	{
		MethodSetElementID = __base::LastMethodID,
		MethodSetAkPropF,
		MethodSetAkPropI,
		MethodCurveType,

		LastMethodID
	};
};

namespace ActionProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionProxy::MethodSetElementID,
		WwiseObjectIDext
	> SetElementID;

	typedef ObjectProxyCommandData::CommandData4
	< 
		IActionProxy::MethodSetAkPropF,
		AkUInt32, // AkPropID
		AkReal32,
		AkReal32,
		AkReal32
	> SetAkPropF;

	typedef ObjectProxyCommandData::CommandData4
	< 
		IActionProxy::MethodSetAkPropI,
		AkUInt32, // AkPropID
		AkInt32,
		AkInt32,
		AkInt32
	> SetAkPropI;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionProxy::MethodCurveType,
		AkUInt32 // AkCurveInterpolation
	> CurveType;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionExceptProxy : virtual public IActionProxy
{
	DECLARE_BASECLASS( IActionProxy );
public:
	virtual void AddException( WwiseObjectIDext in_elementID ) = 0;
	virtual void RemoveException( WwiseObjectIDext in_elementID ) = 0;
	virtual void ClearExceptions() = 0;

	enum MethodIDs
	{
		MethodAddException = __base::LastMethodID,
		MethodRemoveException,
		MethodClearExceptions,

		LastMethodID
	};
};

namespace ActionExceptProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionExceptProxy::MethodAddException,
		WwiseObjectIDext
	> AddException;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionExceptProxy::MethodRemoveException,
		WwiseObjectIDext
	> RemoveException;

	typedef ObjectProxyCommandData::CommandData0
	< 
		IActionExceptProxy::MethodRemoveException
	> ClearExceptions;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionStopProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionExceptProxy );
public:
	virtual void ApplyToStateTransitions( bool in_bApplyToStateTransitions ) = 0;
	virtual void ApplyToDynamicSequences( bool in_bApplyToDynamicSequences ) = 0;

	enum MethodIDs
	{
		MethodApplyToStateTransitions = __base::LastMethodID,
		MethodApplyToDynamicSequences,

		LastMethodID
	};
};

namespace ActionStopProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1< IActionStopProxy::MethodApplyToStateTransitions, bool > ApplyToStateTransitions;
	typedef ObjectProxyCommandData::CommandData1< IActionStopProxy::MethodApplyToDynamicSequences, bool > ApplyToDynamicSequences;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionPauseProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionExceptProxy );
public:
	virtual void IncludePendingResume( bool in_bIncludePendingResume ) = 0;
	virtual void ApplyToStateTransitions( bool in_bApplyToStateTransitions ) = 0;
	virtual void ApplyToDynamicSequences( bool in_bApplyToDynamicSequences ) = 0;

	enum MethodIDs
	{
		MethodIncludePendingResume = __base::LastMethodID,
		MethodApplyToStateTransitions,
		MethodApplyToDynamicSequences,

		LastMethodID
	};
};

namespace ActionPauseProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1< IActionPauseProxy::MethodIncludePendingResume, bool > IncludePendingResume;
	typedef ObjectProxyCommandData::CommandData1< IActionPauseProxy::MethodApplyToStateTransitions, bool > ApplyToStateTransitions;
	typedef ObjectProxyCommandData::CommandData1< IActionPauseProxy::MethodApplyToDynamicSequences, bool > ApplyToDynamicSequences;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionResumeProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionExceptProxy );
public:
	virtual void IsMasterResume( bool in_bIsMasterResume ) = 0;
	virtual void ApplyToStateTransitions( bool in_bApplyToStateTransitions ) = 0;
	virtual void ApplyToDynamicSequences( bool in_bApplyToDynamicSequences ) = 0;

	enum MethodIDs
	{
		MethodIsMasterResume = __base::LastMethodID,
		MethodApplyToStateTransitions,
		MethodApplyToDynamicSequences,

		LastMethodID
	};
};

namespace ActionResumeProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1< IActionResumeProxy::MethodIsMasterResume, bool > IsMasterResume;
	typedef ObjectProxyCommandData::CommandData1< IActionResumeProxy::MethodApplyToStateTransitions, bool > ApplyToStateTransitions;
	typedef ObjectProxyCommandData::CommandData1< IActionResumeProxy::MethodApplyToDynamicSequences, bool > ApplyToDynamicSequences;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionMuteProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionExceptProxy );
public:

	enum MethodIDs
	{
		LastMethodID = __base::LastMethodID
	};
};

class IActionSetAkPropProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionExceptProxy );
public:
	virtual void SetValue( AkReal32 in_fValue, AkValueMeaning in_eValueMeaning, AkReal32 in_fMin = 0, AkReal32 in_fMax = 0 ) = 0;

	enum MethodIDs
	{
		MethodSetValue = __base::LastMethodID,

		LastMethodID
	};
};

namespace ActionSetAkPropProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData4
	< 
		IActionSetAkPropProxy::MethodSetValue,
		AkReal32,
		AkUInt32, // AkValueMeaning
		AkReal32,
		AkReal32
	> SetValue;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionSetStateProxy : virtual public IActionProxy
{
	DECLARE_BASECLASS( IActionProxy );
public:
	virtual void SetGroup( AkStateGroupID in_groupID ) = 0;
	virtual void SetTargetState( AkStateID in_stateID ) = 0;

	enum MethodIDs
	{
		MethodSetGroup = __base::LastMethodID,
		MethodSetTargetState,

		LastMethodID
	};
};

namespace ActionSetStateProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionSetStateProxy::MethodSetGroup,
		AkStateGroupID
	> SetGroup;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionSetStateProxy::MethodSetTargetState,
		AkStateID
	> SetTargetState;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionSetSwitchProxy : virtual public IActionProxy
{
	DECLARE_BASECLASS( IActionProxy );
public:
	virtual void SetSwitchGroup( const AkSwitchGroupID in_ulSwitchGroupID ) = 0;
	virtual void SetTargetSwitch( const AkSwitchStateID in_ulSwitchID ) = 0;

	enum MethodIDs
	{
		MethodSetSwitchGroup = __base::LastMethodID,
		MethodSetTargetSwitch,

		LastMethodID
	};
};

namespace ActionSetSwitchProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionSetSwitchProxy::MethodSetSwitchGroup,
		AkSwitchGroupID
	> SetSwitchGroup;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionSetSwitchProxy::MethodSetTargetSwitch,
		AkSwitchStateID
	> SetTargetSwitch;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionSetGameParameterProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionExceptProxy );
public:
	virtual void SetValue( AkReal32 in_value, AkValueMeaning in_eValueMeaning, AkReal32 in_rangeMin, AkReal32 in_rangeMax ) = 0;
	virtual void BypassGameParameterInternalTransition( bool in_bBypassGameParameterInternalTransition ) = 0;

	enum MethodIDs
	{
		MethodSetValue = __base::LastMethodID,
		MethodBypassGameParameterInternalTransition,

		LastMethodID
	};
};

namespace ActionSetGameParameterProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData4
	< 
		IActionSetGameParameterProxy::MethodSetValue,
		AkPitchValue,
		AkUInt32, // AkValueMeaning
		AkPitchValue,
		AkPitchValue
	> SetValue;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionSetGameParameterProxy::MethodBypassGameParameterInternalTransition,
		bool
	> BypassGameParameterInternalTransition;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionUseStateProxy : virtual public IActionProxy
{
	DECLARE_BASECLASS( IActionProxy );
public:
	virtual void UseState( bool in_bUseState ) = 0;

	enum MethodIDs
	{
		MethodUseState = __base::LastMethodID,

		LastMethodID
	};
};

namespace ActionUseStateProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionUseStateProxy::MethodUseState,
		bool
	> UseState;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionBypassFXProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionExceptProxy );
public:
	virtual void BypassFX( bool in_bBypassFX ) = 0;
	virtual void SetBypassTarget( bool in_bBypassAllFX, AkUInt8 in_ucEffectsMask ) = 0;

	enum MethodIDs
	{
		MethodBypassFX = __base::LastMethodID,
		MethodSetBypassTarget,

		LastMethodID
	};
};

namespace ActionBypassFXProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionBypassFXProxy::MethodBypassFX,
		bool
	> BypassFX;

	typedef ObjectProxyCommandData::CommandData2
	< 
		IActionBypassFXProxy::MethodSetBypassTarget,
		bool,
		AkUInt8
	> SetBypassTarget;
}

// -----------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------

class IActionSeekProxy : virtual public IActionExceptProxy
{
	DECLARE_BASECLASS( IActionProxy );
public:
	virtual void SetSeekPositionPercent( AkReal32 in_position, // Position expressed as percentage of sound [0,1].
		AkReal32 in_rangeMin = 0, AkReal32 in_rangeMax = 0 ) = 0;
	virtual void SetSeekPositionTimeAbsolute( AkTimeMs in_position, // Position in milliseconds.
		AkTimeMs in_rangeMin = 0, AkTimeMs in_rangeMax = 0 ) = 0;
	virtual void SetSeekToNearestMarker( bool in_bSeekToNearestMarker ) = 0;

	enum MethodIDs
	{
		MethodSetSeekPositionPercent = __base::LastMethodID,
		MethodSetSeekPositionTimeAbsolute,
		MethodSetSeekToNearestMarker,

		LastMethodID
	};
};

namespace ActionSeekProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData3
	< 
		IActionSeekProxy::MethodSetSeekPositionPercent,
		AkReal32,
		AkReal32,
		AkReal32
	> SetSeekPositionPercent;

	typedef ObjectProxyCommandData::CommandData3
	< 
		IActionSeekProxy::MethodSetSeekPositionTimeAbsolute,
		AkTimeMs,
		AkTimeMs,
		AkTimeMs
	> SetSeekPositionTimeAbsolute;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IActionSeekProxy::MethodSetSeekToNearestMarker,
		bool
	> SetSeekToNearestMarker;
}

#endif // #ifndef AK_OPTIMIZED
