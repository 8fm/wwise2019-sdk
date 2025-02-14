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
// AkRTPCMgr.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _RTPC_MGR_H_
#define _RTPC_MGR_H_

#include "AkRTPC.h"
#include <AK/Tools/Common/AkHashList.h>
#include <AK/SoundEngine/Common/IAkRTPCSubscriber.h>
#include "AkConversionTable.h"
#include "AudiolibDefs.h"
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkKeyArray.h>
#include "AkMultiKeyList.h"
#include "ITransitionable.h"
#include "PrivateStructures.h"
#include <AK/Tools/Common/AkListBareLight.h>
#include "AkMidiStructs.h"
#include "AkTransition.h"
#include "AkRTPCKey.h"
#include "AkRegisteredObj.h" // for IsPositionCached() in ASSERT()
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkDeltaMonitor.h"

class CAkRegisteredObj;
class CAkSwitchAware;

#define DEFAULT_RTPC_VALUE 0
#define DEFAULT_SWITCH_TYPE 0

// AkRTPCExceptionChecker
//	This simple class is passed along as a functor/closure when updating RTPC values.  If the AkRTPCExceptionChecker 
//		is available (non-null) the client must call IsException() to determine whether the key passed in is an 
//		exception to the value being set.  These exceptions are necessary to prevent wider-scoped rtpc values from 
//		overriding narrower-scoped values, when they exist.
class AkRTPCExceptionChecker // - IsException interface
{
public:
	virtual bool IsException(const AkRTPCKey& in_keyToCheck)=0;
};

//Typed version of AkRTPCExceptionChecker to support different value tree types.
template<typename T_TREE>
class AkRTPCExceptionChecker_: public AkRTPCExceptionChecker
{
public:
	AkRTPCExceptionChecker_(	const AkRTPCKey& in_keyBeingSet, const T_TREE& in_valueTree ): m_keyBeingSet(in_keyBeingSet), m_valueTree(in_valueTree) {}
	virtual bool IsException(const AkRTPCKey& in_keyToCheck) {  return m_valueTree.CheckException( m_keyBeingSet, in_keyToCheck ); }
private:
	const AkRTPCKey& m_keyBeingSet;
	const T_TREE& m_valueTree;
};

//Struct definitions

struct AkRTPCSubscriptionKey
{
	void*				pSubscriber;	// Cast to appropriate interface/class depending on eType
	AkRTPC_ParameterID	ParamID;

	bool operator==( const AkRTPCSubscriptionKey & in_other ) const
	{
		return pSubscriber == in_other.pSubscriber && 
			   ParamID == in_other.ParamID;
	}
};

inline AkHashType AkHash(AkRTPCSubscriptionKey in_key)
{ 
	return (AkHashType) ((AkUIntPtr)in_key.pSubscriber + (AkUIntPtr)in_key.ParamID);
}

struct AccumulateAdd{
	inline static float BaseValue() { return 0.0f; }
	template <typename T> inline static void Accumulate(T& in_acc, const T& in_rhs) { in_acc += in_rhs; }
};
struct AccumulateMult{
	inline static float BaseValue() { return 1.0f; }
	template <typename T> inline static void Accumulate(T& in_acc, const T& in_rhs) { in_acc *= in_rhs; }
};

struct CurrentValue {
	static bool GetModulatorValue( AkRtpcID in_RTPC_ID, AkRTPCKey& io_rtpcKey, AkReal32& out_value );
	static bool ModulatorSupportsAutomatedParams(AkRtpcID in_RTPC_ID);
};

class CAkLayer;

// CAkRTPCMgr Class
// Unique per instance of Audiolib, the RTPC manager owns the real 
// time parameters and notified those who wants to be
class CAkRTPCMgr
{	
public:
	
	enum SubscriberType
	{
		// CAREFUL! Subscriber type order also dictates notification order!  Therefore PBIs are notified first, and so on.
		// This prevents dependency issues caused by the start/stop of sounds due to Blend and Switch containers.
		SubscriberType_PBI = 0,
		SubscriberType_Plugin = 1,
		SubscriberType_CAkParameterNodeBase = 2,
		SubscriberType_Modulator = 3,
		SubscriberType_CAkLayer = 4,
		SubscriberType_CAkCrossfadingLayer = 5, // Special case to ensure that the cross RTPC is always processed after the other RTPCs WG-32867 Must stay after SubscriberType_CAkLayer
		SubscriberType_SwitchGroup = 6
	};

