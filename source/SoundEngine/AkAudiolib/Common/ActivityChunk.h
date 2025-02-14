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
// ActivityChunk.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ACTIVITYCHUNK_H_
#define _ACTIVITYCHUNK_H_

#include "AkPBI.h"
#include <AK/Tools/Common/AkListBareLight.h>
#include "AkRTPCSubscriber.h"

typedef AkArray<CAkParameterNodeBase*, CAkParameterNodeBase*, ArrayPoolDefault> AkChildArray;

struct AkLimiterKey{
	AkForceInline bool operator<(const AkLimiterKey& in_limiterKey) const
	{
		return key > in_limiterKey.key;
	}

	AkForceInline bool operator>(const AkLimiterKey& in_limiterKey) const
	{
		return key < in_limiterKey.key;
	}

	AkForceInline bool operator==(const AkLimiterKey& in_limiterKey) const
	{
		return key == in_limiterKey.key;
	}

	enum LimiterType
	{
		LimiterType_Global,
		LimiterType_Bus,
		LimiterType_Default
	};
	
	enum KeyInfoSize
	{
		KeyInfoSize_LimiterType = 3,
		KeyInfoSize_Depth = 29,
		KeyInfoSize_ShortID = 32
	};

	LimiterType GetLimiterTypeFromKey()
	{
		return static_cast<LimiterType>( ( key >> ( KeyInfoSize_Depth + KeyInfoSize_ShortID ) ) & ~( 0 << KeyInfoSize_LimiterType ) );
	}

	AkUInt32 GetDepthFromKey()
	{
		return ( key >> KeyInfoSize_ShortID ) & ~( 0 << KeyInfoSize_Depth );
	}

	AkUInt32 GetShortIDFromKey()
	{
		return key & 0xffffffff;
	}

	void SetKey( LimiterType in_limiterType, AkUInt64 in_depth, AkUInt64 in_shortId )
	{
		AkUInt64  limiterType = in_limiterType;
		key = 0;
		key |= limiterType << ( KeyInfoSize_Depth + KeyInfoSize_ShortID );
		key |= in_depth << KeyInfoSize_ShortID;
		key |= in_shortId;
	}

	AkUInt64 key;
};

struct AkSortedPBIGetKey
{	
	static AkForceInline AkPriorityStruct& Get(CAkPBI*& in_item)
	{
		return in_item->GetPriorityKey();
	}
};

class CAkLimiter;
template<class OWNER_CLASS>
struct AkPBIPriorityCompare
{
public:	
	static AkForceInline bool IsNewer(AkPriorityStruct &a, AkPriorityStruct &b)
	{		
		if(a.seqID == b.seqID)
			return a.pipelineID > b.pipelineID;	
		return a.seqID > b.seqID;
	}

	static AkForceInline bool IsOlder(AkPriorityStruct &a, AkPriorityStruct &b)
	{		
		if(a.seqID == b.seqID)
			return a.pipelineID < b.pipelineID;
		return a.seqID < b.seqID;
	}
	
	static AkForceInline bool Lesser(void* in_pLimiter, AkPriorityStruct &a, AkPriorityStruct &b)
	{
		if(a.GetPriority() == b.GetPriority())
		{
			if(((OWNER_CLASS*)in_pLimiter)->DoesKillNewest())
				return IsOlder(a, b);
			else
				return IsNewer(a, b);
		}
		return a.GetPriority() > b.GetPriority();
	}

	static AkForceInline bool Equal(void* in_pLimiter, AkPriorityStruct &a, AkPriorityStruct &b)
	{
		return a == b;
	}
};


typedef AkSortedKeyArray<AkPriorityStruct, CAkPBI*, ArrayPoolDefault, AkSortedPBIGetKey, AkGrowByPolicy_DEFAULT, AkAssignmentMovePolicy<CAkPBI*>, AkPBIPriorityCompare<CAkLimiter> > AkSortedPBIPriorityList;
class CAkLimiter : public AkSortedPBIPriorityList
{
public:
	CAkLimiter( AkUInt16 in_u16LimiterMax, bool in_bDoesKillNewest, bool in_bAllowUseVirtualBehavior );

	void SetKey( AkLimiterKey::LimiterType in_limiterType, AkUInt64 in_depth, AkUInt64 in_shortId )
	{
		m_limiterKey.SetKey( in_limiterType, in_depth, in_shortId );
	}

	struct AkSortedLimiterGetKey
	{
		static AkForceInline AkLimiterKey& Get( CAkLimiter*& in_item )
		{
			return in_item->m_limiterKey;
		}
	};

	AKRESULT Add( CAkPBI* in_pPBI );
	void Remove( CAkPBI* in_pPBI );
	void Update( AkReal32 in_NewPriority, CAkPBI* in_pPBI );

	void UpdateFlags();

	void UpdateMax( AkUInt16 in_u16LimiterMax )
	{
		m_u16LimiterMax = in_u16LimiterMax;
	}

	void SetUseVirtualBehavior( bool in_bAllowUseVirtualBehavior )
	{
		m_bAllowUseVirtualBehavior = in_bAllowUseVirtualBehavior;
	}

