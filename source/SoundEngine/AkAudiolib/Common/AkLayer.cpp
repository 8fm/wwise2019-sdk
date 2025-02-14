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
// AkLayer.cpp
//
// Implementation of the CAkLayer class, which represents a Layer
// in a Layer Container (CAkLayerCntr)
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkLayer.h"	
#include "AkLayerCntr.h"
#include "AkAudioLibIndex.h"
#include "AkPBI.h"
#include "AkRTPCMgr.h"
#include "AkParameterNode.h"
#include "AkAudioMgr.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include <float.h>

extern AkReal32 g_fVolumeThreshold;
extern AkReal32 g_fVolumeThresholdDB;

CAkLayer::CAkLayer( AkUniqueID in_ulID )
	: CAkIndexable( in_ulID )
	, m_pOwner( NULL )
	, m_crossfadingRTPCID( 0 )
	, m_crossfadingRTPCType( AkRtpcType_GameParameter )
{
}

CAkLayer::~CAkLayer(void)
{
	// Remove all RTPCs
	AKASSERT( g_pRTPCMgr );

	for ( AkUInt32 iBit = 0; !m_RTPCBitArray.IsEmpty(); ++iBit )
	{ 
		if ( m_RTPCBitArray.IsSet( iBit ) )
		{
			g_pRTPCMgr->UnSubscribeRTPC( this, iBit );
			m_RTPCBitArray.UnsetBit( iBit );
		}
	}

	if ( m_crossfadingRTPCID )
		g_pRTPCMgr->UnregisterLayer( this, m_crossfadingRTPCID );

	// Terminate all associations
	for( AssociatedChildMap::Iterator iter = m_assocs.Begin(); iter != m_assocs.End(); ++iter )
	{
		AKVERIFY( AK_Success == (*iter).item.Term( this ) );
	}

	m_assocs.Term();
}

CAkLayer* CAkLayer::Create( AkUniqueID in_ulID )
{
	CAkLayer* pAkLayer = AkNew(AkMemID_Structure, CAkLayer( in_ulID ) );
	if ( pAkLayer )
	{
		if ( pAkLayer->Init() != AK_Success )
		{
			pAkLayer->Release();
			pAkLayer = NULL;
		}
	}

	return pAkLayer;
}

AKRESULT CAkLayer::Init()
{
	AddToIndex();

	return AK_Success;
}

AKRESULT CAkLayer::SetRTPC(
		AkRtpcID					in_RTPC_ID,
		AkRtpcType					in_RTPCType,
		AkRtpcAccum					in_RTPCAccum,
		AkRTPC_ParameterID			in_ParamID,
		AkUniqueID					in_RTPCCurveID,
		AkCurveScaling				in_eScaling,
		AkRTPCGraphPoint*			in_pArrayConversion,		// NULL if none
		AkUInt32					in_ulConversionArraySize,	// 0 if none
		bool						/*in_bNotify*/
		)
{
	AKASSERT(g_pRTPCMgr);
	AKASSERT( (unsigned int)in_ParamID < sizeof(AkUInt64)*8 );

	// Remember that there is RTPC on this param
	m_RTPCBitArray.SetBit( in_ParamID );

	return g_pRTPCMgr->SubscribeRTPC( 
		this,
		in_RTPC_ID, 
		in_RTPCType,
		in_RTPCAccum,
		in_ParamID, 
		in_RTPCCurveID,
		in_eScaling,
		in_pArrayConversion, 
		in_ulConversionArraySize,
		AkRTPCKey(),								// Get notified for all Game Objects/notes/channels
		CAkRTPCMgr::SubscriberType_CAkLayer
	);
}

void CAkLayer::UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID )
{
	AKASSERT(g_pRTPCMgr);
	AKASSERT( (unsigned int)in_ParamID < sizeof(AkUInt64)*8 );

	bool bMoreCurvesRemaining = false;
	g_pRTPCMgr->UnSubscribeRTPC(
		this,
		in_ParamID,
		in_RTPCCurveID,
		&bMoreCurvesRemaining
	);

	// No more RTPC on this param?
	if ( ! bMoreCurvesRemaining )
		m_RTPCBitArray.UnsetBit( in_ParamID );

	RecalcNotification( true );
}

void CAkLayer::TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData ) const
{
	// This gets called from child nodes in ParameterNodeBase: only valid when _not_ in m_bVoiceMgmt mode
	if (!m_pOwner->m_bIsContinuousValidation)
	{
		AkModulatorSubscriberInfo subscrInfo;
		subscrInfo.pSubscriber = this;
		subscrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_CAkLayer;
		subscrInfo.pTargetNode = m_pOwner;
		g_pModulatorMgr->Trigger(subscrInfo, in_params, out_pModPBIData);
	}
}

