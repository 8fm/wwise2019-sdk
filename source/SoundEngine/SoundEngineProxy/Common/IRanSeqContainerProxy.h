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

#include "AkRanSeqCntr.h"

class CAkRanSeqCntr;

class IRanSeqContainerProxy : virtual public IParameterNodeProxy
{
	DECLARE_BASECLASS( IParameterNodeProxy );
public:
	virtual void Mode( AkContainerMode in_eMode	) = 0;
	virtual void IsGlobal( bool in_bIsGlobal ) = 0;

	// Sequence mode related methods
    virtual void SetPlaylist( void* in_pvListBlock, AkUInt32 in_ulParamBlockSize ) = 0;
	virtual void ResetPlayListAtEachPlay( bool in_bResetPlayListAtEachPlay ) = 0;
	virtual void RestartBackward( bool in_bRestartBackward ) = 0;
	virtual void Continuous( bool in_bIsContinuous ) = 0;
	virtual void ForceNextToPlay( AkInt16 in_position, AkWwiseGameObjectID in_gameObjPtr = 0, AkPlayingID in_playingID = NO_PLAYING_ID ) = 0;
	virtual AkInt16 NextToPlay( AkWwiseGameObjectID in_gameObjPtr = 0 ) = 0;

	// Random mode related methods
	virtual void RandomMode( AkRandomMode in_eRandomMode ) = 0;
	virtual void AvoidRepeatingCount( AkUInt16 in_count ) = 0;
	virtual void SetItemWeight( AkUniqueID in_itemID, AkUInt32 in_weight ) = 0;
	virtual void SetItemWeight( AkUInt16 in_position, AkUInt32 in_weight ) = 0;

	virtual void Loop( 
		bool in_bIsLoopEnabled, 
		bool in_bIsLoopInfinite, 
		AkInt16 in_loopCount,
		AkInt16 in_loopModMin, 
		AkInt16 in_loopModMax ) = 0;

	virtual void TransitionMode( AkTransitionMode in_eTransitionMode ) = 0;
	virtual void TransitionTime( AkTimeMs in_transitionTime, AkTimeMs in_rangeMin = 0, AkTimeMs in_rangeMax = 0 ) = 0;


	enum MethodIDs
	{
		MethodMode = __base::LastMethodID,
		MethodIsGlobal,
        MethodSetPlaylist,
		MethodResetPlayListAtEachPlay,
		MethodRestartBackward,
		MethodContinuous,
		MethodForceNextToPlay,
		MethodNextToPlay,
		MethodRandomMode,
		MethodAvoidRepeatingCount,
		MethodSetItemWeight_withID,
		MethodSetItemWeight_withPosition,

		MethodLoop,
		MethodTransitionMode,
		MethodTransitionTime,

		LastMethodID
	};
};

namespace RanSeqContainerProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodMode,
		AkUInt32 // AkContainerMode
	> Mode;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodIsGlobal,
		bool
	> IsGlobal;

    struct SetPlaylist : public ObjectProxyCommandData::CommandData
	{
		SetPlaylist();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		void*       m_pvListBlock;
        AkUInt32    m_ulParamBlockSize;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodResetPlayListAtEachPlay,
		bool
	> ResetPlayListAtEachPlay;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodRestartBackward,
		bool
	> RestartBackward;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodContinuous,
		bool
	> Continuous;

	typedef ObjectProxyCommandData::CommandData3
	< 
		IRanSeqContainerProxy::MethodForceNextToPlay,
		AkInt16,
		AkWwiseGameObjectID,
		AkPlayingID
	> ForceNextToPlay;

	typedef ObjectProxyCommandData::CommandData3
	< 
		IRanSeqContainerProxy::MethodForceNextToPlay,
		AkInt16,
		AkWwiseGameObjectID,
		AkPlayingID
	> ForceNextToPlay;

	struct NextToPlay : public ObjectProxyCommandData::CommandData
	{
		NextToPlay();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjPtr;

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodRandomMode,
		AkUInt32 // AkRandomMode
	> RandomMode;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodAvoidRepeatingCount,
		AkUInt16
	> AvoidRepeatingCount;

	typedef ObjectProxyCommandData::CommandData2
	< 
		IRanSeqContainerProxy::MethodSetItemWeight_withID,
		AkUniqueID,
		AkUInt32
	> SetItemWeight_withID;

	typedef ObjectProxyCommandData::CommandData2
	< 
		IRanSeqContainerProxy::MethodSetItemWeight_withPosition,
		AkUInt16,
		AkUInt32
	> SetItemWeight_withPosition;

	typedef ObjectProxyCommandData::CommandData5
	< 
		IRanSeqContainerProxy::MethodLoop,
		bool,
		bool,
		AkInt16,
		AkInt16,
		AkInt16
	> Loop;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IRanSeqContainerProxy::MethodTransitionMode,
		AkUInt32 // AkTransitionMode
	> TransitionMode;

	typedef ObjectProxyCommandData::CommandData3
	< 
		IRanSeqContainerProxy::MethodTransitionTime,
		AkTimeMs,
		AkTimeMs,
		AkTimeMs 
	> TransitionTime;
}

#endif // #ifndef AK_OPTIMIZED
