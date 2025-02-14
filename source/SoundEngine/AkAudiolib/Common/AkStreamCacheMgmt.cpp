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
// AkStmCacheMgmt.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkEvent.h"
#include "AkParameterNodeBase.h"
#include "AkSound.h"
#include "AkSwitchAware.h"
#include <float.h>

struct AkCacheGameSyncMon: public AkGameSync, public CAkSwitchAware
{
	AkCacheGameSyncMon( AkGameSync in_gameSync ): AkGameSync(in_gameSync), m_lRef(1)
	{
		SubscribeSwitch( in_gameSync.m_ulGroup, in_gameSync.m_eGroupType );
	}

	virtual ~AkCacheGameSyncMon()
	{
		UnsubscribeSwitches();
	}

	//CAkSwitchAware interface
	virtual void SetSwitch( 
		AkUInt32 in_Switch, 
		const AkRTPCKey& in_rtpcKey,
		AkRTPCExceptionChecker* in_pExCheck = NULL
		);

	virtual AkUInt32 AddRef() 
	{
		return ++m_lRef; 
	}

	virtual AkUInt32 Release() 
	{
		AkInt32 lRef = --m_lRef; 
		AKASSERT( lRef >= 0 ); 
		if ( !lRef ) 
		{ 
			AkDelete(AkMemID_Streaming, this );
		} 
		return lRef; 
	}

	AkInt32 m_lRef;
};

struct AkCachePinnedFileSetKey
{
	AkCachePinnedFileSetKey(): m_pGameObj(NULL), m_pEvent(0) {}

	CAkRegisteredObj* m_pGameObj;
	CAkEvent* m_pEvent;

	bool operator< ( const AkCachePinnedFileSetKey& other ) const
	{
		return (	( m_pGameObj < other.m_pGameObj )										||
					( m_pGameObj == other.m_pGameObj && m_pEvent < other.m_pEvent )			);
	}

	bool operator> ( const AkCachePinnedFileSetKey& other ) const
	{
		return (	( m_pGameObj > other.m_pGameObj )										||
			( m_pGameObj == other.m_pGameObj && m_pEvent > other.m_pEvent )			);
	}

	bool operator== ( const AkCachePinnedFileSetKey& other ) const
	{
		return ( m_pGameObj == other.m_pGameObj && m_pEvent == other.m_pEvent ) ;
	}
};

struct AkCachePinnedFileSet
{
	AkCachePinnedFileSet(): m_uActivePriority(0), m_uInactivePriority(0) {}
	~AkCachePinnedFileSet()
	{
		m_ActiveFiles.Term();
		m_InactiveFiles.Term();
		m_SwitchGroups.Term();
	}

	AkUniqueIDSet	m_SwitchGroups;
	AkUniqueIDSet	m_ActiveFiles;
	AkUniqueIDSet	m_InactiveFiles;
	AkPriority		m_uActivePriority;
	AkPriority		m_uInactivePriority;

	void GatherAndPinFilesInStreamCache(CAkEvent* in_pEvent, CAkRegisteredObj* in_pGameObj, AkUInt32 in_uUpdateGameSync, AkUInt32 in_uNewGameSyncValue);
	void UnpinFilesInStreamCache();
	void SetGameSync( AkGameSync& in_gameSync );
	void UnsetAllGameSyncs();
	void UpdatePriority(AkPriority in_uActivePriority, AkPriority in_uInactivePriority);
};

typedef AkKeyDataPtrStruct< AkCachePinnedFileSetKey, AkCachePinnedFileSet > AkCachePinnedFileSetStruct;
typedef AkKeyDataPtrStruct< AkUInt32, AkCacheGameSyncMon > AkCacheGameSyncMonStruct;

typedef AkSortedKeyArray< AkCachePinnedFileSetKey, AkCachePinnedFileSetStruct, ArrayPoolDefault, AkCachePinnedFileSetStruct > AkCachePinnedFileArray;
AkCachePinnedFileArray g_CachePinnedFiles;
CAkLock g_CachePinnedFilesLock;