	// A single RTPC Curve
	struct RTPCCurve
	{
		AkUniqueID RTPCCurveID;
		AkRtpcID RTPC_ID;
		CAkConversionTable ConversionTable;

		bool operator == ( const RTPCCurve& in_other ) const
		{
			return in_other.RTPCCurveID == RTPCCurveID;
		}
	};

	// Multiple RTPC Curves
	typedef AkArray<RTPCCurve, const RTPCCurve&, ArrayPoolDefault> RTPCCurveArray;

	// Each subscription, for a given Subscriber/Property/Game Object, now has
	// a list of Curves
	struct AkRTPCSubscription
	{
		// For AkMapRTPCSubscribers
		AkRTPCSubscriptionKey key; 
		AkRTPCSubscription * pNextItem;

		// Used by effects and PBIs to subscribe only to a particular gameobj/channel/etc
		AkRTPCKey		TargetKey;

		SubscriberType		eType;
		AkRtpcAccum			eAccum;
		RTPCCurveArray		Curves;

		~AkRTPCSubscription()
		{
			AKASSERT( Curves.IsEmpty() );
			Curves.Term();
		}

		void PushUpdate(
			AkRtpcID idRTPC, 
			AkReal32 in_OldValue, 
			AkReal32 in_NewValue, 
			const AkRTPCKey& rtpcKey, 
			AkRTPCExceptionChecker* exChecker, 
			AkRTPCValueTree* values ) const;

		AkReal32 ConvertCurves(AkRtpcID idRTPC, AkReal32 in_NewValue) const;

		void GetParamValue( AkRtpcID in_rtpcId, AkReal32 in_fRtpc, AkReal32& out_fValue ) const;
		void GetParamValues(AkRtpcID in_rtpcId, AkReal32 in_newRtpc, AkReal32 in_oldRtpc, AkReal32& out_newValue, AkReal32& out_oldValue) const;

		template < typename Accumulator >
		void _GetParamValue(AkRtpcID in_rtpcId, AkReal32 in_fRtpc, AkReal32& out_fValue) const
		{
	        out_fValue = Accumulator::BaseValue();
			for( RTPCCurveArray::Iterator it = Curves.Begin(); it != Curves.End(); ++it )
			{
				if( it.pItem->RTPC_ID == in_rtpcId )
				{
					Accumulator::Accumulate( out_fValue, it.pItem->ConversionTable.Convert( in_fRtpc ) );
				}
			}
		}

		template < typename Accumulator >
		void _GetParamValues(AkRtpcID in_rtpcId, AkReal32 in_newRtpc, AkReal32 in_oldRtpc, AkReal32& out_newValue, AkReal32& out_oldValue) const
		{
	        out_newValue = Accumulator::BaseValue();
			out_oldValue = Accumulator::BaseValue();
			for( RTPCCurveArray::Iterator it = Curves.Begin(); it != Curves.End(); ++it )
			{
				if( it.pItem->RTPC_ID == in_rtpcId )
				{
					Accumulator::Accumulate( out_oldValue, it.pItem->ConversionTable.Convert( in_oldRtpc ) );
					Accumulator::Accumulate( out_newValue, it.pItem->ConversionTable.Convert( in_newRtpc ) );
				}
			}
		}

		typedef AkListBare<AkRTPCSubscription,AkListBareNextItem,AkCountPolicyNoCount,AkLastPolicyNoLast> List;

	private:
		bool operator == ( const AkRTPCSubscription& in_other );
	};

	struct AkSubsPtr
	{	
		AkSubsPtr(): pSubs(NULL){};
		AkSubsPtr(AkRTPCSubscription * in_p): pSubs(in_p){};
		AkRTPCSubscription * pSubs;
	
		AkForceInline bool operator<(const AkSubsPtr& b) const
		{
			return pSubs->eType < b.pSubs->eType || (pSubs->eType == b.pSubs->eType && pSubs < b.pSubs);
		}