	AkUInt16 GetMaxInstances()	{ return 	m_u16LimiterMax; }
	bool 	DoesKillNewest()	{ return	m_bDoesKillNewest; }

	void SwapOrdering();	

	const AkLimiterKey GetLimiterKey()
	{
		return m_limiterKey;
	}

	AkUInt16 GetCurrentCount() const
	{
		return m_u16Current;
	}
	
	AkUInt16 GetCurrentVirtualCount() const
	{
		return m_u16CurrentVirtual;
	}

	void IncrementVirtual() { m_u16CurrentVirtual++; }

	void DecrementVirtual() { m_u16CurrentVirtual--; }

protected:	

	AkUInt16	m_u16LimiterMax;
	bool		m_bDoesKillNewest;
	bool		m_bAllowUseVirtualBehavior;

	

public:
	CAkLimiter *pNextLightItem;	//For AkListBareLight

private:
	AkLimiterKey m_limiterKey;

	AkUInt16 m_u16Current;
	AkUInt16 m_u16CurrentVirtual;
};

class CAkParamTargetLimiter: public CAkParameterTarget, public CAkLimiter
{
public:
	CAkParamTargetLimiter():	CAkParameterTarget(AkRTPCKey(), true ), CAkLimiter( 0, false, false ) {}

	void Init( CAkParameterNodeBase* in_pNode, CAkRegisteredObj* in_pGameObj, AkUInt16 in_u16LimiterMax, bool in_bDoesKillNewest, bool in_bAllowUseVirtualBehavior );

	//CAkParameterTarget
	virtual void UpdateTargetParam( AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta )
	{
		if (in_eParam == RTPC_MaxNumInstances){	m_u16LimiterMax = (AkUInt16) in_fValue;	}
	}

	virtual AkRTPCBitArray GetTargetedParamsSet() { return AkRTPCBitArray(1ULL<<RTPC_MaxNumInstances); }
};

class StructMaxInst
{
public:
	//Constructor
	StructMaxInst()
		:m_pLimiter( NULL )
	{
	}

	bool Init(CAkParameterNodeBase* in_pNode, CAkRegisteredObj* in_pGameObj, AkUInt16 in_u16Max, bool in_bDoesKillNewest, bool in_bAllowUseVirtualBehavior )
	{
		// If it fails there will be no limitor, we can't help it, memory will be the new limit.
		m_pLimiter = AkNew( AkMemID_Object, CAkParamTargetLimiter() );
		if (m_pLimiter)
			m_pLimiter->Init(in_pNode, in_pGameObj, in_u16Max, in_bDoesKillNewest, in_bAllowUseVirtualBehavior);
		return m_pLimiter != NULL;
	}

	void DisableLimiter()
	{
		if( m_pLimiter )
		{
			m_pLimiter->Term();
			AkDelete(AkMemID_Object, m_pLimiter);
			m_pLimiter = NULL;
		}
	}

	void SetMax( AkUInt16 in_u16Max )
	{ 
		if( m_pLimiter )
			m_pLimiter->UpdateMax( in_u16Max );
	}

	bool IsMaxNumInstancesActivated(){ return GetMax() != 0; }

	AkUInt16 GetCurrent(){ return m_pLimiter ? m_pLimiter->GetCurrentCount() : 0; }
	AkUInt16 GetCurrentVirtual(){ return m_pLimiter ? m_pLimiter->GetCurrentVirtualCount() : 0; }
	AkUInt16 GetMax()
	{ 
		if( m_pLimiter )
			return m_pLimiter->GetMaxInstances(); 
		return 0;
	}

	void SwapOrdering()
	{
		if( m_pLimiter )
			m_pLimiter->SwapOrdering();
	}

	void SetUseVirtualBehavior( bool in_bUseVirtualBehavior )
	{
		if( m_pLimiter )
			m_pLimiter->SetUseVirtualBehavior( in_bUseVirtualBehavior );
	}

	CAkParamTargetLimiter* m_pLimiter;// One limiting list per game object.
};


typedef CAkKeyArray<CAkRegisteredObj *, StructMaxInst> AkPerObjPlayCount;

class AkActivityCounter
{
public:
	AkActivityCounter()
		: m_iCount(0)
		, m_iRoutedCount(0)
	{
	}

	void Increment(bool in_bIsRoutedToBus)
	{
		++m_iCount;
		if (in_bIsRoutedToBus)
			++m_iRoutedCount;
	}

	void Decrement(bool in_bIsRoutedToBus)
	{
		--m_iCount;
		if (in_bIsRoutedToBus)
			--m_iRoutedCount;
	}

	void IncrementRouted(AkInt16 in_count)
	{
		m_iRoutedCount += in_count;
	}

	void DecrementRouted(AkInt16 in_count)
	{
		AKASSERT(m_iRoutedCount >= in_count);
		m_iRoutedCount -= in_count;
	}

	void Reset()
	{
		m_iCount = 0;
		m_iRoutedCount = 0;
	}

	AkInt16 Count() const{
		return m_iCount;
	}