// Map game sync groups (switch/state groups) to AkCacheGameSyncMonStruct
typedef AkSortedKeyArray< AkUInt32, AkCacheGameSyncMonStruct, ArrayPoolDefault, AkCacheGameSyncMonStruct > AkCacheGameSyncMonArray;
AkCacheGameSyncMonArray g_CacheGameSyncMons;


void PinFilesInStreamCache(CAkEvent* in_pEvent, CAkRegisteredObj* in_pObj, AkPriority in_uActivePriority, AkPriority in_uInactivePriority)
{
	AkCachePinnedFileSetKey key;
	key.m_pGameObj = in_pObj;
	key.m_pEvent = in_pEvent;

	AkAutoLock<CAkLock> lock(g_CachePinnedFilesLock); 

	AkCachePinnedFileSetStruct* pFileSetStruct = g_CachePinnedFiles.Exists( key );
	if (pFileSetStruct)
	{
		AKASSERT(pFileSetStruct->pData != NULL);
		pFileSetStruct->pData->UpdatePriority(in_uActivePriority, in_uInactivePriority);
	}
	else
	{
		pFileSetStruct = g_CachePinnedFiles.Set( key );
		if (pFileSetStruct)
		{
			AKASSERT(pFileSetStruct->pData == NULL);
			
			if (pFileSetStruct->AllocData())
			{
				if (pFileSetStruct->key.m_pGameObj)
					pFileSetStruct->key.m_pGameObj->AddRef();
				
				pFileSetStruct->pData->m_uActivePriority = in_uActivePriority;
				pFileSetStruct->pData->m_uInactivePriority = in_uInactivePriority;
			
				(pFileSetStruct->pData)->GatherAndPinFilesInStreamCache(pFileSetStruct->key.m_pEvent, pFileSetStruct->key.m_pGameObj, 0, 0);
			}
			else
			{
				g_CachePinnedFiles.Unset(key);
			}
		}
	}
}

void UnpinFilesInStreamCache(CAkEvent* in_pEvent, CAkRegisteredObj* in_pObj)
{
	AkCachePinnedFileSetKey key;
	key.m_pGameObj = in_pObj;
	key.m_pEvent = in_pEvent;

	AkAutoLock<CAkLock> lock(g_CachePinnedFilesLock);

	AkCachePinnedFileSetStruct* pFileSetStruct = g_CachePinnedFiles.Exists(key);
	if ( pFileSetStruct )
	{
		AKASSERT(pFileSetStruct->pData != NULL);
		pFileSetStruct->pData->UnpinFilesInStreamCache();
		pFileSetStruct->pData->UnsetAllGameSyncs();
		if (pFileSetStruct->key.m_pGameObj)
			pFileSetStruct->key.m_pGameObj->Release();
		pFileSetStruct->FreeData();

		g_CachePinnedFiles.Unset(key);
	}
}

void UnpinAllFilesInStreamCache(CAkEvent* in_pEvent)
{
	AkAutoLock<CAkLock> lock(g_CachePinnedFilesLock);

	for( AkCachePinnedFileArray::Iterator it = g_CachePinnedFiles.Begin(); it != g_CachePinnedFiles.End(); )
	{
		AkCachePinnedFileSetStruct& fileSetStruct = *it;
		if ( fileSetStruct.key.m_pEvent == in_pEvent )
		{
			AKASSERT(fileSetStruct.pData != NULL);
			fileSetStruct.pData->UnpinFilesInStreamCache();
			fileSetStruct.pData->UnsetAllGameSyncs();
			if (fileSetStruct.key.m_pGameObj)
				fileSetStruct.key.m_pGameObj->Release();
			fileSetStruct.FreeData();
			it = g_CachePinnedFiles.Erase(it);
		}
		else
		{
			++it;
		}
	}

	if (g_CachePinnedFiles.IsEmpty())
		g_CachePinnedFiles.Term();
}