		AkForceInline bool operator>(const AkSubsPtr& b) const
		{
			return pSubs->eType > b.pSubs->eType || (pSubs->eType == b.pSubs->eType && pSubs > b.pSubs);
		}

		AkForceInline bool operator==(const AkSubsPtr& b) const
		{
			return pSubs == b.pSubs;
		}

		AkForceInline bool operator==(const AkRTPCSubscription * b) const
		{
			return pSubs == b;
		}

		AkForceInline const AkSubsPtr& operator=(const AkSubsPtr& b)
		{
			pSubs = b.pSubs;
			return *this;
		}

		AkForceInline AkRTPCSubscription& operator*() const
		{
			return *pSubs;
		}

		static AkForceInline AkSubsPtr& Get(AkSubsPtr& in_item)
		{
			return in_item;
		}
	};

	typedef AkSortedKeyArray<AkSubsPtr, AkSubsPtr, ArrayPoolDefault, AkSubsPtr> AkRTPCSubscriptions;

	friend class AkMonitor;

private:

	// Helpers
	AKRESULT UpdateRTPCSubscriberInfo( void* in_pSubscriber );

	void UpdateSubscription(
		AkRTPCSubscription& in_rSubscription,
		AkRtpcID in_RtpcID = AK_INVALID_UNIQUE_ID
		);


public:
	CAkRTPCMgr();
	~CAkRTPCMgr();

	AKRESULT Init();

	AKRESULT Term();

	///////////////////////////////////////////////////////////////////////////
	// Main SET func
	///////////////////////////////////////////////////////////////////////////

	AKRESULT AKSOUNDENGINE_API SetRTPCInternal( 
		AkRtpcID in_RTPCid, 
		AkReal32 in_Value, 
		CAkRegisteredObj * in_pGameObj,
		AkPlayingID in_playingID,
		TransParamsBase & in_transParams,
		AkValueMeaning in_eValueMeaning	= AkValueMeaning_Independent	// AkValueMeaning_Independent (absolute) or AkValueMeaning_Offset (relative)
		);

	AKRESULT AKSOUNDENGINE_API SetRTPCInternal(
		AkRtpcID in_RTPCid, 
		AkReal32 in_Value, 
		const AkRTPCKey& rtpcKey,
		TransParamsBase & in_transParams,
		AkValueMeaning in_eValueMeaning	= AkValueMeaning_Independent,	// AkValueMeaning_Independent (absolute) or AkValueMeaning_Offset (relative)
		bool in_bBypassInternalRampingIfFirst = false
		);

	AKRESULT SetMidiParameterValue(
		AkAssignableMidiControl in_eParam, 
		AkReal32 in_Value, 
		const AkRTPCKey& rtpcKey
		);

	void SetMidiParameterDefaultValue(
		AkAssignableMidiControl in_eParam, 
		AkReal32 in_DefaultValue
		);

///////////////////////////////////////////////////////////////////////////
	// Main GET func
///////////////////////////////////////////////////////////////////////////

	template<typename Monitor>
	AkReal32 GetRTPCConvertedValue(
		void*					in_pSubscriber,
		AkUInt32				in_ParamID,
		const AkRTPCKey&		in_rtpcKey);

	AkReal32 GetRTPCConvertedValue(
		void*					in_pSubscriber,
		AkUInt32				in_ParamID,
		const AkRTPCKey&		in_rtpcKey)
	{
		return GetRTPCConvertedValue<AkDeltaMonitorDisabled>(in_pSubscriber, in_ParamID, in_rtpcKey);
	}
	
	AkReal32 GetRTPCConvertedValue(
		void*					in_pSubscriber,
		const AkRTPCKey&		in_rtpcKey
		);

	template< typename Accumulate, typename GetValuePolicy, typename Monitor >
	AkReal32 GetRTPCConvertedValue(
		void *					in_pToken,
		const AkRTPCKey&		in_rtpcKey,
		AkRtpcID				in_RTPCid
		);

	template< typename Accumulate, typename GetValuePolicy >
	AkReal32 GetRTPCConvertedValue(
		void *					in_pToken,
		const AkRTPCKey&		in_rtpcKey,
		AkRtpcID				in_RTPCid
	)
	{
		return GetRTPCConvertedValue<Accumulate, GetValuePolicy, AkDeltaMonitorDisabled>(in_pToken, in_rtpcKey, in_RTPCid);
	}

