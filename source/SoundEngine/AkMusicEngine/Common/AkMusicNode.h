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
// AkMusicNode.h
//
// The Music node is meant to be a parent of all playable music objects (excludes tracks).
// Has the definition of the music specific Play method.
// Defines the method for grid query (music objects use a grid, either their own, or that of their parent).
//
//////////////////////////////////////////////////////////////////////

#ifndef _MUSIC_NODE_H_
#define _MUSIC_NODE_H_

#include "AkActiveParent.h"
#include "AkParameterNode.h"
#include "AkMusicStructs.h"
#include <AK/Tools/Common/AkArray.h>

class CAkMatrixAwareCtx;
class CAkSegmentBucket;

class CAkMusicNode : public CAkActiveParent<CAkParameterNode>
{
public:

	AKRESULT SetMusicNodeParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool in_bPartialLoadOnly );

    virtual CAkMatrixAwareCtx * CreateContext( 
        CAkMatrixAwareCtx * in_pParentCtx,
        CAkRegisteredObj * in_GameObject,
        UserParams & in_rUserparams,
		bool in_bPlayDirectly
        ) = 0;

    // Music implementation of game triggered actions handling ExecuteAction(): 
    // For Stop/Pause/Resume, call the music renderer, which searches among its
    // contexts (music renderer's contexts are the "top-level" contexts).
    // Other actions (actions on properties) are propagated through node hierarchy.
    virtual void ExecuteAction( ActionParams& in_rAction );
	virtual void ExecuteActionNoPropagate( ActionParams& in_rAction );
	virtual void ExecuteActionExcept( ActionParamsExcept& in_rAction );

	// Block some parameters notifications (pitch)
	virtual void ParamNotification( NotifParams& in_rParams );

	// Block some RTPC notifications (playback speed)
	virtual void RecalcNotificationWithID( bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID );
	virtual void RecalcNotificationWithBitArray( bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray );

	virtual void NotifyParamChanged( bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID ) { RecalcNotificationWithID( in_bLiveEdit, in_rtpcID ); }
	virtual void NotifyParamsChanged( bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray ) { RecalcNotificationWithBitArray( in_bLiveEdit, in_bitArray ); }
	
	virtual void PushParamUpdate(AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpcKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue, AkRTPCExceptionChecker* in_pExceptCheck);

	using CAkActiveParent<CAkParameterNode>::SetAkProp;

	// Set the value of a property at the node level (float)
	// Extend SetAkProp in order to catch relevant music notifications and notify the music renderer.
	virtual void SetAkProp( 
		AkPropID in_eProp, 
		AkReal32 in_fValue, 
		AkReal32 in_fMin, 
		AkReal32 in_fMax 
		);

	// Set the value of a property at the node level (int)
	virtual void SetAkProp( 
		AkPropID in_eProp, 
		AkInt32 in_iValue, 
		AkInt32 in_iMin, 
		AkInt32 in_iMax 
		);

	// Music notifications. Forward to music renderer.
	virtual void MusicNotification(
		AkReal32			in_RTPCValue,	// Value
		AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a positioning ID.
		CAkRegisteredObj *	in_GameObj		// Target Game Object
		);

	virtual AkObjectCategory Category();

	// Pull music parameters from the hierarchy.
	virtual void GetMusicParams(
		AkMusicParams & io_musicParams,
		const AkRTPCKey& in_rtpcKey
		);

	typedef AkArray< CAkStinger, const CAkStinger&, AkArrayAllocatorNoAlign<AkMemID_Structure> > StingerArray;

	class CAkStingers
	{
	public:
		const StingerArray&	GetStingerArray() const { return m_StingerArray; }
        StingerArray&	GetStingerArray() { return m_StingerArray; }
		void			Term(){ m_StingerArray.Term(); }

		void RemoveAllStingers() { m_StingerArray.RemoveAll(); }

	private:
		StingerArray m_StingerArray;
	};

	AKRESULT SetStingers( CAkStinger* in_pStingers, AkUInt32 in_NumStingers );

	void GetStingers( CAkStingers* io_pStingers );

    // Wwise access.
    // -----------------------------------------------
    void MeterInfo(
        const AkMeterInfo * in_pMeterInfo   // Music grid info. NULL if inherits that of parent.
        );

	const AkMusicGrid & GetMusicGrid();

	void GetMidiTargetNode( bool & r_bOverrideParent, AkUniqueID & r_uMidiTargetId, bool & r_bIsMidiTargetBus );
	void GetMidiTempoSource( bool & r_bOverrideParent, AkMidiTempoSource & r_eTempoSource );
	void GetMidiTargetTempo( bool & r_bOverrideParent, AkReal32 & r_fTargetTempo );

	void SetOverride( AkMusicOverride in_eOverride, bool in_bValue );
	void SetFlag( AkMusicFlag in_eFlag, bool in_bValue );

protected:
    CAkMusicNode( 
        AkUniqueID in_ulID
        );
    virtual ~CAkMusicNode();

	void FlushStingers();

    AKRESULT Init() { return CAkActiveParent<CAkParameterNode>::Init(); }

	virtual AKRESULT	PrepareData();
	virtual void		UnPrepareData();
	virtual AKRESULT	PrepareMusicalDependencies();
	virtual void		UnPrepareMusicalDependencies();

private:
    // Music grid.
    AkMusicGrid     m_grid;

	AkUInt8			m_bOverrideParentMidiTempo :1;
	AkUInt8			m_bOverrideParentMidiTarget :1;
    AkUInt8         m_bOverrideParentGrid :1;

	AkUInt8			m_bMidiTargetTypeBus :1;

	CAkStingers* 	m_pStingers;

	void ExecuteActionInternal(ActionParams& in_rAction);
};

#endif //_MUSIC_NODE_H_