AKRESULT GetBufferStatusForPinnedFiles( CAkEvent* in_pEvent, CAkRegisteredObj* in_pGameObj, AkReal32& out_fPercentBuffered, bool& out_bCacheMemFull )
{
	out_bCacheMemFull = false;
	out_fPercentBuffered = 0;

	AKRESULT res = AK_Success;
	AkReal32 fMinPercentBuffered = 100.0f;

	AkCachePinnedFileSetKey key;
	key.m_pGameObj = in_pGameObj;
	key.m_pEvent = in_pEvent;

	AkAutoLock<CAkLock> lock(g_CachePinnedFilesLock);

	AkCachePinnedFileSetStruct* pFileSetStruct = g_CachePinnedFiles.Exists(key);
	if ( pFileSetStruct )
	{
		AKASSERT(pFileSetStruct->pData != NULL);
		for ( AkUniqueIDSet::Iterator fileIt = pFileSetStruct->pData->m_ActiveFiles.Begin(); fileIt != pFileSetStruct->pData->m_ActiveFiles.End(); ++fileIt)
		{
			bool bCacheMemFull;
			AkReal32 fPercent;
			if ( AK::IAkStreamMgr::Get()->GetBufferStatusForPinnedFile( *fileIt, fPercent, bCacheMemFull ) == AK_Success )
			{
				fMinPercentBuffered = AkMin( fMinPercentBuffered, fPercent );
				out_bCacheMemFull = out_bCacheMemFull || bCacheMemFull;
			}
			else
			{
				return AK_Fail;				
			}
		}

		for ( AkUniqueIDSet::Iterator fileIt = pFileSetStruct->pData->m_InactiveFiles.Begin(); fileIt != pFileSetStruct->pData->m_InactiveFiles.End(); ++fileIt)
		{
			bool bCacheMemFull;
			AkReal32 fPercent;
			if ( AK::IAkStreamMgr::Get()->GetBufferStatusForPinnedFile( *fileIt, fPercent, bCacheMemFull ) == AK_Success )
			{
				fMinPercentBuffered = AkMin( fMinPercentBuffered, fPercent );
				out_bCacheMemFull = out_bCacheMemFull || bCacheMemFull;
			}
			else
			{
				return AK_Fail;
			}
		}
	}
	else 
		return AK_Fail;

	out_fPercentBuffered = fMinPercentBuffered;

	return AK_Success;
}

//
// AkCacheGameSyncMon
//
void AkCacheGameSyncMon::SetSwitch( 
					   	AkUInt32 in_Switch, 
						const AkRTPCKey& in_rtpcKey,
						AkRTPCExceptionChecker* in_pExCheck
					   )
{
	bool found;
	AkCachePinnedFileSetKey key;
	key.m_pGameObj = (CAkRegisteredObj*)in_rtpcKey.GameObj();
	key.m_pEvent = 0;

	AkAutoLock<CAkLock> lock(g_CachePinnedFilesLock);

	AkCachePinnedFileArray::Iterator it;
	it.pItem = g_CachePinnedFiles.BinarySearch( key, found );
	while ( it != g_CachePinnedFiles.End() && 
			it.pItem != NULL && 
			(it.pItem->key.m_pGameObj == in_rtpcKey.GameObj() || 
					( in_rtpcKey.GameObj() == NULL && 
							(!in_pExCheck || !in_pExCheck->IsException(it.pItem->key.m_pGameObj)) 
					 )
			)
			)
	{
		AkCachePinnedFileSet* pFileSet = it.pItem->pData;
		AKASSERT(pFileSet != NULL);
		pFileSet->GatherAndPinFilesInStreamCache(it.pItem->key.m_pEvent, it.pItem->key.m_pGameObj, this->m_ulGroup, in_Switch);

		++it;
	}
}

//
//	AkCachePinnedFileSet
//


void AkCachePinnedFileSet::UnpinFilesInStreamCache()
{
	for ( AkUniqueIDSet::Iterator fileIt = m_ActiveFiles.Begin(); fileIt != m_ActiveFiles.End(); ++fileIt)
	{
		AK::IAkStreamMgr::Get()->UnpinFileInCache( (*fileIt), m_uActivePriority );
	}

	for ( AkUniqueIDSet::Iterator fileIt = m_InactiveFiles.Begin(); fileIt != m_InactiveFiles.End(); ++fileIt)
	{
		AK::IAkStreamMgr::Get()->UnpinFileInCache( (*fileIt), m_uInactivePriority );
	}

	m_ActiveFiles.RemoveAll();
	m_InactiveFiles.RemoveAll();
}