	template< typename Accumulate, typename GetValuePolicy, typename Monitor >
	AkReal32 GetRTPCConvertedValue(
		void *					in_pToken,
		const AkRTPCKey&		in_rtpcKey
		);

	template< typename Accumulate, typename GetValuePolicy >
	AkReal32 GetRTPCConvertedValue(
		void *					in_pToken,
		const AkRTPCKey&		in_rtpcKey
	)
	{
		return GetRTPCConvertedValue<Accumulate, GetValuePolicy, AkDeltaMonitorDisabled>(in_pToken, in_rtpcKey);
	}
	
	// NOTE: rtpcKey is changed to the closest matching key that is found 
	template< typename GetValuePolicy >
	bool GetRTPCValue(
		AkRtpcID in_RTPC_ID,
		AkRTPC_ParameterID in_RTPC_ParamID, //Used in CAkModulatorMgr to identify audio-rate parameters.
		CAkRTPCMgr::SubscriberType in_SubscrType,
		AkRTPCKey& io_rtpcKey,
		AkReal32& out_value,
		bool& out_bAutomatedParam
		);

///////////////////////////////////////////////////////////////////////////
	// Subscription Functions
///////////////////////////////////////////////////////////////////////////

	AKRESULT SubscribeRTPC(
		void*						in_pSubscriber,
		AkRtpcID					in_RTPC_ID,
		AkRtpcType					in_RTPCType,
		AkRtpcAccum					in_RTPCAccum,
		AkRTPC_ParameterID			in_ParamID,		//# of the param that must be notified on change
		AkUniqueID					in_RTPCCurveID,
		AkCurveScaling				in_eScaling,
		AkRTPCGraphPoint*			in_pArrayConversion,//NULL if none
		AkUInt32					in_ulConversionArraySize,//0 if none
		const AkRTPCKey&			in_rtpcKey,
		SubscriberType				in_eType,
        bool                        in_bIsActive = true
		);

	AkRTPCSubscription* GetSubscription(
		const void*					in_pSubscriber,
		AkUInt32				in_ParamID
		);

    void ActivateSubscription(const void* in_pSubscriber, AkRTPCBitArray in_paramSet);
    void DeactivateSubscription(const void* in_pSubscriber, AkRTPCBitArray in_paramSet);

	AKRESULT RegisterLayer( CAkLayer* in_pLayer, AkRtpcID in_rtpcID, AkRtpcType in_rtpcType );
	void UnregisterLayer( CAkLayer* in_pLayer, AkRtpcID in_rtpcID );

	AKRESULT RegisterSwitchGroup( void* in_pSubscriber, AkRtpcID in_rtpcID, AkRtpcType in_rtpcType, AkRTPCGraphPoint* in_pGraphPts, AkUInt32 in_numGraphPts );
	void UnregisterSwitchGroup( void* in_pSubscriber );

	void UnSubscribeRTPC( void* in_pSubscriber, AkUInt32 in_ParamID, AkUniqueID in_RTPCCurveID, bool* out_bMoreCurvesRemaining = NULL );
	void UnSubscribeRTPC( void* in_pSubscriber, AkUInt32 in_ParamID );

    //Note: this version is very slow and does a liner seach across all subscribers.
	void UnSubscribeRTPC( void* in_pSubscriber );

	void RemovedScopedRtpcObj( AkRtpcID in_RTPCid, const AkRTPCKey& in_rtpcKey );

	void ResetRTPC( CAkRegisteredObj * in_GameObj = NULL, AkPlayingID in_playingID = AK_INVALID_PLAYING_ID );

	void SetDefaultParamValue(AkRtpcID in_RTPCid, AkReal32 in_fValue);
	void SetRTPCRamping( AkRtpcID in_RTPCid, AkTransitionRampingType in_rampingType, AkReal32 in_fRampUp, AkReal32 in_fRampDown );
	void ResetRTPCValue(AkRtpcID in_RTPCid, CAkRegisteredObj * in_GameObj, AkPlayingID in_playingID, TransParamsBase & in_transParams);
	void ResetRTPCValue(AkRtpcID in_RTPCid, const AkRTPCKey& in_rtpcKey, TransParamsBase & in_transParams);

