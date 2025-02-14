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
// AkLayer.h
//
// Declaration of the CAkLayer class, which represents a Layer
// in a Layer Container (CAkLayerCntr)
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKLAYER_H_
#define _AKLAYER_H_

#include "AkIndexable.h"
#include "AkConversionTable.h"
#include "AkCommon.h"                                  // for AkModulatorParamXfrmArray
#include "AkModulatorMgr.h"                            // for AkModulatorTriggerParams (ptr only), AkModulatorsToTrigger
#include <AK/Tools/Common/AkArray.h>
#include <float.h>

class AkMutedMap;
struct NotifParams;
class CAkParameterNode;
class CAkLayerCntr;
class AkSoundParams;


// Represents a Layer in a Layer Container (CAkLayerCntr)
class CAkLayer
	: public CAkIndexable
{
public:

    virtual ~CAkLayer();

	// Thread safe version of the constructor
	static CAkLayer* Create( AkUniqueID in_ulID = 0 );

	AKRESULT Init();

	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

	// Read data from Soundbank
	AKRESULT SetInitialValues( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );
	
	void TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData ) const;

	// Return - AKRESULT - AK_Success if succeeded
	AKRESULT GetModulatorParamXfrms(
		AkModulatorParamXfrmArray&	io_paramsXforms,
		AkRtpcID					in_modulatorID,		
		const CAkRegisteredObj *	in_GameObject
		) const;

	// Get audio parameters for the specified associated child
	AKRESULT GetAudioParameters(
		CAkParameterNode*	in_pAssociatedChild,	// Associated child making the call
		AkSoundParams		&io_Parameters,			// Set of parameter to be filled		
		AkMutedMap&			io_rMutedMap,			// Map of muted elements
		const AkRTPCKey&	in_rtpcKey,				// Key to retrieve rtpc parameters
		AkModulatorsToTrigger* out_pModulators
	);

	// Set RTPC info
	AKRESULT SetRTPC(
		AkRtpcID					in_RTPC_ID,
		AkRtpcType					in_RTPCType,
		AkRtpcAccum					in_RTPCAccum,
		AkRTPC_ParameterID			in_ParamID,
		AkUniqueID					in_RTPCCurveID,
		AkCurveScaling				in_eScaling,
		AkRTPCGraphPoint*			in_pArrayConversion,		// NULL if none
		AkUInt32					in_ulConversionArraySize,	// 0 if none
		bool						in_bNotify = true
	);

	// Clear RTPC info
	void UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID );

	// Called by the RTPCMgr when the value of a Game Parameter changes
	AKRESULT SetParamComplexFromRTPCManager( 
		void * in_pToken,
		AkRTPC_ParameterID in_Param_id, 
		AkRtpcID in_RTPCid,
		AkReal32 in_newValue,
		AkReal32 in_oldValue, 
		const AkRTPCKey& in_rtpcKey,
		AkRTPCExceptionChecker* in_pExCheck = NULL
	);

	// Notify associated objects that they must RecalcNotification()
	void RecalcNotification( bool in_bLiveEdit, bool in_bLog = false );

	// Is any of the associated objects currently playing?
	bool IsPlaying() const;

	// Set information about an associated object
	AKRESULT SetChildAssoc(
		AkUniqueID in_ChildID,
		AkRTPCGraphPoint* in_pCrossfadingCurve,	// NULL if none
		AkUInt32 in_ulCrossfadingCurveSize		// 0 if none
	);

	// Clear information about an associated object
	AKRESULT UnsetChildAssoc(
		AkUniqueID in_ChildID 
	);

	// Can the specified object be associated to this layer?
	AKRESULT CanAssociateChild( CAkParameterNodeBase * in_pAudioNode );

	// Set this Layer's owner (NULL to clear it)
	void SetOwner( CAkLayerCntr* in_pOwner );
	const CAkLayerCntr* GetOwner() const { return m_pOwner; }

	// If the layer is associated to the specified child, get
	// the pointer to that child. This is called when a child
	// is added to the container, so child-layer associations can
	// be made on the fly.
	void UpdateChildPtr( AkUniqueID in_ChildID );

	// If the layer is associated to the specified child, clear
	// the pointer to the child. This is called when the child
	// is being removed.
	void ClearChildPtr( AkUniqueID in_ChildID );

	// Set the Unique ID of the Game Parameter to be used for crossfading.
	// 0 means there's no crossfading.
	AKRESULT SetCrossfadingRTPC( AkRtpcID in_rtpcID, AkRtpcType in_rtpcType );

	void OnRTPCChanged(const AkRTPCKey & in_rtpcKey, AkReal32 in_fOldValue, AkReal32 in_fNewValue, AkRTPCExceptionChecker* in_pExceptCheck);


public:
	// NOTE: Although it is used only inside CAkLayer, CAssociatedChildData must be public in order to 
	// avoid private nested class access using the CAkKeyList.

	// Object representing a child-layer association
	class CAssociatedChildData
	{
	public:

		// Constructor/Destructor
		CAssociatedChildData();
		~CAssociatedChildData();

		// Init/Term
		AKRESULT Init( CAkLayer* in_pLayer, AkUniqueID in_ulAssociatedChildID );
		AKRESULT Term( CAkLayer* in_pLayer );

		// If the child pointer is NULL, try to get it and
		// associate the layer to the child.
		AKRESULT UpdateChildPtr( CAkLayer* in_pLayer );

		// If the child pointer is not NULL, dissociate the
		// layer from the child and nullify the pointer
		AKRESULT ClearChildPtr( CAkLayer* in_pLayer );

		bool IsOutsideOfActiveRange(AkReal32 in_fValue);

		// Data
		AkUniqueID m_ulChildID;
		CAkParameterNode* m_pChild;
		CAkConversionTable m_fadeCurve;

		// Helper to properly re-compute the active range
		AKRESULT UpdateActiveRange();

	private:
		struct RangeSet
		{
			RangeSet():
				m_fActiveRangeStart(-FLT_MAX),
				m_fActiveRangeEnd(FLT_MAX)
			{}

			AkReal32 m_fActiveRangeStart;
			AkReal32 m_fActiveRangeEnd;
		};

		typedef AkArray<RangeSet, const RangeSet &, ArrayPoolDefault> AkActiveRangeArray;
		AkActiveRangeArray* m_pActiveRanges;
	};

private:
	friend class CAkLayerCntr;

	// Constructor/Destructor
    CAkLayer( AkUniqueID in_ulID );

	// Indexing
	void AddToIndex();
	void RemoveFromIndex();

	// Notify the Children PBIs that a variation occured
	void Notification(
		AkRTPC_ParameterID in_eType,
		AkReal32 in_fDelta,
		AkReal32 in_fValue,
		const AkRTPCKey& in_rtpcKey,
		AkRTPCExceptionChecker* in_pExCheck = NULL
		);

	void ParamNotification( NotifParams& in_rParams );
	
	// This layer's associations
	typedef CAkKeyArray<AkUniqueID, CAssociatedChildData> AssociatedChildMap;
	AssociatedChildMap m_assocs;

	// Parameters with RTPCs have their bit set to 1 in this array
	AkRTPCBitArray m_RTPCBitArray;

	// The container that owns this layer
	CAkLayerCntr* m_pOwner;

	// Game Parameter to use for crossfading
	AkRtpcID m_crossfadingRTPCID;
	AkRtpcType m_crossfadingRTPCType;
};

#endif // _AKLAYER_H_