void AkCachePinnedFileSet::GatherAndPinFilesInStreamCache(CAkEvent* in_pEvent, CAkRegisteredObj* in_pGameObj, AkUInt32 in_uUpdateGameSync, AkUInt32 in_uNewGameSyncValue)
{
	AKASSERT(in_pEvent);

	//Save the old sets of files to temp variables.
	AkUniqueIDSet oldActiveFileSet = m_ActiveFiles;
	AkUniqueIDSet oldInactiveFileSet = m_InactiveFiles;
	m_ActiveFiles = AkUniqueIDSet(); //Init m_ActiveFiles back to NULL.
	m_InactiveFiles = AkUniqueIDSet(); //Init m_InactiveFiles back to NULL.

	AkSoundArray activeSounds;
	AkSoundArray inactiveSounds;
	AkGameSyncArray gameSyncs;
	in_pEvent->GatherSounds(activeSounds, inactiveSounds, gameSyncs, in_pGameObj, in_uUpdateGameSync, in_uNewGameSyncValue);

	// Update active sounds first to get them in the map with high priority
	for ( AkSoundArray::Iterator soundId = activeSounds.Begin(); soundId != activeSounds.End(); ++soundId)
	{
		AkSrcTypeInfo * pSrcType = (*soundId);
		AKASSERT(pSrcType);

		AkFileID fileId = pSrcType->GetFileID();

		if ( m_ActiveFiles.Exists( fileId ) == NULL && pSrcType->mediaInfo.uInMemoryMediaSize > 0 )
		{
			m_ActiveFiles.Set( fileId );

			AkFileSystemFlags fileSystemFlags( 
				AKCOMPANYID_AUDIOKINETIC, // Company ID. 
				CODECID_FROM_PLUGINID( pSrcType->dwID ), // File/Codec type ID (defined in AkTypes.h).
				0,					// User parameter size.
				0,                  // User parameter.
				((bool)pSrcType->mediaInfo.bIsLanguageSpecific), // True when file location depends on current language.
				pSrcType->GetCacheID()
				);

			fileSystemFlags.uNumBytesPrefetch = pSrcType->mediaInfo.uInMemoryMediaSize;

			AK::IAkStreamMgr::Get()->PinFileInCache( fileId, &fileSystemFlags, m_uActivePriority );
		}
	}

	// Then update inactive sounds
	for ( AkSoundArray::Iterator soundId = inactiveSounds.Begin(); soundId != inactiveSounds.End(); ++soundId)
	{
		AkSrcTypeInfo * pSrcType = (*soundId);
		AKASSERT(pSrcType);

		AkFileID fileId = pSrcType->GetFileID();

		// Make sure we don't override the active priority
		if ( m_ActiveFiles.Exists( fileId ) == NULL && m_InactiveFiles.Exists( fileId ) == NULL &&
			pSrcType->mediaInfo.uInMemoryMediaSize > 0 )
		{
			m_InactiveFiles.Set( fileId );

			AkFileSystemFlags fileSystemFlags( 
				AKCOMPANYID_AUDIOKINETIC, // Company ID. 
				CODECID_FROM_PLUGINID( pSrcType->dwID ), // File/Codec type ID (defined in AkTypes.h).
				0,					// User parameter size.
				0,                  // User parameter.
				((bool)pSrcType->mediaInfo.bIsLanguageSpecific), // True when file location depends on current language.
				pSrcType->GetCacheID()
				);

			fileSystemFlags.uNumBytesPrefetch = pSrcType->mediaInfo.uInMemoryMediaSize;

			AK::IAkStreamMgr::Get()->PinFileInCache( fileId, &fileSystemFlags, m_uInactivePriority );
		}
	}

	//Unpin the files that were in cache before.  Reference counting in the stream manager will make sure the files are released or held as appropriate.
	for ( AkUniqueIDSet::Iterator fileIt = oldActiveFileSet.Begin(); fileIt != oldActiveFileSet.End(); ++fileIt)
	{
		AK::IAkStreamMgr::Get()->UnpinFileInCache( (*fileIt), m_uActivePriority );
	}

	for ( AkUniqueIDSet::Iterator fileIt = oldInactiveFileSet.Begin(); fileIt != oldInactiveFileSet.End(); ++fileIt)
	{
		AK::IAkStreamMgr::Get()->UnpinFileInCache( (*fileIt), m_uInactivePriority );
	}

	oldActiveFileSet.Term();
	oldInactiveFileSet.Term();

	//

	for (AkGameSyncArray::Iterator gsIt = gameSyncs.Begin(); gsIt != gameSyncs.End(); ++gsIt )
	{
		SetGameSync( *gsIt );
	}

	activeSounds.Term();
	inactiveSounds.Term();
	gameSyncs.Term();
}