	AkReal32 GetDefaultValue( AkRtpcID in_RTPCid, bool* out_pbWasFound = NULL );

	void AddBuiltInParamBinding( AkBuiltInParam in_builtInParamIdx, AkRtpcID in_RTPCid );
	void RemoveBuiltInParamBindings( AkRtpcID in_RTPCid );
	void SetBuiltInParamValue( AkBuiltInParam in_builtInParamIdx, const AkRTPCKey& in_key, AkReal32 in_fValue );

private:
	class AkRTPCEntry;

	class CAkRTPCTransition AK_FINAL : public ITransitionable
	{
	public:
		CAkRTPCTransition( AkRTPCEntry * in_pOwner, const AkRTPCKey& in_rtpcKey );
		~CAkRTPCTransition();

		AKRESULT Start( 
			AkReal32 in_fStartValue,
			AkReal32 in_fTargetValue,
			TransParamsBase & in_transParams,
			bool in_bRemoveEntryWhenDone
			);

		void Update( 
			AkReal32 in_fNewTargetValue,
			TransParamsBase & in_transParams,
			bool in_bRemoveEntryWhenDone 
			);

		inline const AkRTPCKey& RTPCKey() const { return m_rtpcKey; }
		AkReal32 GetTargetValue();

		CAkRTPCTransition * pNextLightItem;	// List bare light sibling

	protected:
		virtual void TransUpdateValue(AkIntPtr in_eTargetType, AkReal32 in_unionValue, bool in_bIsTerminated);
		
	private:
		CAkTransition *		m_pTransition;
		AkRTPCEntry *		m_pOwner;
		AkRTPCKey 			m_rtpcKey;
		bool				m_bRemoveEntryWhenDone;
	};

	typedef AkListBareLight<CAkRTPCTransition> AkRTPCTransitions;

	class AkRTPCEntry
	{
		friend class CAkRTPCTransition;
	public:
		AkRTPCEntry( AkRtpcID in_rtpcID )
			: key( in_rtpcID )
			, fDefaultValue( 0.f )
			, rampingType( AkTransitionRampingType_None )
			, fRampUp( 0.f )
			, fRampDown( 0.f )
		{}
		~AkRTPCEntry();

		AKRESULT SetRTPC(
			AkRTPCValue * in_pValueEntryToSet, // Pass in the value entry of the game object map. MUST be == ValueExists( in_GameObj ). NULL if there is none.
			AkRTPCValue * in_pParentValueEntry, // Value of a parent scope, if it exists.
			AkReal32 in_Value, 
			const AkRTPCKey& in_ValKey,
			TransParamsBase & in_transParams,
			bool in_bUnsetWhenDone,		// Remove game object's specific RTPC Value once value is set.
			bool in_bCheckExceptions
			);

		// Remove RTPC value for specified key.
		// IMPORTANT: Subscribers are not notified.
		void RemoveValue( const AkRTPCKey& in_key );

		// Remove all RTPC values that match specified key.  Invalid RTPC key fields are treated as wildcards.
		// IMPORTANT: Subscribers are not notified.
		void RemoveMatchingValues( const AkRTPCKey& in_matchingKey );

		// Warning: This requires a binary search.
		inline AkRTPCValue * FindExactValue( const AkRTPCKey& in_key, AkRTPCValue** out_ppFallback, bool* out_bCheckExceptions ) 
		{ 
			return values.FindExact( in_key, out_ppFallback, out_bCheckExceptions ); 
		}

		//io_key in changed to the key that is found inside th database
		inline AkRTPCValue * FindBestMatchValue( AkRTPCKey& io_key ) { return values.FindBestMatch( io_key ); }

		// Returns the default value if current value does not exist.
		// However, the parent value is used instead if available.
		AkReal32 GetCurrentValue( AkRTPCValue * in_pValueEntry, AkRTPCValue * in_pParentValueEntry );

		// Returns the current target value if there is a transition, or the current value otherwise.
		AkReal32 GetCurrentTargetValue( AkRTPCValue * in_pValueEntry, AkRTPCValue * in_pParentValueEntry , const AkRTPCKey& in_key );

