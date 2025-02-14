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
// AkMusicSwitchCntr.h
//
// Music Switch container definition.
//
//////////////////////////////////////////////////////////////////////
#ifndef _MUSIC_SWITCH_CNTR_H_
#define _MUSIC_SWITCH_CNTR_H_

#include "AkMusicTransAware.h"
#include "AkPreparationAware.h"
#include "AkDecisionTree.h"

class CAkMusicSwitchCntr : public CAkMusicTransAware
						 , public CAkPreparationAware
{
public:

    // Thread safe version of the constructor.
	static CAkMusicSwitchCntr * Create(
        AkUniqueID in_ulID = 0
        );

	AKRESULT SetInitialValues( AkUInt8* in_pData, AkUInt32 in_ulDataSize );

	virtual void SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax );
	virtual void SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax );

	using CAkMusicTransAware::SetAkProp; //This will remove warnings on Vita compilers

	AKRESULT SetDecisionTree( void* in_pData, AkUInt32 in_uSize, AkUInt32 in_uDepth );
	AKRESULT SetArguments( AkUniqueID* in_pArgs, AkUInt8* in_pGroupTypes, AkUInt32 in_uNumArgs );

    // Return the node category.
	virtual AkNodeCategory NodeCategory();

	// Override MusicObject::ExecuteAction() to catch Seek actions.
	virtual void ExecuteAction( ActionParams& in_rAction );

    virtual AKRESULT CanAddChild(
        CAkParameterNodeBase * in_pAudioNode 
        );

    // Context factory.
    virtual CAkMatrixAwareCtx * CreateContext( 
        CAkMatrixAwareCtx * in_pParentCtx,
        CAkRegisteredObj * in_GameObject,
        UserParams &  in_rUserparams,
		bool in_bPlayDirectly
        );

    // Play the specified node
    //
    // Return - AKRESULT - Ak_Success if succeeded
	virtual AKRESULT PlayInternal( AkPBIParams& in_rPBIParams );

	virtual AKRESULT ModifyActiveState( AkUInt32 in_stateID, bool in_bSupported );
	virtual AKRESULT PrepareData();
	virtual void UnPrepareData();

	bool ContinuePlayback(){ return m_bIsContinuePlayback; }
	void ContinuePlayback( bool in_bContinuePlayback ){ m_bIsContinuePlayback = in_bContinuePlayback; }


	// Interface for Contexts
	// ----------------------

	inline AkUInt32 GetSwitchGroup( AkUInt32 in_uIdx ) const
	{ 
		return m_pArguments != NULL ? (AkUInt32)m_pArguments[in_uIdx] : 0; 
	}
	
	inline AkGroupType GetSwitchGroupType( AkUInt32 in_uIdx ) const
	{
		return (AkGroupType) (m_pGroupTypes != NULL ? m_pGroupTypes[in_uIdx] : 0); 
	}

	inline AkUInt32 GetTreeDepth() const { return m_decisionTree.Depth(); }

	AkForceInline AkUniqueID ResolvePath( AkArgumentValueID * in_pPath, AkUInt32 in_cPath, AkPlayingID in_idSequence )
	{
		return m_decisionTree.ResolvePath( ID(), in_pPath, in_cPath, in_idSequence );
	}

	virtual void GatherSounds( AkSoundArray& io_aActiveSounds, AkSoundArray& io_aInactiveSounds, AkGameSyncArray& io_aGameSyncs, bool in_bIsActive, CAkRegisteredObj* in_pGameObj, AkUInt32 in_uUpdateGameSync=0, AkUInt32 in_uNewGameSyncValue=0  );

protected:
    CAkMusicSwitchCntr( 
        AkUniqueID in_ulID
        );
    virtual ~CAkMusicSwitchCntr();
    AKRESULT Init() { return CAkMusicNode::Init(); }
	void	 Term();

private:
	void ReleaseArgments();

	AkDecisionTree	m_decisionTree;

	bool		m_bIsContinuePlayback;

	AkUniqueID* m_pArguments;
	AkUInt8* m_pGroupTypes;
};

#endif //_MUSIC_SWITCH_CNTR_H_