void AkCachePinnedFileSet::SetGameSync( AkGameSync& in_gameSync )
{
	if ( m_SwitchGroups.Exists( in_gameSync.m_ulGroup ) == NULL )
	{
		AkCacheGameSyncMon* pMon = NULL;
		AkCacheGameSyncMonStruct* pMonStruct = g_CacheGameSyncMons.Exists( in_gameSync.m_ulGroup );
		if ( !pMonStruct )
		{
			pMonStruct = g_CacheGameSyncMons.Set(in_gameSync.m_ulGroup);
			if ( pMonStruct != NULL )
			{
				pMonStruct->pData = AkNew(AkMemID_Streaming, AkCacheGameSyncMon(in_gameSync) );
				pMon = pMonStruct->pData;
				if (pMon == NULL)
					g_CacheGameSyncMons.Unset(in_gameSync.m_ulGroup);
			}
		}
		else
		{
			pMon = pMonStruct->pData;
			AKASSERT( pMon != NULL );
			pMon->AddRef();
		}

		if (pMon != NULL)
		{
			m_SwitchGroups.Set( in_gameSync.m_ulGroup );
		}
	}
}

void AkCachePinnedFileSet::UnsetAllGameSyncs()
{
	for ( AkUniqueIDSet::Iterator sgIt = m_SwitchGroups.Begin(); sgIt != m_SwitchGroups.End(); ++sgIt )
	{
		AkUInt32 ulGroup = (*sgIt);

		AkCacheGameSyncMonStruct* pMonStruct = g_CacheGameSyncMons.Exists( ulGroup );
		if ( pMonStruct )
		{
			AKASSERT(pMonStruct->pData != NULL);
			if ( pMonStruct->pData->Release() == 0 )
			{
				pMonStruct->pData = NULL;
				g_CacheGameSyncMons.Unset(ulGroup);
			}
		}
	}

	m_SwitchGroups.RemoveAll();

	if (g_CacheGameSyncMons.IsEmpty())
		g_CacheGameSyncMons.Term();
}

void AkCachePinnedFileSet::UpdatePriority(AkPriority in_uActivePriority, AkPriority in_uInactivePriority)
{
	if ( in_uActivePriority != m_uActivePriority || in_uInactivePriority != m_uInactivePriority )
	{
		for ( AkUniqueIDSet::Iterator fileIt = m_ActiveFiles.Begin(); fileIt != m_ActiveFiles.End(); ++fileIt)
		{
			AK::IAkStreamMgr::Get()->UpdateCachingPriority( *fileIt, in_uActivePriority, m_uActivePriority );
		}
		
		for ( AkUniqueIDSet::Iterator fileIt = m_InactiveFiles.Begin(); fileIt != m_InactiveFiles.End(); ++fileIt)
		{
			AK::IAkStreamMgr::Get()->UpdateCachingPriority( *fileIt, in_uInactivePriority, m_uInactivePriority );
		}

		m_uActivePriority = in_uActivePriority;
		m_uInactivePriority = in_uInactivePriority;
	}
}