        void ActivateSubscription( AkRTPCSubscription* in_pSubscription ){ Move(in_pSubscription, dormantSubscriptions, activeSubscriptions); }
        void DeactivateSubscription( AkRTPCSubscription* in_pSubscription ){ Move(in_pSubscription, activeSubscriptions, dormantSubscriptions); }

		AKRESULT AddSubscription(AkRTPCSubscription* pSubscription, bool in_bIsActive);
		void RemoveSubscription(AkRTPCSubscription* in_pSubscription);
		bool RemoveSubscriptionIfNoCurvesRemain(AkRTPCSubscription* in_pSubscription);

		bool DoesValueHaveTransition( const AkRTPCKey& in_key );

	private:
		// Helpers.
		//
		// Notifies the subscribers and manages the map of values.
		void ApplyRTPCValue( 
			AkRTPCValue * in_pValueEntry, 
			AkRTPCValue * in_pParentValueEntry, 
			AkReal32 in_NewValue, 
			const AkRTPCKey& in_ValKey,
			bool in_bUnsetValue,		// If true, the game object's specific RTPC entry is removed instead of being stored.
			bool in_bCheckExceptions
			);

		// Notifies all subscribers of this RTPC entry, and calls RTPCMgr::NotifyClientRTPCChange()
		// to notify all other global objects of this game parameter change.
		void NotifyRTPCChange( 
			AkReal32 in_CurrentValue,
			AkReal32 in_NewValue, 
			const AkRTPCKey& in_ValKey,
			bool in_bCheckExceptions );

		void GetRTPCExceptions( 
			AkRTPCExceptionChecker*& exChecker
			);

		// Creates or modifies a game parameter transition. 
		// Returns false if creation fails, or if no transition is required whatsoever (transition is 
		// cleaned up inside). In such a case, the caller should apply the RTPC value immediately. 
		// Otherwise, RTPC updates are handled by the transition.
		bool CreateOrModifyTransition(
			const AkRTPCKey& in_rtpcKey, 
			AkReal32 in_fStartValue,
			AkReal32 in_fTargetValue,
			TransParamsBase & in_transParams,
			bool in_bRemoveEntryWhenDone
			);

		// Find[Matching]Transition() - Returns valid iterator if found. Otherwise, iterator equals transitions.End(). 
		void FindTransition( const AkRTPCKey& in_rtpcKey, AkRTPCTransitions::IteratorEx & out_iter );
		void FindMatchingTransition( const AkRTPCKey& in_rtpcKey, AkRTPCTransitions::IteratorEx & out_iter );

		// Destroys transition pointed by valid iterator. Returns the iterator to the next transition.
		inline AkRTPCTransitions::IteratorEx DestroyTransition( AkRTPCTransitions::IteratorEx & in_iter )
		{
            AKASSERT( in_iter != transitions.End() );
			CAkRTPCTransition * pTrRTPC = (*in_iter);
			transitions.Erase( in_iter );
			AkDelete(AkMemID_Object, pTrRTPC );
			return in_iter;
		}

        void Move( AkRTPCSubscription* in_pSubscription, AkRTPCSubscriptions& from, AkRTPCSubscriptions& to );

	public:

		// For AkMapRTPCEntries
		AkRtpcID key;
		AkRTPCEntry * pNextItem;

		AkReal32 fDefaultValue;
		AkTransitionRampingType rampingType;
		AkReal32 fRampUp;
		AkReal32 fRampDown;

		const AkRTPCValueTree& GetValueTree() {return values;} // (WG-48799) : Exposing a const reference for the SwitchMgr to gain access to the exception list. 
	private:
		AkRTPCValueTree values;
		AkRTPCTransitions transitions;
	public:
		AkRTPCSubscriptions activeSubscriptions;
        AkRTPCSubscriptions dormantSubscriptions;
	};

	AkRTPCEntry * GetRTPCEntry( AkRtpcID in_RTPCid );
	//Remove helpers
	void RemoveReferencesToSubscription( AkRTPCSubscription * in_pSubscription );

	typedef AkHashListBare< AkRtpcID, AkRTPCEntry > AkMapRTPCEntries;
	AkMapRTPCEntries m_RTPCEntries;