// Return - AKRESULT - AK_Success if succeeded
AKRESULT CAkLayer::GetModulatorParamXfrms(
	AkModulatorParamXfrmArray&	io_paramsXfrms,
	AkRtpcID					in_modulatorID,		
	const CAkRegisteredObj *	in_GameObject
	) const 
{
	if( m_RTPCBitArray.IsSet( RTPC_Volume ) )
	{
		AkModulatorParamXfrm thisXfrm;
		if( g_pModulatorMgr->GetParamXfrm( this, RTPC_Volume, in_modulatorID, in_GameObject, thisXfrm ) )
			io_paramsXfrms.AddLast( thisXfrm );
	}
	if (m_RTPCBitArray.IsSet(RTPC_MakeUpGain))
	{
		AkModulatorParamXfrm thisXfrm;
		if (g_pModulatorMgr->GetParamXfrm(this, RTPC_MakeUpGain, in_modulatorID, in_GameObject, thisXfrm))
			io_paramsXfrms.AddLast(thisXfrm);
	}

	return AK_Success;
}

AKRESULT CAkLayer::GetAudioParameters(
	CAkParameterNode*	in_pAssociatedChild,	// Associated child making the call
	AkSoundParams		&io_Parameters,			// Set of parameter to be filled	
	AkMutedMap&			io_rMutedMap,			// Map of muted elements
	const AkRTPCKey&	in_rtpcKey,				// Key to retrieve rtpc parameters
	AkModulatorsToTrigger* in_pTriggerModulators
)
{
	AkDeltaMonitorObjBrace braceDelta(key);
	AKASSERT( in_pAssociatedChild );
	
	//TODO, Use the iterator?
	if (io_Parameters.Request.IsSet(AkPropID_MakeUpGain) && m_RTPCBitArray.IsSet(RTPC_MakeUpGain) )
		io_Parameters.Accumulate(AkPropID_MakeUpGain, g_pRTPCMgr->GetRTPCConvertedValue<AkDeltaMonitor>(this, RTPC_MakeUpGain, in_rtpcKey), AkDelta_None);	//Logging done in GetRTPCConvertedValue

	if (io_Parameters.Request.IsSet(AkPropID_Volume) && m_RTPCBitArray.IsSet(RTPC_Volume))
		io_Parameters.Accumulate(AkPropID_Volume, g_pRTPCMgr->GetRTPCConvertedValue<AkDeltaMonitor>( this, RTPC_Volume, in_rtpcKey ), AkDelta_None);	//Logging done in GetRTPCConvertedValue
	
	if (io_Parameters.Request.IsSet(AkPropID_Pitch) && m_RTPCBitArray.IsSet(RTPC_Pitch))
		io_Parameters.Accumulate(AkPropID_Pitch, g_pRTPCMgr->GetRTPCConvertedValue<AkDeltaMonitorDisabled>( this, RTPC_Pitch, in_rtpcKey ), AkDelta_None);	//Logging done in GetRTPCConvertedValue
	
	if (io_Parameters.Request.IsSet(AkPropID_LPF) && m_RTPCBitArray.IsSet(RTPC_LPF))
		io_Parameters.Accumulate(AkPropID_LPF, g_pRTPCMgr->GetRTPCConvertedValue<AkDeltaMonitor>( this, RTPC_LPF, in_rtpcKey ), AkDelta_None);	//Logging done in GetRTPCConvertedValue

	if (io_Parameters.Request.IsSet(AkPropID_HPF) && m_RTPCBitArray.IsSet(RTPC_HPF))
		io_Parameters.Accumulate(AkPropID_HPF, g_pRTPCMgr->GetRTPCConvertedValue<AkDeltaMonitor>(this, RTPC_HPF, in_rtpcKey), AkDelta_None);	//Logging done in GetRTPCConvertedValue

	// Crossfading
	if (m_crossfadingRTPCID != 0)
	{
		const AssociatedChildMap::Iterator itChild = m_assocs.FindEx(in_pAssociatedChild->ID());

		// Children call GetAudioParameters() on their associated layers. Since that association
		// is made by this class, we know there should always be an assoc for a child that calls this function.
		AKASSERT(itChild != m_assocs.End());

		if (itChild.pItem->item.m_fadeCurve.IsInitialized())
		{
			// Get the value of the Game Parameter for the specified Game Object. If it
			// doesn't exist (was never set), then use the default value.
			AkReal32 rtpcValue;
			bool bGameObjectSpecificValue = true;
			bool bIsAutomatedParam = false;
			AkRTPCKey retirevedKey = in_rtpcKey;
			if (!g_pRTPCMgr->GetRTPCValue<CurrentValue>(m_crossfadingRTPCID, RTPC_MaxNumRTPC, CAkRTPCMgr::SubscriberType_CAkCrossfadingLayer, retirevedKey, rtpcValue, bIsAutomatedParam))
			{
				bGameObjectSpecificValue = false;

				rtpcValue = g_pRTPCMgr->GetDefaultValue(m_crossfadingRTPCID);
			}
			else
			{
				bGameObjectSpecificValue = retirevedKey.GameObj() != NULL;
			}

			// Compute the muting level for this child at the current position of the Game Parameter.
			AkReal32 fMuteLevel = itChild.pItem->item.m_fadeCurve.Convert(rtpcValue);				
			if (fMuteLevel != AK_UNMUTED_RATIO || bGameObjectSpecificValue)
			{				
				AkDeltaMonitor::LogRTPC(m_crossfadingRTPCID, RTPC_MuteRatio, rtpcValue, fMuteLevel, bGameObjectSpecificValue ? AkDelta_RTPC : AkDelta_RTPCGlobal);

				// Set the value in the map. If it's AK_UNMUTED_RATIO, we still set it if
				// it's a game object-specific value, because we want the PBI
				// to know that it has a game object-specific muting value so it
				// doesn't replace it with a global value later.

				AkMutedMapItem item;
				item.m_bIsPersistent = false;
				item.m_bIsGlobal = !bGameObjectSpecificValue;
				item.m_Identifier = this;
				item.m_eReason = AkDelta_None;	//Don't log we just did that above, with the RTPC ID.

				// The map should be clean before calling this function!
				AKASSERT(!io_rMutedMap.Exists(item));

				io_rMutedMap.Set(item, fMuteLevel);				
			}
		}
	}
	
	//Gather modulators to trigger
	if (in_pTriggerModulators != NULL)
	{
		AkModulatorSubscriberInfo subscrInfo;
		subscrInfo.pSubscriber = this;
		subscrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_CAkLayer;
		subscrInfo.pTargetNode = m_pOwner;
		g_pModulatorMgr->GetModulators(subscrInfo, *in_pTriggerModulators);
	}

	return AK_Success;
}

