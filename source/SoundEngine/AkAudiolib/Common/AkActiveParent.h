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
// AkActiveParent.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_ACTIVE_PARENT_H_
#define _AK_ACTIVE_PARENT_H_

#include "AkParameterNode.h"
#include "AkParentNode.h"

template <class T> class CAkActiveParent : public CAkParentNode<T>
{
public:

	CAkActiveParent(AkUniqueID in_ulID)
	:CAkParentNode<T>(in_ulID)
	{
	}

	virtual ~CAkActiveParent()
	{
	}

	AKRESULT Init()
	{ 
		return CAkParentNode<T>::Init();
	}

	virtual void ExecuteAction( ActionParams& in_rAction )
	{
		if( CAkParentNode<T>::IsActiveOrPlaying() )
		{
			// Process children in reverse order in order to avoid breaking the array if 
			// they self-destruct within ExecuteAction().
			AkUInt32 uIndex = this->m_mapChildId.Length();
			while ( uIndex > 0 )
			{
				CAkParameterNodeBase * pNode = this->m_mapChildId[--uIndex];
				if ( !in_rAction.bIsFromBus || !pNode->ParentBus() )
					pNode->ExecuteAction( in_rAction );
			}

			// WARNING: this may have self-destructed during last call to pNode->ExecuteAction( in_rAction );
		}
	}

	virtual void ExecuteActionNoPropagate(ActionParams& in_rAction)
	{
		this->CheckPauseTransition(in_rAction);
	}

	virtual void ExecuteActionExcept( ActionParamsExcept& in_rAction )
	{
		this->CheckPauseTransition(in_rAction);

		// Process children in reverse order in order to avoid breaking the array if 
		// they self-destruct within ExecuteAction().
		AkUInt32 uIndex = this->m_mapChildId.Length();
		while ( uIndex > 0 )
		{
			CAkParameterNodeBase * pNode = this->m_mapChildId[--uIndex];
			if( !(in_rAction.bIsFromBus) || !pNode->ParentBus())
			{
				if( !this->IsException( pNode, *(in_rAction.pExeceptionList) ) )
					pNode->ExecuteActionExcept( in_rAction );
			}
		}
		// WARNING: this may have self-destructed during last call to pNode->ExecuteAction( in_rAction );
	}

	virtual void PlayToEnd( CAkRegisteredObj * in_pGameObj , CAkParameterNodeBase* in_NodePtr, AkPlayingID in_PlayingID /* = AK_INVALID_PLAYING_ID */ )
	{
		for( AkMapChildID::Iterator iter = this->m_mapChildId.Begin(); iter != this->m_mapChildId.End(); ++iter )
		{
			CAkParameterNodeBase* pNode = (*iter);
			pNode->PlayToEnd( in_pGameObj, in_NodePtr, in_PlayingID );
		}
	}

	virtual void ParamNotification( NotifParams& in_rParams )
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if(!in_rParams.bIsFromBus || (*iter)->ParentBus() == NULL )
				{
					// IsActivityChunkEnabled == true does not mean it is playing, could be active or something else too... so we check...
					if( (*iter)->IsPlaying() )
					{
						if( !(*iter)->ParamOverriden( in_rParams.eType ) )
						{
							(*iter)->ParamNotification( in_rParams );
						}
					}
				}
			}
		}	
	}

	virtual void MuteNotification(AkReal32 in_fMuteRatio, AkMutedMapItem& in_rMutedItem, bool	in_bIsFromBus = false)
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				// Avoid notifying those who does not want to be notified
				if(!in_bIsFromBus || (*iter)->ParentBus() == NULL)
				{
					if( (*iter)->IsPlaying() )
					{
						(*iter)->MuteNotification(in_fMuteRatio, in_rMutedItem, in_bIsFromBus);
					}
				}
			}
		}
	}

	virtual void MuteNotification(AkReal32 in_fMuteRatio, CAkRegisteredObj * in_pGameObj, AkMutedMapItem& in_rMutedItem, bool in_bPrioritizeGameObjectSpecificItems = false)
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if( (*iter)->IsPlaying() )
				{
					(*iter)->MuteNotification(in_fMuteRatio, in_pGameObj, in_rMutedItem, in_bPrioritizeGameObjectSpecificItems);
				}
			}
		}
	}
	
	virtual void ForAllPBI( 
		AkForAllPBIFunc in_funcForAll,
		const AkRTPCKey & in_rtpcKey,
		void * in_pCookie )
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if( (*iter)->IsPlaying() )
				{
					(*iter)->ForAllPBI( in_funcForAll, in_rtpcKey, in_pCookie );
				}
			}
		}
	}

	virtual void PropagatePositioningNotification(
		AkReal32			in_RTPCValue,	// Value
		AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a positioning ID.
		CAkRegisteredObj *		in_GameObj,		// Target Game Object
		AkRTPCExceptionChecker*	in_pExceptCheck = NULL
		)
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if( !(*iter)->PositioningInfoOverrideParent() && (*iter)->IsPlaying() )
				{
					(*iter)->PropagatePositioningNotification( in_RTPCValue, in_ParameterID, in_GameObj, in_pExceptCheck );
				}
			}
		}
	}

	virtual void ClearLimiters()
	{
		if ( this->IsActivityChunkEnabled() )
		{
			for ( AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if ( (*iter)->IsPlaying() )
				{
					(*iter)->ClearLimiters();
				}
			}
		}
	}