	typedef AkHashListBare< AkRTPCSubscriptionKey, AkRTPCSubscription > AkMapRTPCSubscribers;
	AkMapRTPCSubscribers m_RTPCSubscribers;

	typedef AkArray<AkRtpcID, AkRtpcID, ArrayPoolDefault> AkRtpcIdArray;
	AkRtpcIdArray m_BuiltInParamBindings[ BuiltInParam_Max ];
};

extern AKSOUNDENGINE_API CAkRTPCMgr* g_pRTPCMgr;

inline bool IsAutomatedParam( AkRTPC_ParameterID in_eParam, CAkRTPCMgr::SubscriberType in_eType )
{
	//RTPC_Volume is modulated at audio-rate and therefore is ignored in the usual curve-conversion.
	return	( in_eType != CAkRTPCMgr::SubscriberType_Plugin  &&
				(	in_eParam == RTPC_Volume ||
					in_eParam == RTPC_MakeUpGain )
					);
}

template<typename Monitor>
AkReal32 CAkRTPCMgr::GetRTPCConvertedValue(
	void*					in_pSubscriber,
	AkUInt32				in_ParamID,
	const AkRTPCKey&		in_rtpcKey)
{
	AkRTPCSubscriptionKey key;
	key.pSubscriber = in_pSubscriber;
	key.ParamID = (AkRTPC_ParameterID)in_ParamID;

	AkRTPCSubscription* pSubscription = m_RTPCSubscribers.Exists(key);
	if (pSubscription)
	{
		if (AK_EXPECT_FALSE(pSubscription->eAccum == AkRtpcAccum_Multiply))
			return GetRTPCConvertedValue<AccumulateMult, CurrentValue, Monitor>((void*)pSubscription, in_rtpcKey);
		else
			return GetRTPCConvertedValue<AccumulateAdd, CurrentValue, Monitor>((void*)pSubscription, in_rtpcKey);
	}

	return DEFAULT_RTPC_VALUE;
}

template< typename Accumulator, typename GetValuePolicy, typename Monitor >
AkReal32 CAkRTPCMgr::GetRTPCConvertedValue(
	void *					in_pToken,
	const AkRTPCKey&		in_rtpcKey,
	AkRtpcID				in_RTPCid
	)
{
	AKASSERT ( in_RTPCid != AK_INVALID_RTPC_ID );

	AkRTPCSubscription * pItem = (AkRTPCSubscription *) in_pToken;

	AkReal32 rtpcValue;
	bool bIsAutomatedParam;	

	AkRTPCKey retrievedKey = in_rtpcKey;
	if ( AK_EXPECT_FALSE( ! GetRTPCValue<GetValuePolicy>( in_RTPCid, pItem->key.ParamID, pItem->eType, retrievedKey, rtpcValue, bIsAutomatedParam ) ) )
		rtpcValue = GetDefaultValue(in_RTPCid);


	AkReal32 fResult = Accumulator::BaseValue();
	if (AK_EXPECT_FALSE(!bIsAutomatedParam))
	{
		for( RTPCCurveArray::Iterator iterCurves = pItem->Curves.Begin(); iterCurves != pItem->Curves.End(); ++iterCurves )
		{
			if (iterCurves.pItem->RTPC_ID == in_RTPCid)
			{
				AkReal32 fVal = iterCurves.pItem->ConversionTable.Convert(rtpcValue);
				Accumulator::Accumulate(fResult, fVal);
				Monitor::LogRTPC(iterCurves.pItem->RTPC_ID, pItem->key.ParamID, rtpcValue, fVal, retrievedKey.GameObj() != NULL ? AkDelta_RTPC : AkDelta_RTPCGlobal);
			}
		}
	}		

	return fResult;
}