AKRESULT CAkLayer::SetParamComplexFromRTPCManager( 
	void * in_pToken,
	AkRTPC_ParameterID in_Param_id, 
	AkRtpcID in_RTPCid,
	AkReal32 in_newValue,
	AkReal32 in_oldValue,
	const AkRTPCKey& in_rtpcKey,
	AkRTPCExceptionChecker* in_pExCheck
)
{
	AKASSERT( m_RTPCBitArray.IsSet( in_Param_id ) );
	AKASSERT( in_Param_id >= RTPC_Volume && in_Param_id <= RTPC_MakeUpGain );

	Notification(in_Param_id, in_newValue - in_oldValue, in_newValue, in_rtpcKey, in_pExCheck);

	return AK_Success;
}

void CAkLayer::RecalcNotification( bool in_bLiveEdit, bool in_bLog )
{
	for( AssociatedChildMap::Iterator iter = this->m_assocs.Begin(); iter != this->m_assocs.End(); ++iter )
	{
		if ( (*iter).item.m_pChild && (*iter).item.m_pChild->IsPlaying() )
		{
			(*iter).item.m_pChild->RecalcNotification( in_bLiveEdit, in_bLog );
		}
	}
}

void CAkLayer::AddToIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->m_idxLayers.SetIDToPtr( this );
}

void CAkLayer::RemoveFromIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->m_idxLayers.RemoveID( ID() );
}

AkUInt32 CAkLayer::AddRef() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxLayers.GetLock() ); 
    return ++m_lRef; 
} 

AkUInt32 CAkLayer::Release() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxLayers.GetLock() ); 
    AkInt32 lRef = --m_lRef; 
    AKASSERT( lRef >= 0 ); 
    if ( !lRef ) 
    { 
        RemoveFromIndex(); 
        AkDelete(AkMemID_Structure, this );
    } 
    return lRef; 
}

