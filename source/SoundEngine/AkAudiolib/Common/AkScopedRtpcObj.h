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
// AkScopedRtpcObj.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_RTPC_SCOPE_TRACKER_H_
#define _AK_RTPC_SCOPE_TRACKER_H_

#include "AkRTPCKey.h"

#if 0 //def _DEBUG
#define RTPC_OBJ_SCOPE_SAFETY
#endif

struct AkAkRtpcIDSetGetKey{ static AkForceInline AkRtpcID& Get(AkRtpcID& in_item){ return in_item;} };
typedef AkSortedKeyArray<AkRtpcID, AkRtpcID, ArrayPoolDefault, AkAkRtpcIDSetGetKey >  AkRtpcIDSet;

//	A book keeping class used to keep track of what RTPC id's are used by objects that exist in a particular RTPC scope.
//  Currently extended by CAkRegisteredObj (game object scope), CAkPBI (voice scope) and CAkPlayingMgr::PlayingMgrItem (playing id scope) to keep
//		track of the RTPCs that are used at each of these scopes.  Before the object is deleted it will call Term() which will call into the RTPC and Modulator managers
//		to clean up corresponding scoped items.
class CAkScopedRtpcObj
{
public:

	// Call to add an rtpc id for tracking purposes.  Static interface used by rtpc/modulator manager.
	static AKRESULT AddedNewRtpcValue( AkRtpcID in_rtpcId, const AkRTPCKey& in_ValKey );
	static AKRESULT AddedNewModulatorCtx( AkRtpcID in_rtpcId, const AkRTPCKey& in_ValKey );

#ifdef RTPC_OBJ_SCOPE_SAFETY	
	static void CheckRtpcKey( const AkRTPCKey& in_rtpcKey );
#endif

public:

	CAkScopedRtpcObj();
	~CAkScopedRtpcObj();

	// Call to add an rtpc id for tracking purposes.  
	bool AddedNewRtpcValue( AkRtpcID in_forRtpcId )		{ return NULL != m_RtpcVals.Set(in_forRtpcId); }
	bool AddedNewModulatorCtx( AkRtpcID in_forRtpcId )	{return NULL != m_Modulators.Set(in_forRtpcId); }

	// Term() - Must be called before the object is destroyed.  Notifys interested parties that the 
	//	object has been removed.deleted and that entries corresponding to the rtpc key can be removed.
	void Term(const AkRTPCKey& in_rtpcKey);
	
private:

	void AddedNewRtpcId( AkRtpcIDSet& in_array, AkRtpcID in_id );

	 AkRtpcIDSet m_Modulators;
	 AkRtpcIDSet m_RtpcVals;

#ifdef RTPC_OBJ_SCOPE_SAFETY
	AkUInt32 m_Safety;
#endif
};

#ifdef RTPC_OBJ_SCOPE_SAFETY

template < typename T_VALUE >
bool _CheckRtpcKey( T_VALUE& in_value, const AkRTPCKey& in_rtpcKey, void* )
{
	CAkScopedRtpcObj::CheckRtpcKey( in_rtpcKey );
	return false;
}

template < typename T_RTPC_TREE_VALUE >
void CheckScopeConsistency( AkRTPCKeyTree< T_RTPC_TREE_VALUE >& in_Tree )
{
	in_Tree.ForEach( &_CheckRtpcKey<T_RTPC_TREE_VALUE>, NULL );
}

#define SCOPE_CHECK_TREE( tree, treetype ) CheckScopeConsistency<treetype>( tree )
#define SCOPE_CHECK_TREE_VALUE( key ) CheckRtpcKey( key )
#else
#define SCOPE_CHECK_TREE( tree, treetype )
#define SCOPE_CHECK_TREE_VALUE( key )
#endif

#endif