template< typename Accumulator, typename GetValuePolicy, typename Monitor >
AkReal32 CAkRTPCMgr::GetRTPCConvertedValue(
	void *					in_pToken,
	const AkRTPCKey&		in_rtpcKey
	)
{
	AkRTPCSubscription * pItem = (AkRTPCSubscription *) in_pToken;

	AkReal32 rtpcValue;
	bool bIsAutomatedParam;	

	AkReal32 fResult = Accumulator::BaseValue();
	for( RTPCCurveArray::Iterator iterCurves = pItem->Curves.Begin(); iterCurves != pItem->Curves.End(); ++iterCurves )
	{
		AkRTPCKey retrievedKey = in_rtpcKey;
		if (AK_EXPECT_FALSE(!GetRTPCValue<GetValuePolicy>( iterCurves.pItem->RTPC_ID, pItem->key.ParamID, pItem->eType, retrievedKey, rtpcValue, bIsAutomatedParam )))
			rtpcValue = GetDefaultValue(iterCurves.pItem->RTPC_ID);

		if (AK_EXPECT_FALSE(!bIsAutomatedParam))
		{
			AkReal32 fVal = iterCurves.pItem->ConversionTable.Convert(rtpcValue);
			Accumulator::Accumulate(fResult, fVal);
			Monitor::LogRTPC(iterCurves.pItem->RTPC_ID, pItem->key.ParamID, rtpcValue, fVal, retrievedKey.GameObj() != NULL ? AkDelta_RTPC : AkDelta_RTPCGlobal);
		}
	}	

	return fResult;
}

template< typename GetValuePolicy >
bool CAkRTPCMgr::GetRTPCValue(
							  AkRtpcID in_RTPC_ID,
							  AkRTPC_ParameterID in_RTPC_ParamID,
							  CAkRTPCMgr::SubscriberType in_SubscrType,
							  AkRTPCKey& io_rtpcKey,
							  AkReal32& out_value,
							  bool& out_bAutomatedParam
							  )
{
	AkRTPCEntry * pEntry = m_RTPCEntries.Exists( in_RTPC_ID );
	if ( pEntry )
	{
		out_bAutomatedParam = false;

		AkRTPCValue * pValue = pEntry->FindBestMatchValue( io_rtpcKey );
		if ( pValue )
		{
			out_value = pValue->fValue;
			return true;
		}
	}
	else //Check for a modulator value
	{
		out_bAutomatedParam = IsAutomatedParam(in_RTPC_ParamID, in_SubscrType) && GetValuePolicy::ModulatorSupportsAutomatedParams(in_RTPC_ID);
		if ( out_bAutomatedParam )
		{
			out_value = 1.0f;
			return true;
		}
		else
		{
			return GetValuePolicy::GetModulatorValue(in_RTPC_ID,io_rtpcKey,out_value);
		}
	}

	return false;
}

AkUInt32 GetNumberOfRTPC(AkUInt8*& io_rpData);

template <class T>
AKRESULT SetInitialRTPC(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, T* in_pThis, bool in_bNotify = true)
{
	AKRESULT eResult = AK_Success;

	AkUInt32 ulNumRTPC = READBANKDATA(AkUInt16, io_rpData, io_rulDataSize);

	for (AkUInt32 i = 0; i < ulNumRTPC; ++i)
	{
		const AkRtpcID l_RTPCID = READBANKDATA(AkUInt32, io_rpData, io_rulDataSize);
		const AkRtpcType rtpcType = (AkRtpcType)READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
		const AkRtpcAccum rtpcAccum = (AkRtpcAccum)READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
		const AkRTPC_ParameterID l_ParamID = (AkRTPC_ParameterID)READVARIABLESIZEBANKDATA(AkUInt32, io_rpData, io_rulDataSize);
		const AkUniqueID rtpcCurveID = (AkUniqueID)READBANKDATA(AkUInt32, io_rpData, io_rulDataSize);
		const AkCurveScaling eScaling = (AkCurveScaling)READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
		const AkUInt32 ulSize = READBANKDATA(AkUInt16, io_rpData, io_rulDataSize);

		eResult = in_pThis->SetRTPC(l_RTPCID, rtpcType, rtpcAccum, l_ParamID, rtpcCurveID, eScaling, (AkRTPCGraphPoint*)io_rpData, ulSize, in_bNotify);
		if (eResult != AK_Success)
			break;

		// Skipping the Conversion array
		io_rpData += (ulSize*sizeof(AkRTPCGraphPoint));
		io_rulDataSize -= (ulSize*sizeof(AkRTPCGraphPoint));
	}

	return eResult;
}

#endif //_RTPC_MGR_H_