void CAkLayer::Notification( AkRTPC_ParameterID in_eType, AkReal32 in_fDelta, AkReal32 in_fValue, const AkRTPCKey& in_rtpcKey, AkRTPCExceptionChecker* in_pExceptCheck )
{
	NotifParams l_Params;
	l_Params.eType = in_eType;
	l_Params.bIsFromBus = false;
	l_Params.rtpcKey = in_rtpcKey;
	l_Params.fDelta = in_fDelta;
	l_Params.fValue = in_fValue;
	l_Params.pExceptCheck = in_pExceptCheck;
	ParamNotification( l_Params );
}

void CAkLayer::ParamNotification( NotifParams& in_rParams )
{
	// Since this is used only privately, we know the notification never comes
	// from a bus. If this ever changes, then look at what other bus-aware
	// classes do in that case.
	AKASSERT( ! in_rParams.bIsFromBus );

	AkDeltaMonitorObjBrace deltaBrace(key);

	for( AssociatedChildMap::Iterator iter = this->m_assocs.Begin(); iter != this->m_assocs.End(); ++iter )
	{
		if ( (*iter).item.m_pChild && (*iter).item.m_pChild->IsPlaying() )
		{
			(*iter).item.m_pChild->ParamNotification( in_rParams );
		}
	}
}

bool CAkLayer::IsPlaying() const
{
	for( AssociatedChildMap::Iterator iter = this->m_assocs.Begin(); iter != this->m_assocs.End(); ++iter )
	{
		if ( (*iter).item.m_pChild && (*iter).item.m_pChild->IsPlaying() )
		{
			// Just one playing associated object is enough
			return true;
		}
	}

	return false;
}

AKRESULT CAkLayer::SetInitialValues( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	//
	// Read ID
	//

	// We don't care about the ID, just skip it
	SKIPBANKDATA(AkUInt32, io_rpData, io_rulDataSize);

	//
	// Read RTPCs
	//

	AKRESULT eResult = SetInitialRTPC(io_rpData, io_rulDataSize, this);

	if ( eResult == AK_Success )
	{
		//
		// Read Crossfading Info
		//

		AkRtpcID rtpcID = (AkRtpcID)READBANKDATA(AkUInt32, io_rpData, io_rulDataSize);
		AkRtpcType rtpcType = (AkRtpcType)READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
		eResult = SetCrossfadingRTPC( rtpcID, rtpcType );
		if ( eResult == AK_Success )
		{
			//
			// Read Assocs
			//
	
			const AkUInt32 ulNumAssoc = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	
			if ( ulNumAssoc )
			{
				eResult = m_assocs.Reserve( ulNumAssoc );
				if ( eResult == AK_Success )
				{
					for( AkUInt32 i = 0; i < ulNumAssoc; ++i )
					{
						// Read the associated child's ID
						const AkUInt32 ulAssociatedChildID = READBANKDATA(AkUInt32, io_rpData, io_rulDataSize);
	
						// Read the crossfading curve
						AkUInt32 ulCurveSize = READBANKDATA(AkUInt32, io_rpData, io_rulDataSize);
	
						// Create the assoc
						eResult = SetChildAssoc( ulAssociatedChildID, (AkRTPCGraphPoint*)io_rpData, ulCurveSize );
	
						if ( eResult != AK_Success )
						{
							// Stop loading as soon as something doesn't work
							break;
						}
	
						// Skip the Conversion array that was processed by SetChildAssoc() above
						io_rpData += ( ulCurveSize * sizeof(AkRTPCGraphPoint) );
						io_rulDataSize -= ( ulCurveSize * sizeof(AkRTPCGraphPoint) );
					}
				}
			}
		}
	}

	return eResult;
}

AKRESULT CAkLayer::CanAssociateChild( CAkParameterNodeBase * in_pAudioNode )
{
	AKRESULT eResult = AK_Fail;

	AKASSERT( in_pAudioNode );

	if ( m_pOwner && in_pAudioNode->Parent() )
	{
		// We have an Owner and the object has a parent: It must be the same object
		if ( in_pAudioNode->Parent() == m_pOwner )
			eResult = AK_Success;
	}
	else
	{
		// Either we don't yet have an owner or the object doesn't yet
		// have a parent. That's Ok, we'll try again later when we are
		// assigned an owner or when the object becomes a child of the
		// container (in the meantime, we will not connect to the object)
		eResult = AK_PartialSuccess;
	}

	return eResult;
}

