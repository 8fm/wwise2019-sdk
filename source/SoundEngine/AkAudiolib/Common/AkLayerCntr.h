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
// AkLayerCntr.h
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKLAYERCNTR_H_
#define _AKLAYERCNTR_H_

#include "AkMultiPlayNode.h"
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkListBareLight.h>

// class corresponding to a Layer container
//
// Author:  dmouraux
class CAkLayerCntr
	: public CAkMultiPlayNode
{
	DECLARE_BASECLASS(CAkMultiPlayNode);

public:

	// Thread safe version of the constructor
	static CAkLayerCntr* Create( AkUniqueID in_ulID = 0 );

	//Return the node category
	virtual AkNodeCategory NodeCategory();

 	// Call a play on the definition directly
	//
    // Return - AKRESULT - Ak_Success if succeeded
	virtual AKRESULT PlayInternal( AkPBIParams& in_rPBIParams );

	virtual void ExecuteAction(ActionParams& in_rAction);
	virtual void ExecuteActionNoPropagate(ActionParams& in_rAction);
	virtual void ExecuteActionExcept(ActionParamsExcept& in_rAction);

	virtual void OnPreRelease();

	AKRESULT SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize);

    virtual AKRESULT AddChild( WwiseObjectIDext in_ulID );
    virtual void RemoveChild( CAkParameterNodeBase* in_pChild );
	using CAkMultiPlayNode::RemoveChild; //This will remove warnings Vita compiler

	AKRESULT AddLayer( AkUniqueID in_LayerID );
	AKRESULT RemoveLayer( AkUniqueID in_LayerID );

	void SetContinuousValidation(
		bool in_bIsContinuousCheck
		);
	bool IsContinuousValidation() const { return m_bIsContinuousValidation; }

	struct ContPlaybackChildMuteCount
	{
		AkUniqueID key;
		AkUInt32   uCount;
	};

	struct ContPlaybackItem
	{
		ContPlaybackItem *  pNextLightItem; // for AkListContPlayback

		AkRTPCKey			rtpcKey;
		UserParams			UserParameters;
		AkPlaybackState		ePlaybackState;
		AkSortedKeyArray<AkUniqueID, ContPlaybackChildMuteCount, ArrayPoolDefault > childMuteCount;
		CAkModulatorData    modulatorData;
#ifndef AK_OPTIMIZED
		AkUniqueID			playTargetID;	// ID of original played item, which led to this PBI
		bool				bPlayDirectly;
#endif		
	}; 

	virtual AKRESULT AddChildInternal( CAkParameterNodeBase* in_pChild );

protected:
	friend class CAkLayer;

    CAkLayerCntr( AkUniqueID in_ulID );
	virtual ~CAkLayerCntr();

	void CleanContPlayback();
	void NotifyEndContinuous(ContPlaybackItem& in_rSwitchContPlayback);
	void SetPlaybackStateContInst(CAkRegisteredObj * in_pGameObj, AkPlayingID in_PlayingID, AkPlaybackState in_PBState);
	void ResumeContInst(CAkRegisteredObj * in_pGameObj, AkPlayingID in_PlayingID);
	void StopContInst(CAkRegisteredObj * in_pGameObj, AkPlayingID in_PlayingID);

	typedef AkArray< CAkLayer*, CAkLayer*, ArrayPoolDefault > LayerList;
	LayerList m_layers;

	typedef AkListBareLight< ContPlaybackItem > AkListContPlayback;
	AkListContPlayback m_listContPlayback;

	bool m_bIsContinuousValidation;

private:
	void ExecuteActionInternal(ActionParams& in_rAction);
};

#endif // _AKLAYERCNTR_H_