#ifndef AK_OPTIMIZED
	virtual void UpdateRTPC()
	{
		if ( this->IsActivityChunkEnabled() )
		{
			for ( AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if ( (*iter)->IsPlaying() )
				{
					(*iter)->UpdateRTPC();
				}
			}
		}
	}

	virtual void ResetRTPC()
	{
		if (this->IsActivityChunkEnabled())
		{
			for (AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter)
			{
				if ((*iter)->IsPlaying())
				{
					(*iter)->ResetRTPC();
				}
			}
		}
	}
#endif

	virtual void RecalcNotification( bool in_bLiveEdit, bool in_bLog = false )
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if( (*iter)->IsPlaying() )
				{
					(*iter)->RecalcNotification( in_bLiveEdit, in_bLog );
				}
			}
		}
	}

	virtual void NotifyBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask,
		CAkRegisteredObj * in_pGameObj,
		AkRTPCExceptionChecker* in_pExceptCheck = NULL
		)
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				CAkParameterNode* pParameterNode = (CAkParameterNode*)((*iter));
				if( pParameterNode->IsPlaying() && !pParameterNode->GetFXOverrideParent() )
				{
					pParameterNode->NotifyBypass( in_bitsFXBypass, in_uTargetMask, in_pGameObj, in_pExceptCheck );
				}
			}
		}
	}

	virtual AKRESULT PrepareData()
	{
		AKRESULT eResult = AK_Success;
		for( AkMapChildID::Iterator iter = this->m_mapChildId.Begin(); iter != this->m_mapChildId.End(); ++iter )
		{
			eResult = (*iter)->PrepareData();
			if( eResult != AK_Success )
			{
				// Here unprepare what have been prepared up to now.
				for( AkMapChildID::Iterator iterFlush = this->m_mapChildId.Begin(); iterFlush != iter; ++iterFlush )
				{
					(*iterFlush)->UnPrepareData();
				}
				break;
			}
		}

		return eResult;
	}

	virtual void UnPrepareData()
	{
		for( AkMapChildID::Iterator iter = this->m_mapChildId.Begin(); iter != this->m_mapChildId.End(); ++iter )
		{
			(*iter)->UnPrepareData();
		}
	}

#ifndef AK_OPTIMIZED
	virtual void InvalidatePositioning()
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if( (*iter)->IsPlaying() )
				{
					(*iter)->InvalidatePositioning();
				} 
			}
		}
	}

#endif //AK_OPTIMIZED

	virtual void UpdateFx(
		AkUInt32 in_uFXIndex
		)
	{
		if( this->IsActivityChunkEnabled() )
		{
			for(AkMapChildID::Iterator iter = CAkParentNode<T>::m_mapChildId.Begin(); iter != CAkParentNode<T>::m_mapChildId.End(); ++iter )
			{
				if( (*iter)->IsPlaying() )
					(*iter)->UpdateFx( in_uFXIndex );
			}
		}
	}

};

#endif