AKRESULT CAkLayer::SetChildAssoc(
	AkUniqueID in_ChildID,
	AkRTPCGraphPoint* in_pCrossfadingCurve,
	AkUInt32 in_ulCrossfadingCurveSize
)
{
	AKRESULT eResult = AK_Fail;

	CAssociatedChildData* pAssoc = NULL;

	// Get/Create the assoc
	AssociatedChildMap::Iterator itFind = m_assocs.FindEx( in_ChildID );
	if ( itFind != m_assocs.End() )
	{
		// The assoc already exists, we will just update it
		pAssoc = &((*itFind).item);
		eResult = AK_Success;
	}
	else
	{
		// Create a new Assoc
		pAssoc = m_assocs.Set( in_ChildID );
		if ( pAssoc )
		{
			// Initialize the Assoc
			eResult = pAssoc->Init( this, in_ChildID );
			if ( eResult != AK_Success )
			{
				// Forget it
				m_assocs.Unset( in_ChildID );
				pAssoc = NULL;
			}
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	if ( eResult == AK_Success )
	{
		// Set the new curve (this will first clear the existing curve if any)
		if ( in_ulCrossfadingCurveSize > 0 )
			eResult = pAssoc->m_fadeCurve.Set( in_pCrossfadingCurve, in_ulCrossfadingCurveSize, AkCurveScaling_None );
		else
			pAssoc->m_fadeCurve.Unset();

		eResult = pAssoc->UpdateActiveRange();

		// If this failed, then we just don't have a crossfading curve for that child, but
		// we still have the assoc (so RTPCs etc will work)

		// Notify even if Set() failed, since any existing curve was lost
		if ( pAssoc->m_pChild )
			pAssoc->m_pChild->RecalcNotification( false );
	}

	return eResult;
}

AKRESULT CAkLayer::UnsetChildAssoc(
	AkUniqueID in_ChildID 
)
{
	AKRESULT eResult = AK_InvalidInstanceID;

	AssociatedChildMap::Iterator itFind = m_assocs.FindEx( in_ChildID );
	if ( itFind != m_assocs.End() )
	{
		// WG-43704 We should not delete the element from m_assocs since we might need it when reloading the banks 
		(*itFind).item.m_pChild = NULL;
		eResult = AK_Success;
	}

	return eResult;
}

void CAkLayer::SetOwner( CAkLayerCntr* in_pOwner )
{
	AKASSERT( ! m_pOwner || ! in_pOwner );

	if ( m_pOwner )
	{
		// Disconnect all assocs
		for( AssociatedChildMap::Iterator iter = m_assocs.Begin(); iter != m_assocs.End(); ++iter )
		{
			(*iter).item.ClearChildPtr( this );
		}
	}

	m_pOwner = in_pOwner;

	if ( m_pOwner )
	{
		// Try to connect all assocs
		for( AssociatedChildMap::Iterator iter = m_assocs.Begin(); iter != m_assocs.End(); ++iter )
		{
			(*iter).item.UpdateChildPtr( this );
		}
	}
}

void CAkLayer::UpdateChildPtr( AkUniqueID in_ChildID )
{
	AssociatedChildMap::Iterator itFind = m_assocs.FindEx( in_ChildID );
	if ( itFind != m_assocs.End() )
	{
		(*itFind).item.UpdateChildPtr( this );
	}
}

void CAkLayer::ClearChildPtr( AkUniqueID in_ChildID )
{
	AssociatedChildMap::Iterator itFind = m_assocs.FindEx( in_ChildID );
	if ( itFind != m_assocs.End() )
	{
		(*itFind).item.ClearChildPtr( this );
	}
}

AKRESULT CAkLayer::SetCrossfadingRTPC( AkRtpcID in_rtpcID, AkRtpcType in_rtpcType )
{
	AKRESULT eResult = AK_Success;

	if ( m_crossfadingRTPCID != in_rtpcID )
	{
		if ( m_crossfadingRTPCID )
			g_pRTPCMgr->UnregisterLayer( this, m_crossfadingRTPCID );

		m_crossfadingRTPCID = in_rtpcID;
		m_crossfadingRTPCType = in_rtpcType;

		if ( m_crossfadingRTPCID )
		{
			eResult = g_pRTPCMgr->RegisterLayer( this, m_crossfadingRTPCID, m_crossfadingRTPCType );

			if ( eResult != AK_Success )
				m_crossfadingRTPCID = 0;
		}

		RecalcNotification( false );
	}

	return eResult;
}

struct _LayerMuteStruct
{
	AkMutedMapItem item;
	CAkConversionTable* pCurve;
	AkReal32 fValue;
	AkReal32 fMute;
	bool bMuteCalculated;
};

static void _LayerMuteFunc(
  	CAkPBI * in_pPBI,
	const AkRTPCKey &,
	void * in_pCookie 
	)
{		
	_LayerMuteStruct * pMuteStruct = (_LayerMuteStruct *) in_pCookie;
	if ( !pMuteStruct->bMuteCalculated )
	{
		pMuteStruct->fMute = pMuteStruct->pCurve->Convert( pMuteStruct->fValue );
		pMuteStruct->bMuteCalculated = true;
	}
	in_pPBI->MuteNotification( pMuteStruct->fMute, pMuteStruct->item, true );
}

void CAkLayer::OnRTPCChanged(const AkRTPCKey & in_rtpcKey, AkReal32 in_fOldValue, AkReal32 in_fNewValue, AkRTPCExceptionChecker* in_pExceptCheck)
{
	// We register for notifications only when we have a m_crossfadingRTPCID, so
	// this should never be called unless we do have a valid m_crossfadingRTPCID.
	AKASSERT( m_crossfadingRTPCID && g_pRTPCMgr );

	if ( ! m_assocs.IsEmpty() )
	{
		_LayerMuteStruct muteStruct;
        muteStruct.item.m_bIsPersistent = false;
		muteStruct.item.m_bIsGlobal = ( in_rtpcKey.GameObj() == NULL );
		muteStruct.item.m_Identifier = this;
		muteStruct.fValue = in_fNewValue;		
		muteStruct.item.m_eReason = AkDelta_RTPC;

		////////////////////////////////////////////////////////
		// WG-44698 - Crash in CAkLayer::OnRTPCChanged PART 1
		//
		// While in the loop to update the muting level, if one child was already unloaded but technically playing (bankwise), 
		// pending items will be potentially requested to stop.
		// Normally stopped items in this loop are destroyed later on at the end of the audio frame (so nothing is deleted right away).
		// But in one specific case where the only playing item was a "pending item" (in the case we had it was a delayed/trigger rate container command that was holding the last reference)
		// The association gets removed right away while we are already iterating in this array.
		//
		// Current workaround:
		//   1- Addref "this". (Because if the association to be removed was the last one, "this" will also be destroyed as a direct side effect)
		//   2- Addref all associated child
		//   3- Do the Mute Update
		//   4- Release all associated child 
		//   5- Release "this"
		//
		// This fix should prevent any crash with a minimal cost.
		// We should rework the system to not depend on the structure of m_assocs being an array.
		AddRef();
		for (AssociatedChildMap::Iterator iter = m_assocs.Begin(); iter != m_assocs.End(); ++iter)
		{
			if (iter.pItem->item.m_pChild)
				iter.pItem->item.m_pChild->AddRef();
		}
		// WG-44698 - Crash in CAkLayer::OnRTPCChanged END of PART 1
		////////////////////////////////////////////////////////

		// Update the muting level for all our associated objects, based on the new
		// value of the Game Parameter.
		for( AssociatedChildMap::Iterator iter = m_assocs.Begin(); iter != m_assocs.End(); ++iter )
		{
			if (iter.pItem->item.m_pChild)
			{				
				// Active voice mgmt
				if (m_pOwner->m_bIsContinuousValidation)
				{
					bool bPrevMute = iter.pItem->item.IsOutsideOfActiveRange(in_fOldValue);
					bool bNextMute = iter.pItem->item.IsOutsideOfActiveRange(in_fNewValue);

					if (bPrevMute != bNextMute)
					{
						for (CAkLayerCntr::AkListContPlayback::Iterator itCont = m_pOwner->m_listContPlayback.Begin(); itCont != m_pOwner->m_listContPlayback.End(); ++itCont)
						{
							CAkLayerCntr::ContPlaybackItem * pItem = *itCont;

							if (pItem->rtpcKey.MatchValidFields(in_rtpcKey)
								&& (in_pExceptCheck == NULL || !in_pExceptCheck->IsException(pItem->rtpcKey)))
							{
								CAkLayerCntr::ContPlaybackChildMuteCount * pMute = pItem->childMuteCount.Exists(iter.pItem->item.m_ulChildID);
								if (pMute)
								{
									if (bPrevMute && !bNextMute)
									{
										if (--pMute->uCount == 0)
										{
											TransParams tparams;
											tparams.eFadeCurve = AkCurveInterpolation_Linear;
											tparams.TransitionTime = 0;

											AkPBIParams pbiParams;

#ifndef AK_OPTIMIZED
											pbiParams.SetPlayTargetID(pItem->playTargetID);
											pbiParams.bPlayDirectly = pItem->bPlayDirectly;
#endif

											pbiParams.eType = AkPBIParams::PBI;
											pbiParams.pInstigator = m_pOwner;
											pbiParams.bIsFirst = true;

											pbiParams.pGameObj = pItem->rtpcKey.GameObj();
											pbiParams.pTransitionParameters = &tparams;
											pbiParams.userParams = pItem->UserParameters;
											pbiParams.ePlaybackState = pItem->ePlaybackState;
											pbiParams.uFrameOffset = 0;

											pbiParams.pContinuousParams = NULL;
											pbiParams.sequenceID = AK_INVALID_SEQUENCE_ID;
											pbiParams.bNeedsFadeIn = true;

											//ANTI Delta brace.  
											//While it is worthwhile to have all RTPC changes in one packet, when a Blend container is affected, sounds may start.  
											//So Post what we have and let the normal sound-startup parameter collection go.
											AkDeltaMonitor::CloseObjectBrace();
											AkDeltaMonitor::CloseMultiTargetBrace();											
											iter.pItem->item.m_pChild->Play(pbiParams);											
											AkDeltaMonitor::OpenMultiTargetBrace(AkDelta_RTPC, m_crossfadingRTPCID, in_fNewValue);
											AkDeltaMonitor::OpenObjectBrace(key);
										}

									}
									else if (!bPrevMute && bNextMute)
									{
										if (++pMute->uCount == 1)
										{
											ActionParams l_Params;
											l_Params.bIsFromBus = false;
											l_Params.bIsMasterResume = false;
											l_Params.eType = ActionParamType_Stop;
											l_Params.pGameObj = pItem->rtpcKey.GameObj();
											l_Params.playingID = pItem->rtpcKey.PlayingID();
											l_Params.transParams.eFadeCurve = AkCurveInterpolation_Linear;
											l_Params.transParams.TransitionTime = 0;
											l_Params.bIsMasterCall = false;
											iter.pItem->item.m_pChild->ExecuteAction(l_Params);
											g_pAudioMgr->StopPendingAction(iter.pItem->item.m_pChild, pItem->rtpcKey.GameObj(), pItem->rtpcKey.PlayingID());
										}
									}
								}
							}
						}
					}
				}

				if (iter.pItem->item.m_pChild->IsPlaying())
				{
					muteStruct.bMuteCalculated = false;
					muteStruct.pCurve = &iter.pItem->item.m_fadeCurve;

					iter.pItem->item.m_pChild->ForAllPBI(
						_LayerMuteFunc,
						in_rtpcKey,
						&muteStruct);
				}
			}
		}

		////////////////////////////////////////////////////////
		// WG-44698 - Crash in CAkLayer::OnRTPCChanged PART 2
		// m_assocs is a CAkKeyArray, we can for now iterate backward on it.
		// This is because every call to release could potentially remove the item from the array.
		// This fix depends on the fact m_assocs is an array. Will need a better fix.
		AkInt32 arraysize = m_assocs.Length();
		for (AkInt32 index = arraysize - 1; index >= 0; --index)
		{
			if (m_assocs[index].item.m_pChild)
				m_assocs[index].item.m_pChild->Release();
		}
		Release();
		// WG-44698 - Crash in CAkLayer::OnRTPCChanged END of PART 2
		////////////////////////////////////////////////////////
	}
}

//////////////////////////////////////////////////////////////////////
// CAkLayer::CAssociatedChildData
//////////////////////////////////////////////////////////////////////

CAkLayer::CAssociatedChildData::CAssociatedChildData()
	: m_ulChildID( 0 )
	, m_pChild( NULL )
	, m_pActiveRanges( NULL )
{
}

CAkLayer::CAssociatedChildData::~CAssociatedChildData()
{
}

AKRESULT CAkLayer::CAssociatedChildData::Init( CAkLayer* in_pLayer, AkUniqueID in_ulAssociatedChildID )
{
	AKASSERT( ! m_pChild && ! m_ulChildID );

	// Remember the object's ID
	m_ulChildID = in_ulAssociatedChildID;

	// Try to get a pointer to the object. It's Ok if it doesn't exist yet,
	// maybe UpdateChildPtr() will be called again layer. Note that
	// UpdateChildPtr() doesn't fail if the object doesn't exist because
	// it's not an error.
	return UpdateChildPtr( in_pLayer );
}

AKRESULT CAkLayer::CAssociatedChildData::Term( CAkLayer* in_pLayer )
{
	AKRESULT eResult = ClearChildPtr( in_pLayer );

	// Forget the object
	m_pChild = NULL;
	m_ulChildID = 0;

	m_fadeCurve.Unset();
	
	if (m_pActiveRanges)
	{
		m_pActiveRanges->Term();
		AkDelete(AkMemID_Structure, m_pActiveRanges);
		m_pActiveRanges = NULL;
	}

	return eResult;
}

AKRESULT CAkLayer::CAssociatedChildData::UpdateActiveRange()
{
	AKRESULT eResult = AK_Success;

	if (m_pActiveRanges)
		m_pActiveRanges->RemoveAll();
	
	if (m_fadeCurve.Size() >= 2)
	{
		// WG-43083_ContinuousBlend_Dynamic_ActiveRange
		// We need to maintain multiple active ranges for single sounds, otherwise the continuous optimization of not creating the voices when
		// out of the active range does not optimize when one sound has dead zones.

		if (!m_pActiveRanges)
		{
			m_pActiveRanges = AkNew(AkMemID_Structure, AkActiveRangeArray);
			if (!m_pActiveRanges)
			{
				return AK_InsufficientMemory;
			}
		}
		
		RangeSet fullRange; // Constructor sets it to full range.

		bool bActive = false;
		for (AkUInt32 i = 0; i < m_fadeCurve.Size(); ++i)
		{
			const AkRTPCGraphPoint& point = m_fadeCurve.Point(i);
			if (!bActive)
			{
				if (point.To > 0 || point.Interp != AkCurveInterpolation_Constant)
				{
					fullRange.m_fActiveRangeStart = point.From;
					bActive = true;
				}
			}
			else
			{
				if (point.To <= 0)
				{
					fullRange.m_fActiveRangeEnd = point.From;
					bActive = false;
					if (!m_pActiveRanges->AddLast(fullRange))
					{
						eResult = AK_InsufficientMemory;
						break;
					}
				}
			}
		}
		if (bActive)
		{
			// The last point was the end of the active zone, push it.
			fullRange.m_fActiveRangeEnd = m_fadeCurve.GetLastPoint().From;
			if (!m_pActiveRanges->AddLast(fullRange))
			{
				eResult = AK_InsufficientMemory;
			}
		}

	}
	else
	{
		// No activity Curves means always active.
	}

	return eResult;
}

bool CAkLayer::CAssociatedChildData::IsOutsideOfActiveRange(AkReal32 in_fValue)
{
	if (!m_pActiveRanges || m_pActiveRanges->Length() == 0)
		return false;// No activity curve means always active.

	for (AkActiveRangeArray::Iterator iter = m_pActiveRanges->Begin(); iter != m_pActiveRanges->End(); ++iter)
	{
		// if it is within any of the active range, it is active.
		if (in_fValue >= (*iter).m_fActiveRangeStart && in_fValue <= (*iter).m_fActiveRangeEnd)
		{
			return false;
		}
	}

	return true;
}

AKRESULT CAkLayer::CAssociatedChildData::UpdateChildPtr( CAkLayer* in_pLayer )
{
	AKASSERT( in_pLayer && m_ulChildID );

	if ( m_pChild )
	{
		// Nothing to do
		AKASSERT( m_pChild->ID() == m_ulChildID );
		return AK_Success;
	}

	// We start with AK_Success because we don't want to fail if
	// the object doesn't exist -- that's a normal case (for example if
	// loading Soundbanks that contain only some of the children of
	// a container).
	AKRESULT eResult = AK_Success;

	CAkParameterNodeBase* pAudioNode = g_pIndex->GetNodePtrAndAddRef( m_ulChildID, AkNodeType_Default );
	if ( pAudioNode )
	{
		eResult = in_pLayer->CanAssociateChild( pAudioNode );
		if ( eResult == AK_Success )
		{
			// We can safely cast since all associated objects are children
			// of a Layer Container, and they are all CAkParameterNodes. The
			// only exception would be if there's an ID mismatch and pAudioNode is
			// not the expected object, but this might happen only if Soundbanks
			// are really messed up.
			m_pChild = static_cast<CAkParameterNode*>( pAudioNode );

			eResult = m_pChild->AssociateLayer( in_pLayer );
			if ( eResult != AK_Success )
			{
				m_pChild = NULL;
			}
		}
		else if ( eResult == AK_PartialSuccess )
		{
			// The child might be valid, but the layer doesn't have an owner yet
			// so we can't really tell. We'll just wait until the Layer has an
			// owner and we'll try again.
			eResult = AK_Success;
		}
		pAudioNode->Release();
	}

	return eResult;
}

AKRESULT CAkLayer::CAssociatedChildData::ClearChildPtr( CAkLayer* in_pLayer )
{
	AKRESULT eResult = AK_Success;

	if ( m_pChild )
	{
		eResult = m_pChild->DissociateLayer( in_pLayer );
		m_pChild = NULL;
	}

	return eResult;
}