	AkInt16 RoutedCount() const{
		return m_iRoutedCount;
	}

private:
	AkInt16 m_iCount;
	AkInt16 m_iRoutedCount;
};

// The following class contains multiple information that is not required when the node is not playing.
// ( saving significant memory amount on each AudioNode in % )
class AkActivityChunk
{
public:
	AkActivityChunk()
		:m_Limiter()
		,m_bIsGlobalLimit( true )
		,m_bInURendererList( false )
	{
	}

	void Init( CAkParameterNodeBase* in_pNode, AkUInt16 in_u16MaxNumInstance, bool in_bIsGlobalLimit, bool in_bDoesKillNewest,  bool in_bAllowUseVirtualBehavior )
	{
		m_Limiter.Init(in_pNode, NULL, in_u16MaxNumInstance, in_bDoesKillNewest, in_bAllowUseVirtualBehavior);
		m_PlayCount.Reset();
		m_ActivityCount.Reset();
		m_bIsGlobalLimit = in_bIsGlobalLimit;
	}

	~AkActivityChunk()
	{
		Term();
	}

	void Term()
	{
		m_Limiter.UnregisterParamTarget();
		m_Limiter.Term();
		m_listPBI.Term();
		AKASSERT( m_ListPlayCountPerObj.Length() == 0 );//If not true, we would have to term m_ListPlayCountPerObj members.
		m_ListPlayCountPerObj.Term();
	}

	AkInt16 GetPlayCount() const { return m_PlayCount.Count(); }
	AkInt16 GetRoutedToBusPlayCount() const { return m_PlayCount.RoutedCount(); }
	AkInt16 GetActivityCount() const { return m_ActivityCount.Count(); }
	AkInt16 GetRoutedToBusActivityCount() const { return m_ActivityCount.RoutedCount(); }

	AkUInt16 GetPlayCountValid() const { return m_Limiter.GetCurrentCount(); }
	AkUInt16 GetVirtualCountValid() const { return m_Limiter.GetCurrentVirtualCount(); }

	void IncrementPlayCount(bool in_bIsRoutedToBus)
	{
		m_PlayCount.Increment(in_bIsRoutedToBus);
	}
	void DecrementPlayCount(bool in_bIsRoutedToBus)
	{
		m_PlayCount.Decrement(in_bIsRoutedToBus);
	}

	void IncrementActivityCount(bool in_bIsRoutedToBus)
	{ 
		m_ActivityCount.Increment(in_bIsRoutedToBus);
	}
	void DecrementActivityCount(bool in_bIsRoutedToBus)
	{
		m_ActivityCount.Decrement(in_bIsRoutedToBus);
	}

	void IncrementPlayCountRouted(AkInt16 in_count)
	{
		m_PlayCount.IncrementRouted(in_count);
	}
	void DecrementPlayCountRouted(AkInt16 in_count)
	{
		m_PlayCount.DecrementRouted(in_count);
	}

	void IncrementActivityCountRouted(AkInt16 in_count)
	{
		m_ActivityCount.IncrementRouted(in_count);
	}
	void DecrementActivityCountRouted(AkInt16 in_count)
	{
		m_ActivityCount.DecrementRouted(in_count);
	}

	bool ChunkIsUseless() const
	{
		// WG-28615 // can go negative due to known bug not fixed yet.
		AKASSERT( m_Limiter.GetCurrentCount() >= m_Limiter.GetCurrentVirtualCount() );
		return (m_PlayCount.Count() <= 0)
			&& (m_ActivityCount.Count() <= 0)
			&& !m_Limiter.GetCurrentCount()
			&& !m_Limiter.GetCurrentVirtualCount()
			&& (m_listPBI.IsEmpty())
			&& (m_ListPlayCountPerObj.IsEmpty() ) ;
	}

	AkPerObjPlayCount m_ListPlayCountPerObj;

	typedef AkListBareLight<CAkBehavioralCtx> AkListLightCtxs;
	AkListLightCtxs m_listPBI;

	void UpdateMaxNumInstanceGlobal( AkUInt16 in_u16LastMaxNumInstanceForRTPC )
	{ 
		m_Limiter.UpdateMax( in_u16LastMaxNumInstanceForRTPC );
	}

	bool IsGlobalLimit(){ return m_bIsGlobalLimit; }

	void SetIsGlobalLimit( bool in_bIsGlobalLimit ) { m_bIsGlobalLimit = in_bIsGlobalLimit; }

	bool InURendererList() const { return m_bInURendererList; }
	void SetInURendererList( bool in_bInList ) { m_bInURendererList = in_bInList; }

	//NOTE: This limiter is useless if m_bIsGlobalLimit is false.  Instead we use the limiters in m_ListPlayCountPerObj.
	CAkParamTargetLimiter m_Limiter;

private:

	AkActivityCounter m_PlayCount;
	AkActivityCounter m_ActivityCount;

	AkUInt8 m_bIsGlobalLimit : 1;
	AkUInt8 m_bInURendererList : 1; // The owner is in the URenderer active node list
};

#endif //_ACTIVITYCHUNK_H_
