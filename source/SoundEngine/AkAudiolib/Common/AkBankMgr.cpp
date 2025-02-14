/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkBankMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkBankMgr.h"
#include "AkEvent.h"
#include "AkActionPlay.h"
#include "AkSound.h"
#include "AkState.h"
#include "AkModulator.h"
#include "AkActorMixer.h"
#include "AkAuxBus.h"
#include "AkSwitchCntr.h"
#include "AkRanSeqCntr.h"
#include "AkLayerCntr.h"
#include "AkEnvironmentsMgr.h"
#include "AkAudioLib.h"
#include "AkAudioMgr.h"
#include "AkSwitchMgr.h"
#include "AkDialogueEvent.h"
#include "AkOutputMgr.h"
#include "AkQueuedMsg.h"
#include "AkEffectsMgr.h"
#include "AkURenderer.h"
#include "AkVirtualAcousticsManager.h"

#include "AkProfile.h"

#include "../../../Codecs/AkVorbisDecoder/Common/AkVorbisInfo.h"
#include "../../../Codecs/AkOpusDecoder/OpusCommon.h"
#include "AkVPLSrcNode.h"
#include "AkPluginCodec.h"

extern AkExternalBankHandlerCallback g_pExternalBankHandlerCallback;
extern AkPlatformInitSettings g_PDSettings;
extern AkInitSettings g_settings;

#ifdef AK_ATRAC9_SUPPORTED
	// On Sony platforms, we load banks aligned on AK_BANK_PLATFORM_DATA_ALIGNMENT (256)
	// to support having ATRAC9 in any bank on a sound by sound basis. When no ATRAC9 is
	// implied, it is not required.

	// Case where we have the codec ID
	#define AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_CODECID( __alignment__, __codecID__ )		\
		if ( (__codecID__) != AKCODECID_ATRAC9 )											\
		{																					\
			__alignment__ = AK_BANK_PLATFORM_DATA_NON_ATRAC9_ALIGNMENT;						\
		}

	// If the sound is not exactly a multiple of 256, it is not ATRAC9,
	// and we can optimize by changing the memory alignment constraint.
	#define AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_MEDIA_SIZE( __alignment__, __mediaSize__ )	\
		if( __mediaSize__ % AK_BANK_PLATFORM_DATA_ALIGNMENT )								\
		{																					\
			__alignment__ = AK_BANK_PLATFORM_DATA_NON_ATRAC9_ALIGNMENT;						\
		}

	// Case where we may or may not have the Codec ID
	#define AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_CODECID_OR_MEDIA_SIZE( __alignment__, __codecID__, __mediaSize__ )	\
		if ( (__codecID__) == 0 )																					\
		{																											\
			AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_MEDIA_SIZE( (__alignment__), (__mediaSize__) )						\
		}																											\
		else																										\
		{																											\
			AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_CODECID( (__alignment__), (__codecID__) );							\
		}

#else
	// Nothing to do on other platforms
	#define AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_CODECID( __alignment__, __codecID__ )
	#define AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_MEDIA_SIZE( __alignment__, __mediaSize__ )
	#define AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_CODECID_OR_MEDIA_SIZE( __alignment__, __codecID__, __mediaSize__ )
#endif

template < typename tChar >
bool HasFileExtension(const tChar* in_pStr, size_t in_uStrSize, const tChar* in_pFileExt, size_t in_uFileExtSize)
{
	bool bStrHasFileExt = false;
	if (in_uStrSize > in_uFileExtSize)
	{
		bStrHasFileExt = true;
		const tChar* pStr = in_pStr + in_uStrSize - in_uFileExtSize;
		const tChar* pFileExt = in_pFileExt;
		for (AkUInt32 i = 0; i < in_uFileExtSize; ++i)
		{
			if (*pStr != *pFileExt)
			{
				bStrHasFileExt = false;
				break;
			}

			++pStr;
			++pFileExt;
		}
	}
	return bStrHasFileExt;
}

static bool HasFileExtension(const char* in_pStr, const char* in_pFileExt)
{
	return HasFileExtension(in_pStr, strlen(in_pStr), in_pFileExt, strlen(in_pFileExt));
}

static AkUInt32 GetBankAlignmentFromHeader(AkBankHeader &in_hdr)
{
	AkUInt32 uAlignment = AK_BANK_PLATFORM_DATA_ALIGNMENT;
	if (in_hdr.dwBankGeneratorVersion >= AK_BANK_READER_VERSION_MEDIA_ALIGNMENT_FIELD)
	{
		uAlignment = in_hdr.uAlignment;
	}
#ifdef AK_PS4
	// In situations where the user do not wish to force alignment if he is not using ATRAC, we still must at minimum honor SSE restrictions for plug-in data processing data directly from bank memory.
	else if (!g_PDSettings.bStrictAtrac9Aligment)
	{
		uAlignment = AK_BANK_PLATFORM_DATA_NON_ATRAC9_ALIGNMENT;
	}
#endif

	return uAlignment;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT AkFileNameString::Copy(const wchar_t* in_pBankStr, const wchar_t* in_pFileExt)
{
	AKRESULT res = AK_Success;
	Term();
	if (in_pBankStr != NULL)
	{
		AkUInt32 stringSize = (AkUInt32)wcslen(in_pBankStr);
		AkUInt32 fileExtSize = in_pFileExt != NULL ? (AkUInt32)wcslen(in_pFileExt) : 0;

		bool bAddFileExt = !HasFileExtension(in_pBankStr, stringSize, in_pFileExt, fileExtSize);
		if (!bAddFileExt)
			fileExtSize = 0;

		char* pStr = NULL;
		size_t uBufSize = (stringSize + 1) * sizeof(char) + (bAddFileExt ? fileExtSize*sizeof(char) : 0);
		pStr = (char*)AkAlloc(AkMemID_Object, uBufSize);
		if (pStr)
		{
			int writtenBytes = AkWideCharToChar(in_pBankStr, (AkUInt32)uBufSize, pStr);

			if (bAddFileExt)
				writtenBytes += AkWideCharToChar(in_pFileExt, (AkUInt32)uBufSize - writtenBytes, pStr + writtenBytes);

			pStr[uBufSize-1] = 0;

			m_pStr = pStr;
			m_bOwner = true;
		}
		else
		{
			res = AK_InsufficientMemory;
		}
	}

	return res;
}
#endif

AKRESULT AkFileNameString::Copy(const char* in_pBankStr, const char* in_pFileExt)
{
	AKRESULT res = AK_Success;
	Term();
	if (in_pBankStr != NULL)
	{
		AkUInt32 stringSize = (AkUInt32)strlen(in_pBankStr);
		AkUInt32 fileExtSize = in_pFileExt != NULL ? (AkUInt32)strlen(in_pFileExt) : 0;

		bool bAddFileExt = !HasFileExtension(in_pBankStr, stringSize, in_pFileExt, fileExtSize);

		size_t uBufSize = (stringSize + 1) * sizeof(char) + (bAddFileExt? fileExtSize*sizeof(char) : 0);

		char* pStr = (char*)AkAlloc(AkMemID_Object, uBufSize);
		if (pStr)
		{
			memcpy(pStr, in_pBankStr, stringSize * sizeof(char));

			if (bAddFileExt)
				memcpy(pStr + stringSize, in_pFileExt, fileExtSize * sizeof(char));

			pStr[uBufSize-1] = 0;

			m_pStr = pStr;
			m_bOwner = true;
		}
		else
		{
			res = AK_InsufficientMemory;
		}
	}

	return res;
}

AKRESULT AkFileNameString::Set(const char* in_pString, const char* in_pFileExt)
{
	AKRESULT res = AK_Success;
	Term();
	if (!HasFileExtension(in_pString, in_pFileExt))
	{
		res = Copy(in_pString, in_pFileExt);
	}
	else
	{
		m_pStr = in_pString;
		m_bOwner = false;
	}
	return res;
}

void AkFileNameString::Term()
{
	if (m_bOwner)
		AkFree(AkMemID_Object, (void*)m_pStr);

	m_pStr = NULL;
	m_bOwner = false;
}



CAkUsageSlot::CAkUsageSlot( AkBankKey in_BankKey, AkInt32 in_mainRefCount, AkInt32 in_prepareEventRefCount, AkInt32 in_prepareBankRefCount )
	: key( in_BankKey.bankID, in_BankKey.pInMemoryPtr )
	, m_pData( NULL )
	, m_paLoadedMedia( NULL )
	, m_uLoadedDataSize( 0 )
	, m_uLoadedMetaDataSize( 0 )
	, m_bUsageProhibited(false)
	, m_uNumLoadedItems( 0 )
	, m_uIndexSize( 0 )
	, m_pfnBankCallback( NULL )
	, m_pCookie( NULL )
	, m_iRefCount( in_mainRefCount )
	, m_iPrepareRefCount( in_prepareEventRefCount+in_prepareBankRefCount )
	, m_bWasLoadedAsABank( in_mainRefCount != 0 )
	, m_bWasIndexAllocated( false )
	, m_bIsMediaPrepared( false )
	, m_bUseDeviceMemory( false )
	, m_uAlignment(AK_BANK_PLATFORM_DATA_ALIGNMENT)
	, m_iWasPreparedAsABankCounter( in_prepareBankRefCount )
{
}

CAkUsageSlot::~CAkUsageSlot()
{
	CheckFreeIndexArray();
}

void CAkUsageSlot::AddRef()
{
	// TODO
	// maybe we should increment only if one of the two count is non zero.
	// Which also suggest the AddRef could fail... which would be an issue too.
	// ...kind of in the middle of some problem here...
	// Must check where the AddRefs are done.
	AkAtomicInc32( &m_iRefCount );
}

void CAkUsageSlot::AddRefPrepare()
{
	AkAtomicInc32( &m_iPrepareRefCount );
}

void CAkUsageSlot::Release( bool in_bSkipNotification )
{
	CAkBankList::Lock();

	AkInt32 iNewRefCount = AkAtomicDec32( &m_iRefCount );
	AKASSERT( iNewRefCount >= 0 );

	if ( iNewRefCount <= 0 )
	{
		Unload();
		g_pBankManager->UnloadMedia( this );

		if( m_iPrepareRefCount <= 0 )
		{
			CAkBankList::Unlock();
			MONITOR_LOADEDBANK( this, true );

			if( !in_bSkipNotification )
			{
				UnloadCompletionNotification();
			}

			AkDelete(AkMemID_Object, this );
		}
		else
		{
			CAkBankList::Unlock();
			MONITOR_LOADEDBANK( this, false );
			if( !in_bSkipNotification )
			{
				UnloadCompletionNotification();
			}
		}
	}
	else
	{
		CAkBankList::Unlock();
	}
}

// ReleasePrepare() is callon only by the Bank thread.
void CAkUsageSlot::ReleasePrepare( bool in_bIsFinal /*= false*/ )
{
	CAkBankList::Lock();
	AKASSERT( m_iPrepareRefCount != 0 );

	AkInt32 iNewRefCount = 0;
	if( in_bIsFinal )
		m_iPrepareRefCount = 0;
	else
		iNewRefCount = AkAtomicDec32( &m_iPrepareRefCount );

	AKASSERT( iNewRefCount >= 0 );

	if ( iNewRefCount <= 0 )
	{
		g_pBankManager->UnPrepareMedia( this );
		if( m_iRefCount <= 0 )
		{
			g_pBankManager->m_BankList.Remove( AkBankKey(key.bankID, NULL) );
			CAkBankList::Unlock();
			RemoveContent();
			Unload();

			MONITOR_LOADEDBANK( this, true );

			AkDelete(AkMemID_Object, this );
		}
		else
		{
			CAkBankList::Unlock();
		}
	}
	else
	{
		CAkBankList::Unlock();
	}
}

void CAkUsageSlot::RemoveContent()
{
	AkListLoadedItem::Iterator iter = m_listLoadedItem.Begin();
	while( iter != m_listLoadedItem.End() )
	{
		//To avoid locking for EACH object we release, break the lock only once in a while.
		//This will allow the audio thread to do its stuff normally.  Anyway, releasing should not be long.
		AkUInt16 uLockCount = 1;
		CAkFunctionCritical SpaceSetAsCritical;
		while( iter != m_listLoadedItem.End() && (uLockCount & 0x00FF))
		{
			// At this point, only sounds, track and music objects are remaining
			CAkIndexable* pNode = static_cast<CAkIndexable*>(*iter);
			pNode->Release();

			++iter;
			++uLockCount;
		}
	}
	m_listLoadedItem.Term();
}

void CAkUsageSlot::DeleteDataBlock()
{
	if( m_pData )
	{
		AkMemPoolId poolId = AkMemID_Media | (m_bUseDeviceMemory ? AkMemType_Device : 0);
		AkFalign(poolId, m_pData);
		m_pData = NULL;
	}
}

void CAkUsageSlot::Unload()
{
	if ( ( m_pData ) || ( m_uLoadedDataSize ) )
	{
		AK_PERF_DECREMENT_BANK_MEMORY( m_uLoadedDataSize );
	}

	DeleteDataBlock();
}

void CAkUsageSlot::UnloadCompletionNotification()
{
	MONITOR_BANKNOTIF( key.bankID, AK_INVALID_UNIQUE_ID, AkMonitorData::NotificationReason_BankUnloaded );
	// Notify user.

	if( m_pfnBankCallback )
	{
		g_pBankManager->DoCallback( m_pfnBankCallback, key.bankID, key.pInMemoryPtr, AK_Success, m_pCookie );
		m_pfnBankCallback = NULL;// to make sure the notification is sent only once
	}
}

void CAkUsageSlot::CheckFreeIndexArray()
{
	if( WasIndexAllocated() )
	{
		AkFree(AkMemID_Object, m_paLoadedMedia );
		WasIndexAllocated(false);
	}
	m_paLoadedMedia = NULL;
}

CAkBankMgr::CAkBankMgr()	
	: m_bAccumulating( false )
	, m_OperationResult( AK_Success )
{
}

CAkBankMgr::~CAkBankMgr()
{
}

AKRESULT CAkBankMgr::Init()
{
	if(!m_BankList.Init())
		return AK_InsufficientMemory;
	return m_BankReader.Init();
}

void CAkBankMgr::Term()
{
	UnloadAll();

	m_BankList.Term();

	AKASSERT( m_MediaHashTable.Length() == 0 );

	m_MediaHashTable.Term();

	FlushFileNameTable();

	m_BankReader.Term();

	m_PreparationAccumulator.Term();
}

void CAkBankMgr::FlushFileNameTable()
{
	for( AkIDtoStringHash::Iterator iter = m_BankIDToFileName.Begin(); iter != m_BankIDToFileName.End(); ++iter )
	{
		AKASSERT( iter.pItem->Assoc.item );
		AkFree(AkMemID_Object, iter.pItem->Assoc.item );
	}
	m_BankIDToFileName.Term();
}

//====================================================================================================
//====================================================================================================

AKRESULT CAkBankMgr::ExecuteCommand(CAkBankMgr::AkBankQueueItem& in_Item)
{
	WWISE_SCOPED_PROFILE_MARKER("CAkBankMgr::ExecuteCommand");

	AKRESULT eResult = AK_Success;

	switch ( in_Item.eType )
	{
	case QueueItemLoad:
		eResult = LoadBankPre( in_Item );
		break;

	case QueueItemUnload:
		eResult = UnloadBankPre( in_Item );
		break;

	case QueueItemPrepareEvent:
		eResult = PrepareEvents( in_Item );
		break;

	case QueueItemUnprepareEvent:
		eResult = UnprepareEvents( in_Item );
		break;

	case QueueItemSupportedGameSync:
		eResult = PrepareGameSync( in_Item );
		break;

	case QueueItemUnprepareAllEvents:
		eResult = UnprepareAllEvents( in_Item );
		break;

	case QueueItemClearBanks:
		eResult = ClearBanksInternal( in_Item );
		break;

	case QueueItemPrepareBank:
		eResult = PrepareBank( in_Item );
		break;

	case QueueItemUnprepareBank:
		eResult = UnPrepareBank( in_Item );
		break;

	case QueueItemLoadMediaFile:
		eResult = LoadMediaFile( in_Item );
		break;

	case QueueItemUnloadMediaFile:
		eResult = UnloadMediaFile( in_Item );
		break;

	case QueueItemLoadMediaFileSwap:
		eResult = MediaSwap( in_Item );
		break;

	default:
		break;
	}

	return eResult;
}

void CAkBankMgr::BankMonitorNotification( AkBankQueueItem & in_Item )
{
	switch( in_Item.eType )
	{
	case QueueItemLoad:
		MONITOR_BANKNOTIF_NAME( in_Item.bankID, AK_INVALID_UNIQUE_ID, AkMonitorData::NotificationReason_BankLoadRequestReceived, in_Item.GetBankString() );
		break;

	case QueueItemUnload:
		MONITOR_BANKNOTIF_NAME(in_Item.bankID, AK_INVALID_UNIQUE_ID, AkMonitorData::NotificationReason_BankUnloadRequestReceived, in_Item.GetBankString());
		break;

	case QueueItemPrepareEvent:
		MONITOR_PREPARENOTIFREQUESTED( AkMonitorData::NotificationReason_PrepareEventRequestReceived, in_Item.prepare.numEvents );
		break;

	case QueueItemUnprepareEvent:
		MONITOR_PREPARENOTIFREQUESTED( AkMonitorData::NotificationReason_UnPrepareEventRequestReceived, in_Item.prepare.numEvents );
		break;

	case QueueItemSupportedGameSync:
		if( !in_Item.gameSync.bSupported )
		{
			MONITOR_PREPARENOTIFREQUESTED( AkMonitorData::NotificationReason_UnPrepareGameSyncRequested, in_Item.gameSync.uNumGameSync );
		}
		else
		{
			MONITOR_PREPARENOTIFREQUESTED( AkMonitorData::NotificationReason_PrepareGameSyncRequested, in_Item.gameSync.uNumGameSync );
		}
		break;

	case QueueItemUnprepareAllEvents:
		MONITOR_PREPARENOTIFREQUESTED( AkMonitorData::NotificationReason_ClearPreparedEventsRequested, 0 );
		break;

	case QueueItemClearBanks:
		MONITOR_BANKNOTIF( AK_INVALID_UNIQUE_ID, AK_INVALID_UNIQUE_ID, AkMonitorData::NotificationReason_ClearAllBanksRequestReceived );
		break;

	case QueueItemPrepareBank:
		MONITOR_PREPAREBANKREQUESTED(  AkMonitorData::NotificationReason_PrepareBankRequested, in_Item.bankID, in_Item.bankPreparation.uFlags );
		break;

	case QueueItemUnprepareBank:
		MONITOR_PREPAREBANKREQUESTED(  AkMonitorData::NotificationReason_UnPrepareBankRequested, in_Item.bankID, in_Item.bankPreparation.uFlags );
		break;

	default:
		break;
	}
}

void CAkBankMgr::NotifyCompletion( AkBankQueueItem & in_rItem, AKRESULT in_OperationResult )
{
	AkUniqueID itemID = AK_INVALID_UNIQUE_ID;
	switch( in_rItem.eType )
	{
	case QueueItemLoad:
	case QueueItemUnload:
		itemID = in_rItem.bankID;
		break;

	case QueueItemPrepareBank:
	case QueueItemUnprepareBank:
		itemID = in_rItem.bankID;
		break;

	case QueueItemPrepareEvent:
	case QueueItemUnprepareEvent:
		itemID = (in_rItem.prepare.numEvents == 1) ? in_rItem.prepare.eventID : AK_INVALID_UNIQUE_ID;
		break;

	case QueueItemSupportedGameSync:
	case QueueItemUnprepareAllEvents:
	case QueueItemClearBanks:
	case QueueItemLoadMediaFile:
	case QueueItemUnloadMediaFile:
	case QueueItemLoadMediaFileSwap:
		// Nothing
		// different than the "default:" case as we want to make sure everyone is properly handled.
		break;

	default:
		AKASSERT(!"Unhandled AkBankQueueItemType in NotifyCompletion.");
		break;
	}

	DoCallback( in_rItem.callbackInfo.pfnBankCallback, itemID, in_rItem.GetFromMemPtr(), in_OperationResult, in_rItem.callbackInfo.pCookie );
}

AKRESULT CAkBankMgr::SetFileReader( AkFileID in_FileID, const char* in_strBankName, AkUInt32 in_uFileOffset, AkUInt32 in_codecID, void * in_pCookie, bool in_bIsLanguageSpecific /*= true*/ )
{
	// We are in a Prepare Event or Prepare Bank or Load Bank by String.

	if( in_uFileOffset != 0 || in_codecID == AKCODECID_BANK )
	{
		// If we have the bank name, we use the string, otherwise, we use the file ID directly.
		if (in_strBankName)
		{
			return m_BankReader.SetFile(in_strBankName, in_uFileOffset, in_pCookie);
		}
		else
		{
			return m_BankReader.SetFile( in_FileID, in_uFileOffset, AKCODECID_BANK, in_pCookie );
		}
	}
	else
	{
		AKASSERT( in_codecID != AKCODECID_BANK );
		return m_BankReader.SetFile( in_FileID, in_uFileOffset, in_codecID, in_pCookie, in_bIsLanguageSpecific );
	}
}

AKRESULT CAkBankMgr::LoadSoundFromFile( AkSrcTypeInfo& in_rMediaInfo, AkUInt8* io_pData )
{
	m_BankReader.Reset();

	AKRESULT eResult = SetFileReader( in_rMediaInfo.mediaInfo.uFileID, NULL, 0, CODECID_FROM_PLUGINID( in_rMediaInfo.dwID ), NULL, in_rMediaInfo.mediaInfo.bIsLanguageSpecific );

	AkUInt32 uReadSize = 0;
	if( eResult == AK_Success )
	{
		eResult = m_BankReader.FillData( io_pData, in_rMediaInfo.mediaInfo.uInMemoryMediaSize, uReadSize );
	}
	if( eResult == AK_Success && in_rMediaInfo.mediaInfo.uInMemoryMediaSize != uReadSize )
		eResult = AK_Fail;

	m_BankReader.CloseFile();

	return eResult;
}

void LogWrongBankVersion(AkUInt32 in_bankVersion)
{
	AkOSChar wszBuffer[AK_MAX_STRING_SIZE];
	static const AkOSChar* format = AKTEXT("Load bank failed : incompatible bank version. Bank was generated with %s version of Wwise. The Bank version is %d and the current SDK version is %d");
	static const AkOSChar* olderBank_format = AKTEXT("an older");
	static const AkOSChar* newerBank_format = AKTEXT("a newer");

	if (in_bankVersion < AK_BANK_READER_VERSION)
		AK_OSPRINTF(wszBuffer, AK_MAX_STRING_SIZE, format, olderBank_format, in_bankVersion, AK_BANK_READER_VERSION);
	else
		AK_OSPRINTF(wszBuffer, AK_MAX_STRING_SIZE, format, newerBank_format, in_bankVersion, AK_BANK_READER_VERSION);

	MONITOR_ERRORMSG(wszBuffer);
}

AKRESULT CAkBankMgr::LoadBank( AkBankQueueItem in_Item, CAkUsageSlot *& out_pUsageSlot, AkLoadBankDataMode in_eLoadMode, bool in_bIsFromPrepareBank, bool in_bDecode )
{
	AkBankID bankID = in_Item.bankID;

	CAkBankList::Lock();

	CAkUsageSlot* pSlot = m_BankList.Get( in_Item.GetBankKey() );
	if( pSlot && pSlot->WasLoadedAsABank() )
	{
		CAkBankList::Unlock();
		AKASSERT( in_eLoadMode == AkLoadBankDataMode_OneBlock );// If the call is not preparing media, then it should not pass here.
		MONITOR_BANKNOTIF( bankID, AK_INVALID_UNIQUE_ID, AkMonitorData::NotificationReason_BankAlreadyLoaded );
		return AK_BankAlreadyLoaded;
	}

	bool bDone = false;

	AKRESULT eResult = AK_Success;
	CAkBankList::Unlock();

#ifndef AK_OPTIMIZED
	AK::MemoryMgr::StartProfileThreadUsage();
#endif

	if( pSlot == NULL )
	{
		AkInt32 uMainRefCount = 0;
		AkInt32 uPrepareEventRefCount = 0;
		AkInt32 uPrepareBankRefCount = 0;

		if( in_eLoadMode == AkLoadBankDataMode_OneBlock )
		{
			uMainRefCount = 1;
		}
		else if( in_bIsFromPrepareBank )
		{
			uPrepareBankRefCount=1;
		}
		else
		{
			uPrepareEventRefCount = 1;
		}

		out_pUsageSlot = AkNew(AkMemID_Object, CAkUsageSlot( in_Item.GetBankKey(), uMainRefCount, uPrepareEventRefCount, uPrepareBankRefCount ) );

		if ( !out_pUsageSlot )
		{
			eResult = AK_InsufficientMemory;
		}
	}
	else
	{
		out_pUsageSlot = pSlot;
		if( in_bIsFromPrepareBank )
		{
			out_pUsageSlot->WasPreparedAsABank( true );
		}
	}

	m_BankReader.Reset();

	bool bIsLoadedFromMemory =	in_Item.bankLoadFlag == AkBankLoadFlag_FromMemoryInPlace ||
								in_Item.bankLoadFlag == AkBankLoadFlag_FromMemoryOutOfPlace;
	bool bIsInPlaceLoad = in_Item.bankLoadFlag == AkBankLoadFlag_FromMemoryInPlace;

	if(eResult == AK_Success)
	{
		switch( in_eLoadMode )
		{
		case AkLoadBankDataMode_OneBlock:
			if( in_Item.bankLoadFlag == AkBankLoadFlag_UsingFileID )
			{
				eResult = m_BankReader.SetFile( bankID, 0, AKCODECID_BANK, in_Item.callbackInfo.pCookie );
				break;
			}
			else if( bIsLoadedFromMemory )
			{
				eResult = m_BankReader.SetFile( in_Item.load.pInMemoryBank, in_Item.load.ui32InMemoryBankSize );
				break;
			}
			// No break here intentionnal, default to SetFileReader(

		default:
			eResult = SetFileReader( bankID, in_Item.GetBankString(), 0, AKCODECID_BANK, in_Item.callbackInfo.pCookie );
			break;
		}
	}

	// Getting the bank information from its header
	AkBankHeader BankHeaderInfo = {0};
	bool bBackwardDataCompatibilityMode = false;
	if(eResult == AK_Success)
	{
		eResult = ProcessBankHeader( BankHeaderInfo, bBackwardDataCompatibilityMode );
	}

	// Loading the Bank
	while (!bDone && (eResult == AK_Success))
	{
		AkSubchunkHeader SubChunkHeader;

		AkUInt32 ulTotalRead = 0;
		AkUInt32 ulReadBytes = 0;

		eResult = m_BankReader.FillData(&SubChunkHeader, sizeof(SubChunkHeader), ulReadBytes);
		ulTotalRead += ulReadBytes;
		if(eResult != AK_Success)
		{
			break;
		}
		if(ulTotalRead == sizeof(SubChunkHeader))
		{
			if ( bBackwardDataCompatibilityMode )
			{
				// In Backward Compatibility mode, we accept only banks that contains DATA information.
				// All other type of content is not supported.
				// Note that enabling this mode brings some risk as we will load DATA without knowing what it is.
				// The user must guarantee that the content that will be attempted to be loaded will still be compatible, because we will load it anyway.
				// Failure to do so could lead to Decoders and/or plug-ins to try to work with a data format that is unexpected.
				if (SubChunkHeader.dwTag != BankDataChunkID && SubChunkHeader.dwTag != BankDataIndexChunkID)
				{
					LogWrongBankVersion(BankHeaderInfo.dwBankGeneratorVersion);
					eResult = AK_WrongBankVersion;
					break;
				}
			}
			switch( SubChunkHeader.dwTag )
			{
			case BankDataChunkID:

				out_pUsageSlot->SetAligment(GetBankAlignmentFromHeader(BankHeaderInfo));
#ifdef AK_XBOX
				if (BankHeaderInfo.bDeviceAllocated)
					out_pUsageSlot->SetUseDeviceMemory();
#endif

				switch( in_eLoadMode )
				{
				case AkLoadBankDataMode_Structure:
					m_BankReader.Skip(SubChunkHeader.dwChunkSize, ulReadBytes);
					if(ulReadBytes != SubChunkHeader.dwChunkSize)
					{
						eResult = AK_InvalidFile;
						bDone = true;
					}
					break;

				case AkLoadBankDataMode_MediaAndStructure:
				case AkLoadBankDataMode_Media:
					AKASSERT( bIsLoadedFromMemory == false );

					eResult = PrepareMedia(
						out_pUsageSlot,
						SubChunkHeader.dwChunkSize, // required to skip if already loaded...
						in_bDecode );
					break;

				case AkLoadBankDataMode_OneBlock:
					AkUInt8* pDataBank = NULL;
					if( bIsLoadedFromMemory && bIsInPlaceLoad )
					{
						//Bank loaded from memory
						pDataBank = (AkUInt8*)m_BankReader.GetData( SubChunkHeader.dwChunkSize );
						out_pUsageSlot->m_uLoadedDataSize = SubChunkHeader.dwChunkSize;
						AK_PERF_INCREMENT_BANK_MEMORY( out_pUsageSlot->m_uLoadedDataSize );
						m_BankReader.ReleaseData();
					}
					else
					{
						//Bank loaded from file
						eResult = ProcessDataChunk(SubChunkHeader.dwChunkSize, out_pUsageSlot);
						pDataBank = out_pUsageSlot->m_pData;
					}

					if( eResult == AK_Success )
					{
						eResult = LoadMedia(
							pDataBank,
							out_pUsageSlot
							);
					}
					break;
				}
				break;

			case BankDataIndexChunkID:
				switch( in_eLoadMode )
				{
				case AkLoadBankDataMode_Structure:
					m_BankReader.Skip(SubChunkHeader.dwChunkSize, ulReadBytes);
					if( ulReadBytes != SubChunkHeader.dwChunkSize )
					{
						eResult = AK_InvalidFile;
						bDone = true;
					}
					break;

				case AkLoadBankDataMode_MediaAndStructure:
				case AkLoadBankDataMode_Media:
					AKASSERT( bIsLoadedFromMemory == false );
					// No break here normal.
				case AkLoadBankDataMode_OneBlock:
					eResult = LoadMediaIndex( out_pUsageSlot, SubChunkHeader.dwChunkSize, bIsInPlaceLoad );
					break;
				}
				break;

			case BankHierarchyChunkID:
				eResult = ProcessHircChunk( out_pUsageSlot, bankID );
				break;

			case BankStrMapChunkID:
				eResult = ProcessStringMappingChunk( SubChunkHeader.dwChunkSize, out_pUsageSlot );
				break;

			case BankStateMgrChunkID:
				eResult = ProcessGlobalSettingsChunk( SubChunkHeader.dwChunkSize );
				break;

			case BankEnvSettingChunkID:
				eResult = ProcessEnvSettingsChunk( SubChunkHeader.dwChunkSize );
				break;

			case BankCustomPlatformName:
				eResult = ProcessCustomPlatformChunk( SubChunkHeader.dwChunkSize );
				break;

			case BankInitChunkID:
				eResult = ProcessPluginChunk(SubChunkHeader.dwChunkSize);
				break;
			default:
				AKASSERT(!"Unknown Bank chunk for this Reader version, it will be ignored");
				//Skip it
				m_BankReader.Skip(SubChunkHeader.dwChunkSize, ulReadBytes);
				if(ulReadBytes != SubChunkHeader.dwChunkSize)
				{
					eResult = AK_InvalidFile;
					bDone = true;
				}
				break;

			}
		}
		else
		{
			if(ulReadBytes != 0)
			{
				AKASSERT(!"Should not happen on a valid file");
				eResult = AK_InvalidFile;
			}
			bDone = true;
		}
	}

	m_BankReader.CloseFile();

#ifndef AK_OPTIMIZED
	//Profile the amount of memory this bank takes.
	AkUInt32 uLoadedMetaDataSize = (AkUInt32)AK::MemoryMgr::StopProfileThreadUsage();
	if ( out_pUsageSlot )
		out_pUsageSlot->m_uLoadedMetaDataSize = uLoadedMetaDataSize;
#endif

	AkMonitorData::NotificationReason Reason;
	AkUniqueID l_langageID = AK_INVALID_UNIQUE_ID;
	if(eResult == AK_Success)
	{
		Reason = AkMonitorData::NotificationReason_BankLoaded;
		l_langageID = BankHeaderInfo.dwLanguageID;
	}
	else
	{
		Reason = AkMonitorData::NotificationReason_BankLoadFailed;
		MONITOR_ERROR( AK::Monitor::ErrorCode_BankLoadFailed );
	}
	MONITOR_BANKNOTIF( bankID, l_langageID, Reason );

	return eResult;
}

AKRESULT CAkBankMgr::LoadBankPre( AkBankQueueItem& in_rItem )
{
	AKRESULT eLoadResult = AK_Success;

	CAkUsageSlot * pUsageSlot = NULL;
	AKRESULT eResult = LoadBank( in_rItem, pUsageSlot, AkLoadBankDataMode_OneBlock, false,false );
	if( eResult == AK_BankAlreadyLoaded )
	{
		eResult = AK_Success;
		eLoadResult = AK_BankAlreadyLoaded;
	}
	else if ( eResult == AK_Success )
	{
		pUsageSlot->WasLoadedAsABank( true );

		m_BankList.Set( in_rItem.GetBankKey(), pUsageSlot );
	}

	MONITOR_LOADEDBANK( pUsageSlot, false );

	if( eResult != AK_Success )
	{
		// Here, should not pass the notification flag.
		if ( pUsageSlot )
		{
			pUsageSlot->RemoveContent();
			pUsageSlot->Release( true );
		}

		eLoadResult = eResult;
	}

	// Notify user.
	NotifyCompletion( in_rItem, eLoadResult );

	return eResult;
}

AKRESULT CAkBankMgr::UnloadBankPre( AkBankQueueItem in_Item )
{
	AKRESULT eResult = AK_Success;

	CAkBankList::Lock();

	AkBankKey bankKey = in_Item.GetBankKey();

	CAkUsageSlot * pUsageSlot = m_BankList.Get( bankKey );
	if( pUsageSlot )
	{
		if( pUsageSlot->WasLoadedAsABank() )
		{
			m_BankList.Remove( bankKey );
			CAkBankList::Unlock();
			eResult = KillSlot(pUsageSlot, in_Item.callbackInfo.pfnBankCallback, in_Item.callbackInfo.pCookie);
		}
		else
		{
			CAkBankList::Unlock();
			eResult = AK_Fail;

			// Notify user.
			NotifyCompletion( in_Item, eResult );
		}
	}
	else // not found
	{
		CAkBankList::Unlock();
		eResult = AK_UnknownBankID;

		MONITOR_ERRORMSG( AKTEXT("Unload bank failed, requested bank was not found.") );

		// Notify user.
		NotifyCompletion( in_Item, eResult );
	}

	return eResult;
}

AKRESULT CAkBankMgr::ClearBanksInternal( AkBankQueueItem in_Item )
{
	int i = 0;
	CAkUsageSlot** l_paSlots = NULL;
	{
		AkAutoLock<CAkBankList> BankListGate( m_BankList );

		CAkBankList::AkListLoadedBanks& rBankList = m_BankList.GetUNSAFEBankListRef();

		AkUInt32 l_ulNumBanks = rBankList.Length();

		if( l_ulNumBanks )
		{
			l_paSlots = (CAkUsageSlot**)AkAlloca( l_ulNumBanks * sizeof( CAkUsageSlot* ) );
			{
				CAkBankList::AkListLoadedBanks::IteratorEx iter = rBankList.BeginEx();
				while(iter != rBankList.End() )
				{
					if( (*iter)->WasLoadedAsABank() )
					{
						l_paSlots[i] = *iter;
						++i;
						iter = rBankList.Erase(iter);
					}
					else
						++iter;
				}
			}
		}
	}

	i--;

	// unloading in reverse order
	while ( i >= 0 )
	{
		KillSlotSync(l_paSlots[i]);
		i--;
	}

	NotifyCompletion( in_Item, AK_Success );

	return AK_Success;
}

AKRESULT CAkBankMgr::PrepareEvents( AkBankQueueItem in_Item )
{
	AKRESULT eResult = AK_Success;

	AKASSERT( in_Item.prepare.numEvents );

	EnableAccumulation();

	if( in_Item.prepare.numEvents == 1 )
	{
		eResult = PrepareEvent( in_Item, in_Item.prepare.eventID );

		if( eResult == AK_Success )
		{
			eResult = ProcessAccumulated();

			if( eResult != AK_Success )
			{
				UnprepareEvent( in_Item.prepare.eventID );
			}
		}

		if( eResult == AK_Success )
		{
			MONITOR_PREPAREEVENTNOTIF( AkMonitorData::NotificationReason_EventPrepareSuccess, in_Item.prepare.eventID );
		}
		else
		{
			MONITOR_PREPAREEVENTNOTIF( AkMonitorData::NotificationReason_EventPrepareFailure, in_Item.prepare.eventID );
		}
	}
	else
	{
		// Multiple events to prepare
		AKASSERT( in_Item.prepare.pEventID );

		for( AkUInt32 i = 0; i < in_Item.prepare.numEvents; ++i )
		{
			eResult = PrepareEvent( in_Item, in_Item.prepare.pEventID[i] );
			if( eResult != AK_Success )
			{
				while( i > 0 )
				{
					--i;
					UnprepareEvent( in_Item.prepare.pEventID[i] );
				}
				break;
			}
		}

		if( eResult == AK_Success )
		{
			eResult = ProcessAccumulated();

			if( eResult != AK_Success )
			{
				// Handle Error, revert prepared nodes.
				for( AkUInt32 i = 0; i < in_Item.prepare.numEvents; ++i )
				{
					UnprepareEvent( in_Item.prepare.pEventID[i] );
				}
			}
		}

		AkMonitorData::NotificationReason eReason = eResult == AK_Success ? AkMonitorData::NotificationReason_EventPrepareSuccess : AkMonitorData::NotificationReason_EventPrepareFailure;
		for( AkUInt32 i = 0; i < in_Item.prepare.numEvents; ++i )
		{
			MONITOR_PREPAREEVENTNOTIF( eReason, in_Item.prepare.pEventID[i] );
		}

		AkFree(AkMemID_Object, in_Item.prepare.pEventID );
		in_Item.prepare.pEventID = NULL;
	}

	DisableAccumulation();

	// Notify user.
	NotifyCompletion( in_Item, eResult );

	return eResult;
}

AKRESULT CAkBankMgr::PrepareEventInternal( AkBankQueueItem& in_Item, CAkEvent* in_pEvent )
{
	AKRESULT eResult = AK_Success;
	for( CAkEvent::AkActionList::Iterator iter = in_pEvent->m_actions.Begin(); iter != in_pEvent->m_actions.End(); ++iter )
	{
		CAkAction* pAction = *iter;
		AkActionType aType = pAction->ActionType();
		if( aType == AkActionType_Play )
		{
			CAkActionPlay* pActionPlay = (CAkActionPlay*)pAction;

			in_Item.bankID = pActionPlay->GetBankID();

			//Look up file name in string table, it may be in an external bank.
			char** ppString = m_BankIDToFileName.Exists(in_Item.bankID);
			if (ppString)
				in_Item.fileName.Set(*ppString);

			eResult = PrepareBankInternal( in_Item, AkLoadBankDataMode_Structure );

			if( eResult == AK_Success )
			{
				eResult = CAkParameterNodeBase::PrepareNodeData( pActionPlay->ElementID() );
				if( eResult != AK_Success )
				{
					UnPrepareBankInternal(pActionPlay->GetBankID());
				}
			}
		}
		else if( aType == AkActionType_PlayEvent )
		{
			CAkEvent* pSubEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( pAction->ElementID() );
			if( pSubEvent )
			{
				eResult = PrepareEventInternal( in_Item, pSubEvent );
				pSubEvent->Release();
			}
			else
			{
				// Partial prepare would cause problems, bail out!
				eResult = AK_Fail;
			}
		}
		if( eResult != AK_Success )
		{
			// Iterate to undo the partial prepare
			for( CAkEvent::AkActionList::Iterator iterFlush = in_pEvent->m_actions.Begin(); iterFlush != iter; ++iterFlush )
			{
				pAction = *iterFlush;
				aType = pAction->ActionType();
				if( aType == AkActionType_Play )
				{
					CAkActionPlay* pActionPlay = (CAkActionPlay*)pAction;
					CAkParameterNodeBase::UnPrepareNodeData( pActionPlay->ElementID() );
					UnPrepareBankInternal(pActionPlay->GetBankID());
				}
				else if( aType == AkActionType_PlayEvent)
				{
					CAkEvent* pSubEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( pAction->ElementID() );
					if( pSubEvent )
					{
						UnprepareEventInternal( pSubEvent );
						pSubEvent->Release();
					}
				}
			}
			break;
		}
	}
	return eResult;
}

AKRESULT CAkBankMgr::PrepareEvent( AkBankQueueItem in_Item, AkUniqueID in_EventID )
{
	AKRESULT eResult = AK_Success;

	AKASSERT( g_pIndex );
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_EventID );
	if( pEvent )
	{
		if( !pEvent->IsPrepared() )
		{
			eResult = PrepareEventInternal( in_Item, pEvent );
			// If successfully prepared, we increment the refcount
			if( eResult == AK_Success )
			{
				AK_PERF_INCREMENT_PREPARE_EVENT_COUNT();
				pEvent->AddRef();
			}
		}

		if( eResult == AK_Success )
			pEvent->IncrementPreparedCount();

		MONITOR_EVENTPREPARED( pEvent->ID(), pEvent->GetPreparationCount() );

		pEvent->Release();
	}
	else
	{
		eResult = AK_IDNotFound;
	}
	return eResult;
}

AKRESULT CAkBankMgr::UnprepareEvents( AkBankQueueItem in_Item )
{
	AKRESULT eResult = AK_Success;

	AKASSERT( in_Item.prepare.numEvents );

	if( in_Item.prepare.numEvents == 1 )
	{
		eResult = UnprepareEvent( in_Item.prepare.eventID );
	}
	else
	{
		// Multiple events to prepare
		AKASSERT( in_Item.prepare.pEventID );

		for( AkUInt32 i = 0; i < in_Item.prepare.numEvents; ++i )
		{
			eResult = UnprepareEvent( in_Item.prepare.pEventID[i] );
			if( eResult != AK_Success )
				break;
		}

		AkFree(AkMemID_Object, in_Item.prepare.pEventID );
		in_Item.prepare.pEventID = NULL;
	}

	// Notify user.
	NotifyCompletion( in_Item, eResult );

	return eResult;
}

AKRESULT CAkBankMgr::UnprepareAllEvents( AkBankQueueItem in_Item )
{
	ClearPreparedEvents();

	// Notify user.
	NotifyCompletion( in_Item, AK_Success );

	return AK_Success;
}

AKRESULT CAkBankMgr::UnprepareEvent( AkUniqueID in_EventID )
{
	AKRESULT eResult = AK_Success;

	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_EventID );
	if( pEvent )
	{
		UnprepareEvent( pEvent );
		pEvent->Release();
	}
	else
	{
		eResult = AK_IDNotFound;
	}

	MONITOR_PREPAREEVENTNOTIF(
		eResult == AK_Success ? AkMonitorData::NotificationReason_EventUnPrepareSuccess : AkMonitorData::NotificationReason_EventUnPrepareFailure,
		in_EventID );

	return eResult;
}

void CAkBankMgr::UnprepareEventInternal( CAkEvent* in_pEvent )
{
	CAkEvent::AkActionList::Iterator iter = in_pEvent->m_actions.Begin();
	while( iter != in_pEvent->m_actions.End() )
	{
		CAkAction* pAction = *iter;
		++iter;// incrementing iter BEFORE unpreparing hirarchy and releasing the event.

		AkActionType aType = pAction->ActionType();
		if( aType == AkActionType_Play )
		{
			CAkParameterNodeBase::UnPrepareNodeData( ((CAkActionPlay*)(pAction))->ElementID() );
			UnPrepareBankInternal( ((CAkActionPlay*)(pAction))->GetBankID() );
		}
		else if( aType == AkActionType_PlayEvent)
		{
			CAkEvent* pSubEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( pAction->ElementID() );
			if( pSubEvent )
			{
				UnprepareEventInternal( pSubEvent );
				pSubEvent->Release();
			}
		}
	}
}

void CAkBankMgr::UnprepareEvent( CAkEvent* in_pEvent, bool in_bCompleteUnprepare /*= false*/ )
{
	if( in_pEvent->IsPrepared() )
	{
		if( in_bCompleteUnprepare )
		{
			in_pEvent->FlushPreparationCount();
		}
		else
		{
			in_pEvent->DecrementPreparedCount();
		}

		if( !in_pEvent->IsPrepared() )// must check again as DecrementPreparedCount() just changed it.
		{
			AK_PERF_DECREMENT_PREPARE_EVENT_COUNT();

			UnprepareEventInternal( in_pEvent );

			// Successfully unprepared, we increment decrement the refcount.
			in_pEvent->Release();
		}
	}
	MONITOR_EVENTPREPARED( in_pEvent->ID(), in_pEvent->GetPreparationCount() );
}

static void NotifyPrepareGameSync( AkPrepareGameSyncQueueItem in_Item, AKRESULT in_Result )
{
	AkMonitorData::NotificationReason reason;
	if( in_Result == AK_Success )
	{
		if( in_Item.bSupported )
			reason = AkMonitorData::NotificationReason_PrepareGameSyncSuccess;
		else
			reason = AkMonitorData::NotificationReason_UnPrepareGameSyncSuccess;
	}
	else
	{
		if( in_Item.bSupported )
			reason = AkMonitorData::NotificationReason_PrepareGameSyncFailure;
		else
			reason = AkMonitorData::NotificationReason_UnPrepareGameSyncFailure;
	}
	MONITOR_PREPAREGAMESYNCNOTIF( reason, in_Item.uGameSyncID, in_Item.uGroupID, in_Item.eGroupType );
}

AKRESULT CAkBankMgr::PrepareGameSync( AkBankQueueItem in_Item )
{
	AKRESULT eResult = AK_Success;

#ifndef AK_OPTIMIZED
	if( !g_settings.bEnableGameSyncPreparation )
	{
		MONITOR_ERRORMSG( AKTEXT("Unexpected call to PrepareGameSyncs. ")
			AKTEXT("See: \"bEnableGameSyncPreparation\" parameter in AkInitSettings for more information") );
	}
#endif

	if( in_Item.gameSync.bSupported )//do not accumulate when unloading game sync.
		EnableAccumulation();

	if( in_Item.gameSync.uNumGameSync == 1 )
	{
		eResult = g_pStateMgr->PrepareGameSync( in_Item.gameSync.eGroupType, in_Item.gameSync.uGroupID, in_Item.gameSync.uGameSyncID, in_Item.gameSync.bSupported );
		NotifyPrepareGameSync( in_Item.gameSync, eResult );

		if( in_Item.gameSync.bSupported &&eResult == AK_Success )
		{
			eResult = ProcessAccumulated();

			if( eResult != AK_Success )
			{
				// Handle Error, revert prepared nodes.
				g_pStateMgr->PrepareGameSync( in_Item.gameSync.eGroupType, in_Item.gameSync.uGroupID, in_Item.gameSync.uGameSyncID, false );
			}
		}
	}
	else
	{
		AKASSERT( in_Item.gameSync.uNumGameSync && in_Item.gameSync.pGameSyncID );

		for( AkUInt32 i = 0; i < in_Item.gameSync.uNumGameSync; ++i )
		{
			eResult = g_pStateMgr->PrepareGameSync( in_Item.gameSync.eGroupType, in_Item.gameSync.uGroupID, in_Item.gameSync.pGameSyncID[i], in_Item.gameSync.bSupported );
			if( eResult != AK_Success )
			{
				// Rollback if was adding content and one failed
				AKASSERT( in_Item.gameSync.bSupported );// Must not fail when unsupporting

				for( AkUInt32 k = 0; k < i; ++k )
				{
					g_pStateMgr->PrepareGameSync( in_Item.gameSync.eGroupType, in_Item.gameSync.uGroupID, in_Item.gameSync.pGameSyncID[k], false );
				}
				break;
			}
			NotifyPrepareGameSync( in_Item.gameSync, eResult );

		}

		if( in_Item.gameSync.bSupported && eResult == AK_Success )
		{
			eResult = ProcessAccumulated();

			if( eResult != AK_Success )
			{
				// Handle Error, revert prepared nodes.
				for( AkUInt32 i = 0; i < in_Item.gameSync.uNumGameSync; ++i )
				{
					g_pStateMgr->PrepareGameSync( in_Item.gameSync.eGroupType, in_Item.gameSync.uGroupID, in_Item.gameSync.pGameSyncID[i], false );
				}
			}
		}

		// flush data if in_Item.gameSync.uNumGameSync greater than 1
		AkFree(AkMemID_Object, in_Item.gameSync.pGameSyncID );
	}

	if( in_Item.gameSync.bSupported )//do not accumulate when unloading game sync.
		DisableAccumulation();

	// Notify user.
	NotifyCompletion( in_Item, eResult );

	return eResult;
}

AKRESULT CAkBankMgr::PrepareBank( AkBankQueueItem in_Item )
{
	AkLoadBankDataMode loadMode = in_Item.bankPreparation.uFlags == AK::SoundEngine::AkBankContent_All ?
							AkLoadBankDataMode_MediaAndStructure : AkLoadBankDataMode_Structure;

	AKRESULT eResult = PrepareBankInternal( in_Item, loadMode, true, in_Item.bankPreparation.bDecode );

	NotifyCompletion( in_Item, eResult );

	return eResult;
}

AKRESULT CAkBankMgr::UnPrepareBank( AkBankQueueItem in_Item )
{
	AKRESULT eResult = AK_Success;

	AkUniqueID BankID	= in_Item.bankID;

	UnPrepareBankInternal( BankID, true );

	// Notify user.
	NotifyCompletion( in_Item, eResult );

	return eResult;
}

AKRESULT CAkBankMgr::LoadMediaFile( const char* in_pszFileName, AkAlignedPtrSet& out_AlignedPtrSet )
{
	AKRESULT eResult = AK_Success;

#ifndef PROXYCENTRAL_CONNECTED

	// Clean out params.
	out_AlignedPtrSet.pAllocated = NULL;
	out_AlignedPtrSet.uMediaSize = 0;

	// Create the stream.
    AkFileSystemFlags fsFlags;
    fsFlags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
    fsFlags.uCodecID = AKCODECID_BANK;
    fsFlags.pCustomParam = NULL;
    fsFlags.uCustomParamSize = 0;
	fsFlags.bIsLanguageSpecific = false;

	AK::IAkStdStream * pStream = NULL;

	AkOSChar * pszFileName;
	CONVERT_CHAR_TO_OSCHAR( in_pszFileName, pszFileName );
	eResult =  AK::IAkStreamMgr::Get()->CreateStd(
                                    pszFileName,
                                    &fsFlags,
                                    AK_OpenModeRead,
                                    pStream,
									true );	// Force synchronous open.

	if( eResult != AK_Success )
	{
		return eResult;
	}

	// Optional: Set the name of the Stream for the profiling.
#ifndef AK_OPTIMIZED
	pStream->SetStreamName( pszFileName );
#endif

	// Call GetInfo(...) on the stream to get its size and some more...
	AkStreamInfo info;
	pStream->GetInfo( info );

	// Allocate the buffer.

	// Check the size is not NULL.
	if( info.uSize != 0 )
	{
		out_AlignedPtrSet.uMediaSize = (AkUInt32)info.uSize;
		out_AlignedPtrSet.pAllocated = (AkUInt8*)AK::MemoryMgr::Malign(AkMemID_Media, (size_t)info.uSize, AK_BANK_PLATFORM_DATA_ALIGNMENT);
		if( out_AlignedPtrSet.pAllocated )
		{
			// Read the data, blocking:

			AkUInt32 ulActualSizeRead;
			eResult =  pStream->Read( out_AlignedPtrSet.pAllocated,
										(AkUInt32)info.uSize,
										true,
										AK_DEFAULT_BANK_IO_PRIORITY,
										( info.uSize / AK_DEFAULT_BANK_THROUGHPUT ), // deadline (s) = size (bytes) / throughput (bytes/s).
										ulActualSizeRead );

			if ( eResult == AK_Success && pStream->GetStatus() == AK_StmStatusCompleted )
			{
				out_AlignedPtrSet.uMediaSize = ulActualSizeRead;
				// Make sure we read the expected size.
				AKASSERT( ulActualSizeRead == info.uSize );
			}
			else
			{
				AK::MemoryMgr::Falign(AkMemID_Media, out_AlignedPtrSet.pAllocated);
				out_AlignedPtrSet.pAllocated = NULL;
			}
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}
	else
	{
		eResult = AK_InvalidFile;
	}

	if( pStream )
		pStream->Destroy();

#endif

	return eResult;
}

AKRESULT CAkBankMgr::LoadMediaFile( AkBankQueueItem in_Item )
{
	AKRESULT eResult = AK_Success;
#ifndef PROXYCENTRAL_CONNECTED
	{// Bracket to release lock before notification
		AkAutoLock<CAkLock> gate( m_MediaLock );

		AkUniqueID sourceID = in_Item.loadMediaFile.MediaID;

		AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( sourceID );
		if( pMediaEntry )
		{
			pMediaEntry->AddRef();
		}
		else
		{
			// Add the missing entry
			pMediaEntry = m_MediaHashTable.Set( sourceID );
			if( !pMediaEntry )
			{
				eResult = AK_InsufficientMemory;
			}
			else
			{
				pMediaEntry->SetSourceID( sourceID );

				AkAlignedPtrSet mediaPtrs;
				{
					m_MediaLock.Unlock(); // no need to keep the lock during the IO. pMediaEntry is safe as it is already addref'ed at this point.
					eResult = LoadMediaFile( in_Item.fileName.Get(), mediaPtrs );
					m_MediaLock.Lock();
				}

				if( eResult == AK_Success )
				{
					AKASSERT( mediaPtrs.pAllocated != NULL );
					pMediaEntry->SetPreparedData(mediaPtrs.pAllocated, mediaPtrs.uMediaSize, AkMemID_Media);
				}
				else
				{
					ReleaseMediaHashTableEntry(pMediaEntry);
				}
			}
		}
	}

	// Notify user.
	NotifyCompletion( in_Item, eResult );

#endif
	return eResult;
}

AKRESULT CAkBankMgr::UnloadMediaFile( AkBankQueueItem in_Item )
{
	AKRESULT eResult = AK_Success;
	AkAutoLock<CAkLock> gate( m_MediaLock );

	AkUniqueID sourceID = in_Item.loadMediaFile.MediaID;

	AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( sourceID );
	if( pMediaEntry )
	{
		if( ReleaseMediaHashTableEntry(pMediaEntry) != 0 ) // if the memory was released
		{
			// Our target was to unload the media, we failed as it was most likely in use.
			// returning an error code will allow Wwise to re-try later
			eResult = AK_Fail;
		}
	}

	// Notify user.
	NotifyCompletion( in_Item, eResult );

	return eResult;
}

AKRESULT CAkBankMgr::MediaSwap( AkBankQueueItem in_Item )
{
	// Here is the short story on how the powerswap should work.
	// Pseudo code:
	//
	// result = UnloadMedia()
	// if( result == success )
	// {
	//   LoadMedia
	// }
	// else
	// {
	//     This is the tricky part, here we want to perform the Hot Swap.
	//		Re-addref the media
	//     if the addref fail, that mean the data was free, now load it again.
	//     if the addref succeed, Perform the HotSwap

	// }

	AKRESULT eResult = AK_Success;
#ifndef PROXYCENTRAL_CONNECTED
	{// Bracket to release lock before notification
		AkAutoLock<CAkLock> gate( m_MediaLock );

		AkUniqueID sourceID = in_Item.loadMediaFile.MediaID;

		AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( sourceID );
		if( pMediaEntry )
		{
			if( ReleaseMediaHashTableEntry(pMediaEntry) != 0 ) // if the memory was released
			{
				// Our target was to unload the media, we failed as it was most likely in use.
				// returning an error code will allow Wwise to re-try later
				eResult = AK_Fail;
			}
		}

		if( eResult == AK_Success )
		{
			return LoadMediaFile( in_Item );
		}

		// If we end up here, it is because we really need to perform the HotSwap of the Data.

		//Reset the Error flag to success, We are going to fix the problem with HotSwapping of the data.
		eResult = AK_Success;

		// try to re-acquire the media another time and Addref it to keep it alive during the swap.
		pMediaEntry = m_MediaHashTable.Exists( sourceID );
		if( pMediaEntry )
		{
			pMediaEntry->AddRef();

			// Load the media First.
			AkAlignedPtrSet mediaPtrs;
			{
				m_MediaLock.Unlock(); // no need to keep the lock during the IO. pMediaEntry is safe as it is already addref'ed at this point.
				eResult = LoadMediaFile( in_Item.fileName.Get(), mediaPtrs );
				m_MediaLock.Lock();
			}

			if( eResult == AK_Success )
			{
				AKASSERT( mediaPtrs.pAllocated != NULL );

				// Performing the Hot Swap

				// We are going to play out of our playground; acquire main lock.
				CAkFunctionCritical SpaceSetAsCritical;

				//Backup old memory pointer.
				const AkUInt8* pOldPointer = pMediaEntry->GetPreparedMemoryPointer();

				//Free the Old Outdated Media:
				pMediaEntry->FreeMedia();
				//Set the new Media
				pMediaEntry->SetPreparedData(mediaPtrs.pAllocated, mediaPtrs.uMediaSize, AkMemID_Media);

				// Do some magic to Reset effects without stopping them if possible.
				CAkURenderer::ResetAllEffectsUsingThisMedia( pOldPointer );
			}
			else
			{
				// We unloaded but fail to load... so overall it was an unload: Release.
				ReleaseMediaHashTableEntry(pMediaEntry);
			}
		}
		else
		{
			// Some Black magic did fix the problem in the lasst few Microseconds, no hot swap needed, do normal swap.
			return LoadMediaFile( in_Item );
		}
	}
	// Notify user.
	NotifyCompletion( in_Item, eResult );

#endif
	return eResult;
}

void CAkBankMgr::UnPrepareAllBank()
{
	int i = 0;
	AkBankKey* l_paBankIDs = NULL;
	{
		AkAutoLock<CAkBankList> BankListGate( m_BankList );

		CAkBankList::AkListLoadedBanks& rBankList = m_BankList.GetUNSAFEBankListRef();

		AkUInt32 l_ulNumBanks = rBankList.Length();
		if( l_ulNumBanks )
		{
			l_paBankIDs = (AkBankKey*)AkAlloca( l_ulNumBanks * sizeof( AkBankKey ) );
			{
				for( CAkBankList::AkListLoadedBanks::Iterator iter = rBankList.Begin(); iter != rBankList.End(); ++iter )
				{
					if( (*iter)->WasPreparedAsABank() )
					{
						l_paBankIDs[i] = (*iter)->key;
						++i;
					}
				}
			}
		}
	}
	// Unloading in reverse order
	while ( i > 0 )
	{
		AkBankKey bankKey = l_paBankIDs[ --i ];

		CAkBankList::Lock();
		CAkUsageSlot * pUsageSlot = m_BankList.Get( bankKey );
		if( pUsageSlot && pUsageSlot->WasPreparedAsABank())
		{
			CAkBankList::Unlock();
			UnPrepareBankInternal( bankKey.bankID, true, true );
		}
		else
		{
			CAkBankList::Unlock();
		}
	}
}

AKRESULT CAkBankMgr::PrepareBankInternal( AkBankQueueItem in_Item, AkLoadBankDataMode in_LoadBankMode, bool in_bIsFromPrepareBank /*= false*/, bool in_bDecode )
{
	AkBankKey bankKey(in_Item.bankID, NULL);
	{ // Lock brackets
		AkAutoLock<CAkBankList> BankListGate( m_BankList );

		CAkUsageSlot* pUsageSlot = m_BankList.Get( bankKey );
		if( pUsageSlot )
		{
			pUsageSlot->AddRefPrepare();
			pUsageSlot->WasPreparedAsABank( true );
			if( in_LoadBankMode == AkLoadBankDataMode_Structure )
			{
				return AK_Success;
			}

			AKASSERT( in_LoadBankMode == AkLoadBankDataMode_MediaAndStructure );

			if( pUsageSlot->IsMediaPrepared() )
			{
				return AK_Success;
			}

			// Only media must be loaded, correct the target and continue.
			in_LoadBankMode = AkLoadBankDataMode_Media;
		}
	}

	CAkUsageSlot * pUsageSlot = NULL;

	AKRESULT eResult = LoadBank( in_Item, pUsageSlot, in_LoadBankMode, in_bIsFromPrepareBank, in_bDecode );

	if ( eResult == AK_Success )
	{
		m_BankList.Set( bankKey, pUsageSlot );
	}
	else if( eResult != AK_BankAlreadyLoaded )
	{
		// Here, should not pass the notification flag.
		if ( pUsageSlot )
		{
			m_BankList.Remove( bankKey );
			pUsageSlot->ReleasePrepare();
		}
	}

	MONITOR_LOADEDBANK( pUsageSlot, false );

	return eResult;
}

void CAkBankMgr::UnPrepareBankInternal( AkUniqueID in_BankID, bool in_bIsFromPrepareBank, bool in_bIsFinal /*= false*/ )
{
	AkBankKey bankKey( in_BankID, NULL );
	CAkUsageSlot* pSlot = m_BankList.Get( bankKey );

	if( pSlot )
	{
		if( in_bIsFromPrepareBank && pSlot->WasPreparedAsABank() )
		{
			pSlot->WasPreparedAsABank( false );
		}
		pSlot->ReleasePrepare( in_bIsFinal );
	}
}

// Not to be called directly, the thread must be stopped for it to be possible
void  CAkBankMgr::UnloadAll()
{
	ClearPreparedEvents();
	UnPrepareAllBank();

	CAkBankList::AkListLoadedBanks& rBankList = m_BankList.GetUNSAFEBankListRef();
	CAkBankList::AkListLoadedBanks::IteratorEx it = rBankList.BeginEx();
	while ( it != rBankList.End() )
	{
		CAkUsageSlot* pSlot = *it;
		it = rBankList.Erase(it);
		pSlot->RemoveContent();
		pSlot->Release( true );
	}
}

void CAkBankMgr::AddLoadedItem(CAkUsageSlot* in_pUsageSlot, CAkIndexable* in_pIndexable)
{
	// Guaranteed to succeed because the necessary space has already been reserved
	AKVERIFY( in_pUsageSlot->m_listLoadedItem.AddLast(in_pIndexable) );
}

namespace AK
{
	AkUInt32 g_uAltValues[4] = { 0, 0, 0, 0 };
}

static void ApplyHeaderValues(AkBankHeader & io_rBankHeader)
{
	if (g_uAltValues[0] != 0)
	{
		io_rBankHeader.dwBankGeneratorVersion ^= AK::g_uAltValues[0];
		io_rBankHeader.dwSoundBankID ^= AK::g_uAltValues[1];
		io_rBankHeader.dwLanguageID ^= AK::g_uAltValues[2];
		io_rBankHeader.uAlignment ^= (AK::g_uAltValues[3] & 0xFFFF);
		io_rBankHeader.bDeviceAllocated ^= (AK::g_uAltValues[3] >> 16);
	}
}

AKRESULT CAkBankMgr::GetBankInfoFromPtr( const void* in_pData, AkUInt32 in_uSize, bool in_bCheckAlignment, AkBankID & out_bankID)
{
	// The bank must at least be the size of the header so we can read the bank ID.
	if (in_uSize >= AK_MINIMUM_BANK_SIZE)
	{
		AkUInt8* pData = (AkUInt8*)in_pData;
		pData += sizeof(AkSubchunkHeader);

		AkBankHeader hdr = *((AkBankHeader*)pData);
		ApplyHeaderValues(hdr);

		if (in_bCheckAlignment)
		{
			AkUInt32 uAlignmentRestriction = GetBankAlignmentFromHeader(hdr);
			if (((uintptr_t)in_pData) % uAlignmentRestriction != 0)
			{
				return AK_DataAlignmentError;
			}
		}

		out_bankID = hdr.dwSoundBankID;
		return AK_Success;
	}
	else
	{
		return AK_InvalidParameter;
	}
}

AKRESULT CAkBankMgr::ProcessBankHeader(AkBankHeader& in_rBankHeader, bool& out_bBackwardDataCompatibilityMode)
{
	AKRESULT eResult = AK_Success;
	out_bBackwardDataCompatibilityMode = false;
	AkSubchunkHeader SubChunkHeader;

	eResult = m_BankReader.FillDataEx(&SubChunkHeader, sizeof(SubChunkHeader));
	if( eResult == AK_Success && SubChunkHeader.dwTag == BankHeaderChunkID )
	{
		eResult = m_BankReader.FillDataEx(&in_rBankHeader, sizeof(in_rBankHeader) );
		if ( eResult == AK_Success && g_uAltValues[ 0 ] != 0 )
		{
			in_rBankHeader.dwBankGeneratorVersion ^= AK::g_uAltValues[ 0 ];
			in_rBankHeader.dwSoundBankID ^= AK::g_uAltValues[ 1 ];
			in_rBankHeader.dwLanguageID ^= AK::g_uAltValues[ 2 ];
			in_rBankHeader.uAlignment ^= (AK::g_uAltValues[3] & 0xFFFF);
			in_rBankHeader.bDeviceAllocated ^= (AK::g_uAltValues[ 3 ]>>16);
		}

		AkUInt32 uSizeToSkip = SubChunkHeader.dwChunkSize - sizeof(in_rBankHeader);
		if(eResult == AK_Success && uSizeToSkip )
		{
			AkUInt32 ulSizeSkipped = 0;
			eResult = m_BankReader.Skip( uSizeToSkip, ulSizeSkipped);
			if(eResult == AK_Success && ulSizeSkipped != uSizeToSkip)
			{
				eResult = AK_BankReadError;
			}
		}
		if( eResult == AK_Success)
		{
			if (in_rBankHeader.dwBankGeneratorVersion < AK_BANK_READER_VERSION)
			{
				// In Backward Compatibility mode, we accept only banks containing DATA information.
				// All other type of content is not supported.
				// Note that Disabling older bank compatibility check brings some risk as we will load DATA without knowing what it is.
				// The user must guarantee that the content that will be attempted to be loaded will still be compatible, because we will load it anyway.
				// Failure to do so could lead to decoders and/or plug-ins to try to work with a data format that is unexpected.
				if ( in_rBankHeader.dwBankGeneratorVersion >= AK_BANK_READER_VERSION_LAST_MEDIA_ONLY_COMPATIBLE )
				{
					// This Bank was built with a previous version of Wwise.
					// We can load it ONLY if it contains only WEM data content.
					out_bBackwardDataCompatibilityMode = true;
				}
				else
				{
					LogWrongBankVersion(in_rBankHeader.dwBankGeneratorVersion);
					eResult = AK_WrongBankVersion;
				}


			}
			else if (in_rBankHeader.dwBankGeneratorVersion > AK_BANK_READER_VERSION)
			{
				LogWrongBankVersion(in_rBankHeader.dwBankGeneratorVersion);
				eResult = AK_WrongBankVersion;
			}
		}
	}
	else
	{
		eResult = AK_InvalidFile;
	}

	return eResult;
}

AKRESULT CAkBankMgr::ProcessDataChunk(AkUInt32 in_dwDataChunkSize, CAkUsageSlot* in_pUsageSlot)
{
	AKASSERT(in_pUsageSlot->m_pData == NULL);

	AKRESULT eResult = AK_Success;

	if (in_dwDataChunkSize != 0)
	{
		// Allocate data in the pool.
		AkMemPoolId poolId = AkMemID_Media | (in_pUsageSlot->UseDeviceMemory() ? AkMemType_Device : 0);
		in_pUsageSlot->m_pData = (AkUInt8*)AkMalign(poolId, in_dwDataChunkSize, in_pUsageSlot->Alignment());
		if (in_pUsageSlot->m_pData == NULL)
		{
			// Cannot allocate bank memory.
			MONITOR_ERROR(AK::Monitor::ErrorCode_InsufficientSpaceToLoadBank);
			eResult = AK_InsufficientMemory;
		}
		else
		{
			in_pUsageSlot->m_uLoadedDataSize = in_dwDataChunkSize;
			AK_PERF_INCREMENT_BANK_MEMORY(in_pUsageSlot->m_uLoadedDataSize);

			AkUInt32 ulReadBytes = 0;
			eResult = m_BankReader.FillData(in_pUsageSlot->m_pData, in_dwDataChunkSize, ulReadBytes);
			if (eResult == AK_Success && ulReadBytes != in_dwDataChunkSize)
			{
				MONITOR_ERROR(AK::Monitor::ErrorCode_ErrorWhileLoadingBank);
				eResult = AK_InvalidFile;
			}
		}
	}


	return eResult;
}

AKRESULT CAkBankMgr::ProcessHircChunk(CAkUsageSlot* in_pUsageSlot, AkUInt32 in_dwBankID)
{
	AkUInt32 l_NumReleasableHircItem = 0;

	AKRESULT eResult = m_BankReader.FillDataEx(&l_NumReleasableHircItem, sizeof(l_NumReleasableHircItem));
	if( eResult == AK_Success && l_NumReleasableHircItem )
	{
		eResult = in_pUsageSlot->m_listLoadedItem.GrowArray( l_NumReleasableHircItem ) ? AK_Success : AK_Fail;
	}

	bool bWarnedUnknownContent = false;

	for( AkUInt32 i = 0; i < l_NumReleasableHircItem && eResult == AK_Success; ++i )
	{
		AKBKSubHircSection Section;

		eResult = m_BankReader.FillDataEx(&Section, sizeof(Section));
		if(eResult == AK_Success)
		{
			switch( Section.eHircType )
			{
			case HIRCType_Action:
				eResult = ReadAction(Section, in_pUsageSlot);
				break;
			case HIRCType_Event:
				eResult = ReadEvent(Section, in_pUsageSlot);
				break;

			case HIRCType_Sound:
				eResult = ReadSourceParent<CAkSound>(Section, in_pUsageSlot, in_dwBankID);
				break;

			case HIRCType_RanSeqCntr:
				eResult = StdBankRead<CAkRanSeqCntr, CAkParameterNodeBase>( Section, in_pUsageSlot, g_pIndex->GetNodeIndex( AkNodeType_Default ) );
				break;

			case HIRCType_SwitchCntr:
				eResult = StdBankRead<CAkSwitchCntr, CAkParameterNodeBase>( Section, in_pUsageSlot, g_pIndex->GetNodeIndex( AkNodeType_Default ) );
				break;

			case HIRCType_LayerCntr:
				eResult = StdBankRead<CAkLayerCntr, CAkParameterNodeBase>( Section, in_pUsageSlot, g_pIndex->GetNodeIndex( AkNodeType_Default ) );
				break;

			case HIRCType_ActorMixer:
				eResult = StdBankRead<CAkActorMixer, CAkParameterNodeBase>( Section, in_pUsageSlot, g_pIndex->GetNodeIndex( AkNodeType_Default ) );
				break;

			case HIRCType_State:
				eResult = ReadState( Section, in_pUsageSlot );
				break;

			case HIRCType_Bus:
				eResult = ReadBus(Section, in_pUsageSlot);				
				break;

			case HIRCType_AuxBus:
				eResult = StdBankRead<CAkAuxBus, CAkParameterNodeBase>(Section, in_pUsageSlot, g_pIndex->GetNodeIndex(AkNodeType_Bus));
				break;

			case HIRCType_Attenuation:
				eResult = StdBankRead<CAkAttenuation, CAkAttenuation>( Section, in_pUsageSlot, g_pIndex->m_idxAttenuations );
				break;

			case HIRCType_Lfo:
				eResult = StdBankRead<CAkLFOModulator, CAkModulator>( Section, in_pUsageSlot, g_pIndex->m_idxModulators );
				break;

			case HIRCType_Envelope:
				eResult = StdBankRead<CAkEnvelopeModulator, CAkModulator>( Section, in_pUsageSlot, g_pIndex->m_idxModulators );
				break;

			case HIRCType_TimeMod:
				eResult = StdBankRead<CAkTimeModulator, CAkModulator>( Section, in_pUsageSlot, g_pIndex->m_idxModulators );
				break;

			case HIRCType_DialogueEvent:
				eResult = StdBankRead<CAkDialogueEvent, CAkDialogueEvent>( Section, in_pUsageSlot, g_pIndex->m_idxDialogueEvents );
				break;

			case HIRCType_FxShareSet:
				eResult = StdBankRead<CAkFxShareSet, CAkFxShareSet>( Section, in_pUsageSlot, g_pIndex->m_idxFxShareSets );
				break;

			case HIRCType_AudioDevice:
				eResult = StdBankRead<CAkAudioDevice, CAkAudioDevice>(Section, in_pUsageSlot, g_pIndex->m_idxAudioDevices);
				break;

			case HIRCType_FxCustom:
				eResult = StdBankRead<CAkFxCustom, CAkFxCustom>( Section, in_pUsageSlot, g_pIndex->m_idxFxCustom );
				break;

			default:
				{
					if( g_pExternalBankHandlerCallback )
					{
						eResult = g_pExternalBankHandlerCallback( Section, in_pUsageSlot, in_dwBankID );
						if( eResult != AK_PartialSuccess )
						{
							break;
						}
					}
					else if( !bWarnedUnknownContent )
					{
						if(	   Section.eHircType == HIRCType_Segment
							|| Section.eHircType == HIRCType_Track
							|| Section.eHircType == HIRCType_MusicSwitch
							|| Section.eHircType == HIRCType_MusicRanSeq )
						{
							bWarnedUnknownContent = true;
							MONITOR_ERRORMSG( AKTEXT("Music engine not initialized : Content can not be loaded from bank") );
						}
					}
					AkUInt32 ulReadBytes = 0;
					m_BankReader.Skip(Section.dwSectionSize, ulReadBytes );
					if(ulReadBytes != Section.dwSectionSize)
					{
						eResult = AK_InvalidFile;
					}
				}
				break;
			}
		}
	}

	return eResult;
}

AKRESULT CAkBankMgr::ProcessStringMappingChunk( AkUInt32 in_dwDataChunkSize, CAkUsageSlot* /*in_pUsageSlot*/ )
{
	AKRESULT eResult = AK_Success;

	while( in_dwDataChunkSize && eResult == AK_Success )
	{
		// Read header

		AKBKHashHeader hdr;

		eResult = m_BankReader.FillDataEx( &hdr, sizeof( hdr ) );
		if ( eResult != AK_Success ) break;

		in_dwDataChunkSize -= sizeof( hdr );

		// StringType_Bank is a special case

		AKASSERT( hdr.uiType == StringType_Bank );

		////////////////////////////////////////////////////
		//    StringType_Bank
		////////////////////////////////////////////////////
		// hdr.uiSize is the number of entries following
		// each entry is
		// UINT32 bankID
		// UINT8 stringSize
		// char[] string (NOT NULL TERMINATED)
		////////////////////////////////////////////////////

		for( AkUInt32 stringNumber = 0; stringNumber < hdr.uiSize; ++stringNumber )
		{
			// read bank ID
			AkBankID bankID;
			eResult = m_BankReader.FillDataEx( &bankID, sizeof( bankID ) );
			if( eResult != AK_Success )break;
			in_dwDataChunkSize -= sizeof( bankID );

			// read string size (NOT NULL TERMINATED)
			AkUInt8 stringsize;
			eResult = m_BankReader.FillDataEx( &stringsize, sizeof( stringsize ) );
			if( eResult != AK_Success )break;
			in_dwDataChunkSize -= sizeof( stringsize );

			// Check the list.
			if( m_BankIDToFileName.Exists( bankID ) )
			{
				AkUInt32 uIgnoredSkippedSize;
				m_BankReader.Skip( stringsize, uIgnoredSkippedSize );
				AKASSERT( uIgnoredSkippedSize == stringsize );
				in_dwDataChunkSize -= stringsize;
			}
			else
			{
				// Alloc string of size stringsize + 1 ( for missing NULL character )
				// Alloc string of size stringsize + 4 ( for missing .bnk extension )
				char* pString = (char*)AkAlloc(AkMemID_Object, stringsize + 1 + 4 );
				if( pString )
				{
					// Read/Copy the string
					pString[ stringsize ] = '.';
					pString[ stringsize + 1] = 'b';
					pString[ stringsize + 2] = 'n';
					pString[ stringsize + 3] = 'k';
					pString[ stringsize + 4] = 0;
					eResult = m_BankReader.FillDataEx( pString, stringsize );

					if( eResult == AK_Success )
					{
						in_dwDataChunkSize -= stringsize;
						// Add the entry in the table
						char** ppString = m_BankIDToFileName.Set( bankID );
						if( ppString )
						{
							*ppString = pString;
						}
						else
						{
							eResult = AK_InsufficientMemory;
						}
					}
					if( eResult != AK_Success )
					{
						AkFree(AkMemID_Object, pString );
						break;
					}
				}
				else
				{
					// Fill eResult and break.
					eResult = AK_InsufficientMemory;
					break;
				}
			}
		} // for( AkUInt32 stringNumber = 0; stringNumber < hdr.uiSize; ++stringNumber )
	}

	return eResult;
}

AKRESULT CAkBankMgr::ProcessGlobalSettingsChunk( AkUInt32 in_dwDataChunkSize )
{
	AKRESULT eResult = AK_Success;

	AKASSERT(g_pStateMgr);

	if( in_dwDataChunkSize )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		//Here, read the first item, which is the volume threshold
		AkReal32 fVolumeThreshold;
		eResult = m_BankReader.FillDataEx( &fVolumeThreshold, sizeof( fVolumeThreshold ) );
		AK::SoundEngine::SetVolumeThresholdInternal( fVolumeThreshold, AK::SoundEngine::AkCommandPriority_InitDefault );
		if( eResult == AK_Success )
		{
			AkUInt16 u16MaxVoices;
			eResult = m_BankReader.FillDataEx( &u16MaxVoices, sizeof( u16MaxVoices ) );
			AK::SoundEngine::SetMaxNumVoicesLimitInternal( u16MaxVoices, AK::SoundEngine::AkCommandPriority_InitDefault );
		}
		if ( eResult == AK_Success )
		{
			AkUInt16 u16MaxVirtVoices;
			eResult = m_BankReader.FillDataEx( &u16MaxVirtVoices, sizeof( u16MaxVirtVoices ) );
			AK::SoundEngine::SetMaxNumDangerousVirtVoicesLimitInternal( u16MaxVirtVoices, AK::SoundEngine::AkCommandPriority_InitDefault );
		}

		AkUInt32 ulNumStateGroups = 0;
		if( eResult == AK_Success )
		{
			eResult = m_BankReader.FillDataEx( &ulNumStateGroups, sizeof( ulNumStateGroups ) );
		}
		if (eResult == AK_Success)
		{
			for (AkUInt32 i = 0; i < ulNumStateGroups; ++i) //iterating trough state groups
			{
				AkUInt32 ulStateGroupID = 0;
				AkTimeMs DefaultTransitionTime = 0;
				AkUInt32 ulNumTransitions = 0;

				eResult = m_BankReader.FillDataEx(&ulStateGroupID, sizeof(ulStateGroupID));
				if (eResult == AK_Success)
				{
					eResult = m_BankReader.FillDataEx(&DefaultTransitionTime, sizeof(DefaultTransitionTime));
				}
				if (eResult == AK_Success)
				{
					eResult = g_pStateMgr->AddStateGroup(ulStateGroupID) ? AK_Success : AK_Fail;
				}
				if (eResult == AK_Success)
				{
					eResult = g_pStateMgr->SetdefaultTransitionTime(ulStateGroupID, DefaultTransitionTime);
				}
				if (eResult == AK_Success)
				{
					eResult = m_BankReader.FillDataEx(&ulNumTransitions, sizeof(ulNumTransitions));
				}
				if (eResult == AK_Success)
				{
					for (AkUInt32 j = 0; j < ulNumTransitions; ++j)//iterating trough Transition time
					{
						AkUInt32		StateFrom;
						AkUInt32		StateTo;
						AkTimeMs	TransitionTime;

						eResult = m_BankReader.FillDataEx(&StateFrom, sizeof(StateFrom));
						if (eResult == AK_Success)
						{
							eResult = m_BankReader.FillDataEx(&StateTo, sizeof(StateTo));
						}
						if (eResult == AK_Success)
						{
							eResult = m_BankReader.FillDataEx(&TransitionTime, sizeof(TransitionTime));
						}
						if (eResult == AK_Success)
						{
							eResult = g_pStateMgr->AddStateTransition(ulStateGroupID, StateFrom, StateTo, TransitionTime);
						}
						// If failed, quit!
						if (eResult != AK_Success)
						{
							break;
						}
					}
				}
				// If failed, quit!
				if (eResult != AK_Success)
				{
					break;
				}
			}

			AkUInt32 ulNumSwitchGroups = 0;
			if (eResult == AK_Success)
			{
				eResult = m_BankReader.FillDataEx(&ulNumSwitchGroups, sizeof(ulNumSwitchGroups));
			}
			if (eResult == AK_Success)
			{
				for (AkUInt32 i = 0; i < ulNumSwitchGroups; ++i) //iterating trough switch groups
				{
					AkUInt32	SwitchGroupID;
					AkUInt32	rtpcID;
					AkUInt8		rtpcType;
					AkUInt32	ulSize;

					eResult = m_BankReader.FillDataEx(&SwitchGroupID, sizeof(SwitchGroupID));
					if (eResult == AK_Success)
					{
						eResult = m_BankReader.FillDataEx(&rtpcID, sizeof(rtpcID));
					}
					if (eResult == AK_Success)
					{
						eResult = m_BankReader.FillDataEx(&rtpcType, sizeof(rtpcType));
					}
					if (eResult == AK_Success)
					{
						eResult = m_BankReader.FillDataEx(&ulSize, sizeof(ulSize));
					}
					if (eResult == AK_Success)
					{
						// It is possible that the size be 0 in the following situation:
						// Wwise user created a switch to RTPC graph, and disabled it.
						if (ulSize)
						{
							AkUInt32 l_blockSize = ulSize*sizeof(AkSwitchGraphPoint);
							AkSwitchGraphPoint* pGraphPoints = (AkSwitchGraphPoint*)AkAlloc(AkMemID_Object, l_blockSize);
							if (pGraphPoints)
							{
								eResult = m_BankReader.FillDataEx(pGraphPoints, l_blockSize);
								if (eResult == AK_Success)
								{
									eResult = g_pSwitchMgr->AddSwitchRTPC(SwitchGroupID, rtpcID, (AkRtpcType)rtpcType, pGraphPoints, ulSize);
								}
								AkFree(AkMemID_Object, pGraphPoints);
							}
							else
							{
								eResult = AK_InsufficientMemory;
							}
						}
					}
					// If failed, quit!
					if (eResult != AK_Success)
					{
						break;
					}
				}
			}

			//Load default values for game parameters
			AkUInt32 ulNumParams = 0;
			if (eResult == AK_Success)
				eResult = m_BankReader.FillDataEx(&ulNumParams, sizeof(ulNumParams));

			if (eResult == AK_Success)
			{
				for (; ulNumParams > 0; ulNumParams--)
				{
					AkReal32	fValue;
					AkUInt32	RTPC_ID;

					AkUInt32    rampType;
					AkReal32    fRampUp;
					AkReal32    fRampDown;

					eResult = m_BankReader.FillDataEx(&RTPC_ID, sizeof(RTPC_ID));
					if (eResult == AK_Success)
						eResult = m_BankReader.FillDataEx(&fValue, sizeof(fValue));

					if (eResult == AK_Success)
						g_pRTPCMgr->SetDefaultParamValue(RTPC_ID, fValue);

					if (eResult == AK_Success)
						eResult = m_BankReader.FillDataEx(&rampType, sizeof(rampType));
					if (eResult == AK_Success)
						eResult = m_BankReader.FillDataEx(&fRampUp, sizeof(fRampUp));
					if (eResult == AK_Success)
						eResult = m_BankReader.FillDataEx(&fRampDown, sizeof(fRampDown));

					if (eResult == AK_Success)
						g_pRTPCMgr->SetRTPCRamping(RTPC_ID, (AkTransitionRampingType)rampType, fRampUp, fRampDown);

					if (eResult == AK_Success)
					{
						AkUInt8 eBindToBuiltInParam;
						eResult = m_BankReader.FillDataEx(&eBindToBuiltInParam, sizeof(AkUInt8));
						if (eResult && ((AkBuiltInParam)eBindToBuiltInParam) != BuiltInParam_None)
						{
							g_pRTPCMgr->AddBuiltInParamBinding((AkBuiltInParam)eBindToBuiltInParam, RTPC_ID);
						}
					}

					if (eResult != AK_Success)
						break;
				}
			}

			// Load Acoustic Textures
			AkUInt32 ulNumTextures = 0;
			if (eResult == AK_Success)
				eResult = m_BankReader.FillDataEx(&ulNumTextures, sizeof(ulNumTextures));

			if (eResult == AK_Success)
			{
				for (; ulNumTextures > 0; ulNumTextures--)
				{
#define FILL_DATA_EX(PARAM) if (eResult == AK_Success) eResult = m_BankReader.FillDataEx(&PARAM, sizeof(PARAM))
					AkUniqueID	ID;

					AkReal32 fAbsorptionOffset, fAbsorptionLow, fAbsorptionMidLow, fAbsorptionMidHigh, fAbsorptionHigh, fScattering;

					FILL_DATA_EX(ID);

					FILL_DATA_EX(fAbsorptionOffset);
					FILL_DATA_EX(fAbsorptionLow);
					FILL_DATA_EX(fAbsorptionMidLow);
					FILL_DATA_EX(fAbsorptionMidHigh);
					FILL_DATA_EX(fAbsorptionHigh);
					FILL_DATA_EX(fScattering);

					if (eResult == AK_Success)
					{
						AkAcousticTexture acousticTexture = AkAcousticTexture(ID, fAbsorptionOffset, fAbsorptionLow, fAbsorptionMidLow, fAbsorptionMidHigh, fAbsorptionHigh, fScattering);
						eResult = CAkVirtualAcousticsMgr::AddAcousticTexture(ID, acousticTexture);
					}

					if (eResult != AK_Success)
						break;
				}
			}
		}
	}
	else
	{
		AKASSERT(!"Invalid STMG chunk found in the Bank");
	}

	return eResult;
}

AKRESULT CAkBankMgr::ProcessEnvSettingsChunk( AkUInt32 in_dwDataChunkSize )
{
	AKRESULT eResult = AK_Success;

	AKASSERT(g_pStateMgr);
	if(!g_pStateMgr)
	{
		return AK_Fail;
	}

	if( in_dwDataChunkSize )
	{
		for ( int i=0; i<CAkEnvironmentsMgr::MAX_CURVE_X_TYPES; ++i )
		{
			for( int j=0; j<CAkEnvironmentsMgr::MAX_CURVE_Y_TYPES; ++j )
			{
				AkUInt8 bCurveEnabled;
				eResult = m_BankReader.FillDataEx( &bCurveEnabled, sizeof( bCurveEnabled ) );
				if( eResult == AK_Success )
				{
					g_pEnvironmentMgr->SetCurveEnabled( (CAkEnvironmentsMgr::eCurveXType)i, (CAkEnvironmentsMgr::eCurveYType)j, bCurveEnabled ? true : false );
				}

				if( eResult == AK_Success )
				{
					AkUInt8 l_eCurveScaling;
					eResult = m_BankReader.FillDataEx( &l_eCurveScaling, sizeof( AkUInt8 ) );

					AkUInt16 l_ulCurveSize;
					if( eResult == AK_Success )
						eResult = m_BankReader.FillDataEx( &l_ulCurveSize, sizeof( AkUInt16 ) );

					if( eResult == AK_Success )
					{
						AkRTPCGraphPoint* aPoints = (AkRTPCGraphPoint *) AkAlloc( AkMemID_Object, sizeof( AkRTPCGraphPoint ) * l_ulCurveSize );
						if ( aPoints )
                        {
                            eResult = m_BankReader.FillDataEx( &aPoints[0], sizeof( AkRTPCGraphPoint ) * l_ulCurveSize );
                            if( eResult == AK_Success )
						    {
							    g_pEnvironmentMgr->SetObsOccCurve( (CAkEnvironmentsMgr::eCurveXType)i, (CAkEnvironmentsMgr::eCurveYType)j, l_ulCurveSize, aPoints, (AkCurveScaling) l_eCurveScaling );
						    }
						    AkFree(AkMemID_Object, aPoints ); //env mgr will have copied the curve
                        }
                        else
                        	eResult = AK_InsufficientMemory;
					}
				}

				// If failed, quit!
				if( eResult != AK_Success)
				{
					break;
				}
			}

			// If failed, quit!
			if( eResult != AK_Success)
			{
				break;
			}
		}
	}
	else
	{
		AKASSERT(!"Invalid ENVS chunk found in the Bank");
		eResult = AK_Fail;
	}

	return eResult;
}

AKRESULT CAkBankMgr::ProcessCustomPlatformChunk(AkUInt32 in_dwDataChunkSize)
{
	AKRESULT eResult = AK_Success;

	if (in_dwDataChunkSize)
	{
		AkUInt32 uStringSize = 0;
		AkUInt8 * pData = (AkUInt8 *)m_BankReader.GetData(in_dwDataChunkSize);
		char * pCustomPlatformName = READBANKSTRING(pData, in_dwDataChunkSize, uStringSize);
		if (uStringSize > 0)
		{
			char * pName = (char *)AkAlloca(uStringSize + 1);
			strncpy(pName, pCustomPlatformName, uStringSize);
			pName[uStringSize] = 0;
			eResult = AK::SoundEngine::SetCustomPlatformName( pName );
		}

		m_BankReader.ReleaseData();
	}

	return eResult;
}

/////////////////////////////  HIRC PARSERS  ////////////////////////////////////
//
// STATE HIRC PARSER
AKRESULT CAkBankMgr::ReadState( const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot )
{
	AKRESULT eResult = AK_Success;

	const void* pData = m_BankReader.GetData( in_rSection.dwSectionSize );

	if(!pData)
	{
		return AK_Fail;
	}

	AkUniqueID ulStateID = ReadUnaligned<AkUInt32>((AkUInt8*) pData);

	CAkState* pState = g_pIndex->m_idxCustomStates.GetPtrAndAddRef( ulStateID );

	if( pState == NULL )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pState = CAkState::Create( ulStateID );
		if(!pState)
		{
			eResult = AK_Fail;
		}
		else
		{
			eResult = pState->SetInitialValues( (AkUInt8*)pData, in_rSection.dwSectionSize );
			if(eResult != AK_Success)
			{
				pState->Release();
			}
		}
	}

	if(eResult == AK_Success)
	{
		AddLoadedItem( in_pUsageSlot, pState ); //This will allow it to be removed on unload
	}

	m_BankReader.ReleaseData();

	return eResult;
}

AKRESULT CAkBankMgr::ReadBus(const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot)
{
	CAkBus *pMainBusBeforeLoad = CAkBus::GetMainBus();// Get the main bus BEFORE the load.

	CAkIndexItem<CAkParameterNodeBase*>& rIndex = g_pIndex->GetNodeIndex(AkNodeType_Bus);

	AKRESULT eResult = AK_Success;
	const void* pData = m_BankReader.GetData(in_rSection.dwSectionSize);
	if(!pData)
		return AK_Fail;

	AkUniqueID ulID = ReadUnaligned<AkUInt32>((AkUInt8*)pData);

	CAkBus* pObject = static_cast<CAkBus*>(rIndex.GetPtrAndAddRef(ulID));
	if(pObject == NULL)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pObject = CAkBus::Create(ulID);
		if(!pObject)
		{
			eResult = AK_Fail;
		}
		else
		{
			eResult = pObject->SetInitialValues((AkUInt8*)pData, in_rSection.dwSectionSize);
			if(eResult != AK_Success)
			{
				pObject->Release();
				pObject = NULL;
			}
		}
	}
	else
	{
		if (pObject->ParentBus() == NULL)
		{
			// Bus with no parent were master busses
			CAkFunctionCritical SpaceSetAsCritical;
			eResult = pObject->SetMasterBus();
			if (eResult != AK_Success)
			{
				pObject->Release();
				pObject = NULL;
			}
		}
	}

	if(eResult == AK_Success)
	{
		AKASSERT(pObject);
		//Check if we need to initialize the main device.
		//The main device's shareset is in the Init bank, with the Master Audio Bus.
		//We can only initialize it once both are loaded.
		AkDevice *pDevice = CAkOutputMgr::GetPrimaryDevice();
		if(pMainBusBeforeLoad == NULL //If it is the first bus loaded.
			|| (pDevice != NULL && pDevice->userSettings.audioDeviceShareset == 0 && pObject == pMainBusBeforeLoad) )	//Or re-loaded, with a default shareset.
		{
			if (g_settings.settingsMainOutput.audioDeviceShareset == 0 && pObject->GetDeviceShareset() != 0)
				g_settings.settingsMainOutput.audioDeviceShareset = pObject->GetDeviceShareset();	//No shareset specified by the code.  Use the one in banks.
			else
				pObject->SetBusDevice(g_settings.settingsMainOutput.audioDeviceShareset);	//Replace the id just loaded with the one specified manually by the programmer.
		
			g_pAudioMgr->InitSinkPlugin(g_settings.settingsMainOutput);
		}

		AddLoadedItem(in_pUsageSlot, pObject); //This will allow it to be removed on unload
	}

	m_BankReader.ReleaseData();	
	return eResult;
}

// ACTION HIRC PARSER
AKRESULT CAkBankMgr::ReadAction(const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot)
{
	AKRESULT eResult = AK_Success;

	const void* pData = m_BankReader.GetData( in_rSection.dwSectionSize );

	if(!pData)
	{
		return AK_Fail;
	}

	AkUInt32 ulID = ReadUnaligned<AkUInt32>(((AkUInt8*)pData));
	AkActionType ulActionType = (AkActionType)ReadUnaligned<AkUInt16>((AkUInt8*)pData + sizeof(AkUInt32));

	CAkAction* pAction = g_pIndex->m_idxActions.GetPtrAndAddRef(ulID);
	if( pAction == NULL )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pAction = CAkAction::Create(ulActionType, ulID );
		if(!pAction)
		{
			eResult = AK_Fail;
		}
		else
		{
			eResult = pAction->SetInitialValues( (AkUInt8*)pData, in_rSection.dwSectionSize);
			if(eResult != AK_Success)
			{
				pAction->Release();
			}
		}
	}
	else
	{
		// When reading actions of type play, we must read them again it they were posted By Wwise.
		// This is because play actions can be prepared, and need to have the correct Bank ID to load structure from.
		if( ulActionType == AkActionType_Play )
		{
			if( !static_cast<CAkActionPlay*>(pAction)->WasLoadedFromBank() )
			{
				CAkFunctionCritical SpaceSetAsCritical;
				eResult = pAction->SetInitialValues( (AkUInt8*)pData, in_rSection.dwSectionSize);
				if(eResult != AK_Success)
				{
					pAction->Release();
				}
			}
		}
	}

	if(eResult == AK_Success)
	{
		AddLoadedItem(in_pUsageSlot,pAction); //This will allow it to be removed on unload
	}

	m_BankReader.ReleaseData();

	return eResult;
}

// EVENT HIRC PARSER
AKRESULT CAkBankMgr::ReadEvent(const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot)
{
	AKRESULT eResult = AK_Success;

	const void* pData = m_BankReader.GetData( in_rSection.dwSectionSize );

	if(!pData)
	{
		return AK_Fail;
	}

	AkUniqueID ulID = ReadUnaligned<AkUniqueID>((AkUInt8*)pData);

	//If the event already exists, simply addref it
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef(ulID);
	if( pEvent == NULL )
	{
		// Using CreateNoIndex() instead of Create(), we will do the init right after.
		// The goal is to avoid the event to be in the index prior it is completely loaded.
		pEvent = CAkEvent::CreateNoIndex(ulID);
		if(!pEvent)
		{
			eResult = AK_Fail;
		}
		else
		{
			CAkFunctionCritical SpaceSetAsCritical;
			eResult = pEvent->SetInitialValues( (AkUInt8*)pData, in_rSection.dwSectionSize);
			if(eResult != AK_Success)
			{
				pEvent->Release();
			}
			else
			{
				// And here we proceed with the AddToIndex() call, required since we previously used CreateNoIndex() to create the event.
				pEvent->AddToIndex();
			}
		}
	}

	if(eResult == AK_Success)
	{
		AddLoadedItem(in_pUsageSlot,pEvent); //This will allow it to be removed on unload
	}

	m_BankReader.ReleaseData();

	return eResult;
}

AKRESULT CAkBankMgr::ProcessPluginChunk(AkUInt32 in_dwDataChunkSize)
{
	//This chunk contains the list of plugins to register, if dynamic loading is used.
	AkUInt8 *pData = (AkUInt8 *) m_BankReader.GetData(in_dwDataChunkSize);
	if (!pData)
		return AK_Fail;

	AkUInt32 count = READBANKDATA(AkUInt32, pData, in_dwDataChunkSize);
	for (; count > 0; count--)
	{
		AkUInt32 ulPluginID = READBANKDATA(AkUInt32, pData, in_dwDataChunkSize);
		AkUInt32 uStringSize;
		char * pDLLName = READBANKSTRING_UTF8(pData, in_dwDataChunkSize, uStringSize);
		if (!CAkEffectsMgr::IsPluginRegistered(ulPluginID))
		{
			//Try to load it by DLL.
			AkOSChar *szDll;
			CONVERT_CHAR_TO_OSCHAR(pDLLName, szDll);

			AKRESULT res = AK::SoundEngine::RegisterPluginDLL(szDll);
			if(res == AK_FileNotFound)
			{
				MONITOR_ERRORMSG2("Could not find plugin dynamic library ", pDLLName);
			}
			else if(res != AK_Success)
			{
				MONITOR_ERRORMSG2("Could not register plugin ", pDLLName);
			}			
		}
	}			

	return AK_Success;
}
//
//
/////////////////////////////  END OF HIRC PARSERS  ////////////////////////////////////

void CAkBankMgr::ClearPreparedEvents()
{
	AKASSERT( g_pIndex );
	CAkIndexItem<CAkEvent*>& l_rIdx = g_pIndex->m_idxEvents;

	CAkFunctionCritical SpaceSetAsCritical;// To avoid possible deadlocks
	// This function could potentially slow down the audio thread, but this is required to avoid deadlocks.
	AkAutoLock<CAkLock> IndexLock( l_rIdx.m_IndexLock );

	CAkIndexItem<CAkEvent*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin();
	while( iter != l_rIdx.m_mapIDToPtr.End() )
	{
		CAkEvent* pEvent = static_cast<CAkEvent*>( *iter );

		if( pEvent->IsPrepared() )
		{
			pEvent->AddRef();
			UnprepareEvent( pEvent, true );
			++iter;	// Must be done before releasing the event if we unprepared it.
					// It must be done after calling UnprepareEvent as UnprepareEvent may destroy multiple Events and they can all de removed from the index.
			pEvent->Release();
		}
		else
		{
			++iter;
		}
	}
}

AKRESULT CAkBankMgr::LoadMediaIndex( CAkUsageSlot* in_pCurrentSlot, AkUInt32 in_uIndexChunkSize, bool in_bLoadInPlace )
{
	AKRESULT eResult = AK_Success;
	if( in_pCurrentSlot->m_uNumLoadedItems )// m_uNumLoadedItems != 0 means it has already been loaded.
	{
		// skip the chunk
		AkUInt32 ulSkippedBytes;
		m_BankReader.Skip( in_uIndexChunkSize, ulSkippedBytes );
		AKASSERT( ulSkippedBytes == in_uIndexChunkSize );
	}
	else
	{
		AkUInt32 uNumMedias = in_uIndexChunkSize / sizeof( AkBank::MediaHeader );

		AKASSERT( uNumMedias != 0 );

		AkUInt32 uArraySize = uNumMedias * sizeof( AkBank::MediaHeader );

		if( !in_bLoadInPlace )
		{
			in_pCurrentSlot->m_paLoadedMedia = (AkBank::MediaHeader*)AkAlloc(AkMemID_Object, uArraySize );
			if( in_pCurrentSlot->m_paLoadedMedia )
			{
				in_pCurrentSlot->WasIndexAllocated( true );
			}
			if( in_pCurrentSlot->m_paLoadedMedia )
			{
				AKVERIFY( m_BankReader.FillDataEx( in_pCurrentSlot->m_paLoadedMedia, uArraySize ) == AK_Success );
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}
		else
		{
			// We must get it that we use it or not.
			AkBank::MediaHeader* pLoadedMedia = (AkBank::MediaHeader*)m_BankReader.GetData( uArraySize );
			if( !in_pCurrentSlot->m_paLoadedMedia )// we can already have it if we used PrepareBank with media.
				in_pCurrentSlot->m_paLoadedMedia = pLoadedMedia;

			m_BankReader.ReleaseData();
			AKASSERT( in_pCurrentSlot->m_paLoadedMedia );
		}
		if( in_pCurrentSlot->m_paLoadedMedia )
		{
			in_pCurrentSlot->m_uIndexSize = uNumMedias;
		}
	}
	return eResult;
}

AKRESULT CAkBankMgr::LoadMedia( AkUInt8* in_pDataBank, CAkUsageSlot* in_pCurrentSlot )
{
	if( in_pCurrentSlot->m_uNumLoadedItems != 0 || in_pCurrentSlot->m_uIndexSize == 0 )
		return AK_Success;

	AKRESULT eResult = AK_InsufficientMemory;

	if( in_pCurrentSlot->m_paLoadedMedia )
	{
		AkUInt32 nNumLoadedMedias = 0;
		for( ; nNumLoadedMedias < in_pCurrentSlot->m_uIndexSize; ++nNumLoadedMedias )
		{
			AkBank::MediaHeader& rMediaHeader = in_pCurrentSlot->m_paLoadedMedia[ nNumLoadedMedias ];
			if ( rMediaHeader.id != AK_INVALID_UNIQUE_ID )
			{
				AkAutoLock<CAkLock> gate( m_MediaLock );// Will have to decide if performance would be bettre if acquiring the lock outside the for loop.

				bool b_WasAlreadyEnlisted = false;
				AkMediaEntry* pMediaEntry = m_MediaHashTable.Set( rMediaHeader.id, b_WasAlreadyEnlisted );
				if( b_WasAlreadyEnlisted && pMediaEntry->GetPreparedMemoryPointer() != NULL )
				{
					// TODO : this optimization may be source of some confusion.
					// Maybe we should call add alternate bank anyway...
					pMediaEntry->AddRef();
				}
				else if( pMediaEntry )
				{
					pMediaEntry->SetSourceID( rMediaHeader.id );
					eResult = pMediaEntry->AddAlternateSource(
											in_pDataBank + rMediaHeader.uOffset,// Data pointer
											rMediaHeader.uSize,					// Data size
											in_pCurrentSlot						// pUsageSlot
											);

					if( eResult != AK_Success )
					{
						m_MediaHashTable.Unset( rMediaHeader.id );
						break;
					}
					else if( b_WasAlreadyEnlisted )
					{
						pMediaEntry->AddRef();
					}
				}
				else
				{
					break;
				}

				if( pMediaEntry )
				{
					MONITOR_MEDIAPREPARED( *pMediaEntry );
				}
			}

			++( in_pCurrentSlot->m_uNumLoadedItems );
		}

		if( in_pCurrentSlot->m_uIndexSize == nNumLoadedMedias ) // if everything loaded successfully.
		{
			eResult = AK_Success;
		}
	}

	if( eResult != AK_Success )
	{
		UnloadMedia( in_pCurrentSlot );
	}

	return eResult;
}

AkUInt32 CAkBankMgr::GetBufferSizeForDecodedMedia(AkFileParser::FormatInfo& in_fmtInfo, AkUInt32 in_uEncodedData, AkUInt32 in_uDataOffset)
{
	//Adjust offset if necessary to satisfy 4 byte alignment.
	if ((in_uDataOffset & 0x3) != 0)
	{
		//Will have to add a "junk" chunk.
		in_uDataOffset += (sizeof(AkChunkHeader) + (4 - ((in_uDataOffset + sizeof(AkChunkHeader)) & 0x3)));
	}

	if (in_fmtInfo.pFormat->wFormatTag == AK_WAVE_FORMAT_VORBIS)
	{
		WaveFormatVorbis * pFmt = (WaveFormatVorbis *)in_fmtInfo.pFormat;
		AkUInt32 uNumFrames = pFmt->vorbisHeader.dwTotalPCMFrames;
		AkUInt32 uFrameSizePCM = sizeof(AkInt16) * in_fmtInfo.pFormat->nChannels;

		return (uNumFrames * uFrameSizePCM) + in_uDataOffset;
	}
	else if (in_fmtInfo.pFormat->wFormatTag == AK_WAVE_FORMAT_OPUS)
	{
		WaveFormatOpus* pFmt = (WaveFormatOpus *)in_fmtInfo.pFormat;
		AkUInt32 uNumFrames = pFmt->dwTotalPCMFrames;
		AkUInt32 uFrameSizePCM = sizeof(AkInt16) * in_fmtInfo.pFormat->nChannels;

		return (uNumFrames * uFrameSizePCM) + in_uDataOffset;
	}

	AKASSERT(!"Unsupported Decoded media System");
	return 0;
}

AKRESULT CAkBankMgr::DecodeMedia( AkUInt8 * pDst, AkUInt32 uDstSize, AkUInt8 * pSrc, AkUInt32 uMediaSize, AkUInt32 uWaveFmtOffset, AkUInt32 uDataOffset, AkUInt32 uFrameSizePCM, const AkFileParser::FormatInfo& in_rFmtInfo)
{
	AkUInt32 codecID = 0;
	if (in_rFmtInfo.pFormat->wFormatTag == AK_WAVE_FORMAT_VORBIS)
		codecID = AKCODECID_VORBIS;
	else if (in_rFmtInfo.pFormat->wFormatTag == AK_WAVE_FORMAT_OPUS)
		codecID = AKCODECID_AKOPUS;
	else if (in_rFmtInfo.pFormat->wFormatTag == AK_WAVE_FORMAT_OPUS_WEM)
		codecID = AKCODECID_AKOPUS_WEM;
	else
	{
		AKASSERT(!"Unsupported Decode Media Codec.");
	}

	AkUInt32 pluginID = CAkEffectsMgr::GetMergedID(AkPluginTypeCodec, AKCOMPANYID_AUDIOKINETIC, codecID);
	IAkFileCodec * pDecoder = CAkEffectsMgr::AllocFileCodec(pluginID);
	if (!pDecoder)
	{
		return AK_InsufficientMemory;
	}

	// write header

	memcpy( pDst, pSrc, uDataOffset );
	WaveFormatExtensible * pExtensible = (WaveFormatExtensible *) ( pDst + uWaveFmtOffset );
	pExtensible->wFormatTag = AK_WAVE_FORMAT_EXTENSIBLE;
	pExtensible->wBitsPerSample = 16;
	pExtensible->nBlockAlign = (AkUInt16) uFrameSizePCM;

	AkChunkHeader * pDataChunkHdr = (AkChunkHeader *)(pDst + uDataOffset - sizeof(AkChunkHeader));
	if ((uDataOffset & 0x3) != 0)
	{
		//Add a 'junk' chunk to force 4-byte alignment for data chunk.

		AkChunkHeader tempDataChunkHdr = *pDataChunkHdr;

		pDataChunkHdr->ChunkId = AkmmioFOURCC('J', 'U', 'N', 'K');
		pDataChunkHdr->dwChunkSize = 4 - ((uDataOffset + sizeof(AkChunkHeader)) & 0x3);

		memset(pDst + uDataOffset + sizeof(AkChunkHeader), 0, pDataChunkHdr->dwChunkSize);

		uDataOffset += (pDataChunkHdr->dwChunkSize + sizeof(AkChunkHeader));

		pDataChunkHdr = (AkChunkHeader *)(pDst + uDataOffset - sizeof(AkChunkHeader));
		*pDataChunkHdr = tempDataChunkHdr;
	}

	AKASSERT((uDataOffset & 0x3) == 0);

	// write data

	g_csMain.Lock();

	AKRESULT eResult = pDecoder->DecodeFile(pDst + uDataOffset, uDstSize - uDataOffset, pSrc, uMediaSize, pDataChunkHdr->dwChunkSize);

	CAkEffectsMgr::FreeFileCodec(pluginID, pDecoder);

	g_csMain.Unlock();

	return eResult;
}

static void _ReplaceWithDecodedMedia( AkMemPoolId in_poolId, AkUInt8 *& pAllocated, AkUInt32 & uMediaSize )
{
	AkFileParser::FormatInfo fmtInfo;
	AkUInt32 uLoopStart, uLoopEnd, uDataSize, uDataOffset;
	AKRESULT eResult = AkFileParser::Parse( pAllocated, uMediaSize, fmtInfo, NULL, &uLoopStart, &uLoopEnd, &uDataSize, &uDataOffset, NULL, NULL, true );

	if (eResult != AK_Success || uDataSize  + uDataOffset > uMediaSize )
		return; // prefetched (incomplete) media -- do nothing for now, as streamed files are not handled.

	switch ( fmtInfo.pFormat->wFormatTag )
	{
	case AK_WAVE_FORMAT_ADPCM:
	case AK_WAVE_FORMAT_PTADPCM:
		break;
	case AK_WAVE_FORMAT_VORBIS:
	case AK_WAVE_FORMAT_OPUS:
		{
			AkUInt32 uAllocSize = CAkBankMgr::GetBufferSizeForDecodedMedia(fmtInfo, uDataSize, uDataOffset);
			AkUInt8 * pDecoded = (AkUInt8*)AkMalign(in_poolId, uAllocSize, 4);
			if ( !pDecoded )
				return; // not enough memory to create decoded copy

			eResult = CAkBankMgr::DecodeMedia(pDecoded, uAllocSize, pAllocated, uMediaSize, (AkUInt32)((AkUInt8 *)fmtInfo.pFormat - pAllocated), uDataOffset, sizeof(AkInt16) * fmtInfo.pFormat->nChannels, fmtInfo);

			if ( eResult == AK_Success )
			{
				AkFalign(in_poolId, pAllocated);
				pAllocated = pDecoded;
				uMediaSize = uAllocSize;
			}
			else
			{
				AkFalign(in_poolId, pDecoded);
			}

		}
		break;
	}
}

AKRESULT CAkBankMgr::PrepareMedia( CAkUsageSlot* in_pCurrentSlot, AkUInt32 in_dwDataChunkSize, bool in_bDecode )
{
#ifndef PROXYCENTRAL_CONNECTED
	AKASSERT( !"Illegal to Prepare data when running the Wwise sound engine" );
#endif

	AKRESULT eResult = AK_Success;
	if ( in_dwDataChunkSize == 0 )
		return eResult; // Empty chunk -- return immediately

	int iIndexPos = 0;// external from the for for backward cleanup in case of error.
	AkUInt32 uLastReadPosition = 0;
	AkUInt32 uToSkipSize = 0;
	for( ; in_pCurrentSlot->m_uNumLoadedItems < in_pCurrentSlot->m_uIndexSize; ++iIndexPos )
	{
		AkUInt32 uTempToSkipSize = in_pCurrentSlot->m_paLoadedMedia[iIndexPos].uOffset - uLastReadPosition;
		uToSkipSize += uTempToSkipSize;
		uLastReadPosition += uTempToSkipSize;

		AkUniqueID sourceID = in_pCurrentSlot->m_paLoadedMedia[iIndexPos].id;
		if ( sourceID == AK_INVALID_UNIQUE_ID ) // missing file get a cleared entry in media table
		{
			++( in_pCurrentSlot->m_uNumLoadedItems );
			continue;
		}

		AkUInt32 uMediaSize = in_pCurrentSlot->m_paLoadedMedia[iIndexPos].uSize;
		AKASSERT( uMediaSize );

		AkAutoLock<CAkLock> gate( m_MediaLock );

		AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( sourceID );
		if( pMediaEntry )
		{
			pMediaEntry->AddRef();
			if( pMediaEntry->IsDataPrepared() )
			{
				// Data already available, simply return success.
				++( in_pCurrentSlot->m_uNumLoadedItems );
				continue;
			}
		}
		else
		{
			// Add the missing entry
			pMediaEntry = m_MediaHashTable.Set( sourceID );
			if( !pMediaEntry )
			{
				eResult = AK_Fail;
				break;
			}
			pMediaEntry->SetSourceID( sourceID );
		}
		AKASSERT( pMediaEntry );// From this point, we are guaranteed to have one.

		AkUInt8* pAllocated = NULL;

		AkMemPoolId poolId = AkMemID_Media | (in_pCurrentSlot->UseDeviceMemory() ? AkMemType_Device : 0);

		// Take the data and copy it over
		if( pMediaEntry->HasBankSource() )
		{
			// Will allocate pAllocated and set uMediaSize to the effective media size (i.e. size of pAllocated)
			eResult = pMediaEntry->PrepareFromBank(
				pAllocated,
				uMediaSize,
				poolId,
				0 /* Don't know codec ID */
				);
		}
		else
		{
			// Stream/Load it
			AkUInt32 uAlignment = AK_BANK_PLATFORM_DATA_ALIGNMENT;
			AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_MEDIA_SIZE( uAlignment, uMediaSize );

			pAllocated = (AkUInt8*)AkMalign(poolId, uMediaSize, uAlignment);
			if (pAllocated)
			{
				m_MediaLock.Unlock();// no need to keep the lock during the IO. pMediaEntry is safe as it is already addref'ed at this point.

				{
					if( uToSkipSize )
					{
						AkUInt32 uSkipped = 0;
						eResult = m_BankReader.Skip( uToSkipSize, uSkipped );
						if( uToSkipSize != uSkipped )
							eResult = AK_Fail;
						else
						{
							uToSkipSize = 0; // Reset the skip counter for next time.
						}
					}

                    if (eResult == AK_Success)
                    {
                        AkUInt32 uReadSize = 0;
                        eResult = m_BankReader.FillData( pAllocated, uMediaSize, uReadSize );
                        if( eResult == AK_Success && uMediaSize != uReadSize )
                            eResult = AK_Fail;
                        else
						{
                            uLastReadPosition += uReadSize;

							if (in_bDecode)
								_ReplaceWithDecodedMedia(poolId, pAllocated, uMediaSize);
						}
                    }
				}

				m_MediaLock.Lock();
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}

		if( eResult == AK_Success )
		{
			pMediaEntry->SetPreparedData(pAllocated, uMediaSize, poolId);
			++( in_pCurrentSlot->m_uNumLoadedItems );
			continue;
		}

		// Failed, clear the memory...
		if ( pAllocated )
		{
			AkFalign(poolId, pAllocated);
		}

		ReleaseMediaHashTableEntry(pMediaEntry);

		break;
	}

	if( eResult == AK_Success )
	{
		uToSkipSize += in_dwDataChunkSize-uLastReadPosition;
		if( uToSkipSize )
		{
			AkUInt32 uSkipped = 0;
			m_BankReader.Skip( uToSkipSize, uSkipped );
			if( uToSkipSize != uSkipped )
				eResult = AK_Fail;
		}
	}

	if( eResult != AK_Success )
	{
		while(iIndexPos > 0 )
		{
			--iIndexPos;
			AkMediaID mediaID =  in_pCurrentSlot->m_paLoadedMedia[iIndexPos].id;
			if ( mediaID != AK_INVALID_UNIQUE_ID ) // missing file get a cleared entry in media table
				ReleaseSingleMedia( mediaID );
		}
	}
	else
	{
		in_pCurrentSlot->IsMediaPrepared( true );
	}

	return eResult;
}

void CAkBankMgr::UnloadMedia( CAkUsageSlot* in_pCurrentSlot )
{
	if( in_pCurrentSlot->m_paLoadedMedia )
	{
		AkAutoLock<CAkLock> gate( m_MediaLock );// Will have to decide if performance would be better if acquiring the lock inside the for loop.
		while( in_pCurrentSlot->m_uNumLoadedItems )
		{
			--in_pCurrentSlot->m_uNumLoadedItems;

			AkMediaID mediaID = in_pCurrentSlot->m_paLoadedMedia[ in_pCurrentSlot->m_uNumLoadedItems ].id;
			if ( mediaID == AK_INVALID_UNIQUE_ID ) // missing file get a cleared entry in media table
				continue;

			AkMediaHashTable::IteratorEx iter = m_MediaHashTable.FindEx( mediaID );
			AKASSERT( iter != m_MediaHashTable.End() ); // this can indicate a corruption of m_paLoadedMedia

			if( iter != m_MediaHashTable.End() )
			{
				AkMediaEntry& rMediaEntry = iter.pItem->Assoc.item;
				rMediaEntry.RemoveAlternateBank( in_pCurrentSlot );
				MONITOR_MEDIAPREPARED( rMediaEntry );

				if( rMediaEntry.Release() == 0 ) // if the memory was released
				{
					m_MediaHashTable.Erase( iter );
				}
			}
		}
	}
}

void CAkBankMgr::UnPrepareMedia( CAkUsageSlot* in_pCurrentSlot )
{
	if( !in_pCurrentSlot->IsMediaPrepared() )
		return;	// Wasn't prepared.

	if( in_pCurrentSlot->m_paLoadedMedia )
	{
		{// Locking bracket.
			AkAutoLock<CAkLock> gate( m_MediaLock );// Will have to decide if performance would be better if acquiring the lock inside the for loop.
			for( AkUInt32 i = 0; i < in_pCurrentSlot->m_uNumLoadedItems; ++i )
			{
				AkMediaID mediaID = in_pCurrentSlot->m_paLoadedMedia[ i ].id;
				if ( mediaID == AK_INVALID_UNIQUE_ID ) // missing file get a cleared entry in media table
					continue;

				AkMediaHashTable::IteratorEx iter = m_MediaHashTable.FindEx( mediaID );
				AKASSERT( iter != m_MediaHashTable.End() ); // this can indicate a corruption of m_paLoadedMedia

				if( iter != m_MediaHashTable.End() )
				{
					if( iter.pItem->Assoc.item.Release() == 0 ) // if the memory was released
					{
						m_MediaHashTable.Erase( iter );
					}
				}
			}
		}

		in_pCurrentSlot->IsMediaPrepared( false );
	}
}

AkMediaInfo CAkBankMgr::GetMedia( AkMediaID in_mediaId, CAkUsageSlot* &out_pMediaSlot )
{
	AkMediaInfo returnedMediaInfo(NULL, 0, AK_INVALID_POOL_ID);

	AkAutoLock<CAkBankList> BankListGate( m_BankList ); // CAkBankList lock must be acquired before MediaLock.
	AkAutoLock<CAkLock> gate( m_MediaLock );

	AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( in_mediaId );

	if( pMediaEntry )
	{
		pMediaEntry->GetMedia( returnedMediaInfo, out_pMediaSlot );
	}

	return returnedMediaInfo;
}

// GetMedia() and ReleaseMedia() calls must be balanced.
void CAkBankMgr::ReleaseMedia( AkMediaID in_mediaId )
{
	AkAutoLock<CAkBankList> BankListGate( m_BankList );
	AkAutoLock<CAkLock> gate( m_MediaLock );
	AkMediaHashTable::IteratorEx iter = m_MediaHashTable.FindEx( in_mediaId );

	if( iter != m_MediaHashTable.End() )
	{
		AkMediaEntry& rMediaEntry = iter.pItem->Assoc.item;

		if( rMediaEntry.Release() == 0 ) // if the memory was released
		{
			m_MediaHashTable.Erase( iter );
		}
	}
}
AKRESULT CAkBankMgr::PrepareSingleMedia( AkSrcTypeInfo& in_rmediaInfo )
{
	if( m_bAccumulating )
	{
		AkSrcTypeInfo * pItem = m_PreparationAccumulator.AddNoSetKey( in_rmediaInfo.mediaInfo ); // allows duplicate keys.
		if( !pItem )
			return AK_InsufficientMemory;
		else
		{
			*pItem = in_rmediaInfo;
			return AK_Success;
		}
	}
	else
	{
		return LoadSingleMedia( in_rmediaInfo );
	}
}

void CAkBankMgr::UnprepareSingleMedia( AkUniqueID in_SourceID )
{
	// In accumulation mode, UnprepareSingleMedia should do nothing since PrepareSingleMedia did nothing.
	if( !m_bAccumulating )
		ReleaseSingleMedia( in_SourceID );
}

void CAkBankMgr::EnableAccumulation()
{
	AKASSERT( m_bAccumulating == false );
	m_bAccumulating = true;
}

void CAkBankMgr::DisableAccumulation()
{
	AKASSERT( m_bAccumulating == true );
	m_bAccumulating = false;
	m_PreparationAccumulator.RemoveAll();
}

AKRESULT CAkBankMgr::ProcessAccumulated()
{
	AKRESULT eResult = AK_Success;

	for( AkSortedPreparationList::Iterator iter = m_PreparationAccumulator.Begin();
		iter != m_PreparationAccumulator.End();
		++iter )
	{
		eResult = LoadSingleMedia( *iter );
		if( eResult != AK_Success )
		{
			// Reverse Cleanup.
			for( AkSortedPreparationList::Iterator iterDestructor = m_PreparationAccumulator.Begin();
					iterDestructor != iter;
					++iterDestructor )
			{
				ReleaseSingleMedia( (*iterDestructor).mediaInfo.sourceID );
			}
			break; // Will return last error code.
		}
	}

	return eResult;
}

AKRESULT CAkBankMgr::LoadSingleMedia( AkSrcTypeInfo& in_rMediaInfo )
{
#ifndef PROXYCENTRAL_CONNECTED
	AKASSERT( !"Illegal to Prepare data when running the Wwise sound engine" );
#endif

	AkUInt32 uMediaSize = in_rMediaInfo.mediaInfo.uInMemoryMediaSize;
	if( uMediaSize == 0 || (in_rMediaInfo.mediaInfo.Type == SrcTypeFile && !in_rMediaInfo.mediaInfo.bPrefetch) )
	{
		// Media is not meant to be loaded
		return AK_Success;
	}

	AkAutoLock<CAkLock> gate( m_MediaLock );

	AkUniqueID sourceID = in_rMediaInfo.mediaInfo.sourceID;
	AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( sourceID );
	if( pMediaEntry )
	{
		pMediaEntry->AddRef();
		if( pMediaEntry->IsDataPrepared() )
		{
			// Data already available, simply return success.
			return AK_Success;
		}
	}
	else
	{
		// Add the missing entry
		pMediaEntry = m_MediaHashTable.Set( sourceID );
		if( !pMediaEntry )
			return AK_Fail;
		pMediaEntry->SetSourceID( sourceID );
	}
	AKASSERT( pMediaEntry );// From this point, we are guaranteed to have one.

	AkUInt8* pAllocated = NULL;

	// Take the data and copy it over
	AKRESULT eResult = AK_Success;

	AkCodecID codecId = CODECID_FROM_PLUGINID(in_rMediaInfo.dwID);
	AkMemPoolId poolId = AkMemID_Media | (codecId == AKCODECID_XMA ? AkMemType_Device : 0);

	if( pMediaEntry->HasBankSource() )
	{
		// Will allocate pAllocated and set uMediaSize to the effective media size (i.e. size of pAllocated)
		eResult = pMediaEntry->PrepareFromBank(
			pAllocated,
			uMediaSize,
			poolId,
			codecId
			);
	}
	else
	{
		// Stream/Load it
		AkUInt32 uAlignment = AK_BANK_PLATFORM_DATA_ALIGNMENT;
		AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_CODECID( uAlignment, codecId)

		pAllocated = (AkUInt8*)AkMalign(poolId, uMediaSize, uAlignment);
		if(pAllocated)
		{
			m_MediaLock.Unlock();// no need to keep the lock during the IO. pMediaEntry is safe as it is already addref'ed at this point.
			eResult = LoadSoundFromFile( in_rMediaInfo, pAllocated );
			m_MediaLock.Lock();
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	if( eResult == AK_Success )
	{
		AKASSERT( pAllocated != NULL );
		pMediaEntry->SetPreparedData(pAllocated, uMediaSize, poolId);
	}
	else
	{
		// Failed, clear the memory...
		if ( pAllocated )
		{
			AkFalign(poolId, pAllocated);
		}

		ReleaseMediaHashTableEntry(pMediaEntry);
	}

	return eResult;
}

void CAkBankMgr::ReleaseSingleMedia( AkUniqueID in_SourceID )
{
	AkAutoLock<CAkLock> gate( m_MediaLock );

	AkMediaHashTable::IteratorEx iter = m_MediaHashTable.FindEx( in_SourceID );

	if( iter != m_MediaHashTable.End() )
	{
		AkMediaEntry& rMediaEntry = iter.pItem->Assoc.item;
		if( rMediaEntry.Release() == 0 ) // if the memory was released
		{
			m_MediaHashTable.Erase( iter );
		}
	}
}

AKRESULT CAkBankMgr::AddMediaToTable( AkSourceSettings * in_pSourceSettings, AkUInt32 in_uNumSourceSettings )
{
#ifndef PROXYCENTRAL_CONNECTED
	AKASSERT( !"Illegal to Prepare data when running the Wwise sound engine" );
#endif

	AKRESULT eResult = AK_Success;

	if( in_uNumSourceSettings == 0 )
	{
		// If the size is 0, well... there is nothing to prepare.
		return AK_Success;
	}

	if( in_pSourceSettings == NULL )
	{
		return AK_Fail;
	}

	AkAutoLock<CAkLock> gate( m_MediaLock );
	AkUInt32 AddedCnt;

	for( AddedCnt = 0; AddedCnt < in_uNumSourceSettings; AddedCnt++)
	{
		AkSourceSettings * pSourceSettings = &in_pSourceSettings[AddedCnt];

		if( pSourceSettings->pMediaMemory == NULL || pSourceSettings->uMediaSize == 0)
		{
			eResult = AK_Fail;
			break;
		}

		AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( pSourceSettings->sourceID );
		if( pMediaEntry )
		{
			pMediaEntry->AddRef();
		}
		else
		{
			// Add the missing entry
			pMediaEntry = m_MediaHashTable.Set( pSourceSettings->sourceID );
			if( !pMediaEntry )
			{
				eResult = AK_Fail;
				break;
			}
			pMediaEntry->SetSourceID( pSourceSettings->sourceID );
		}
		AKASSERT( pMediaEntry );// From this point, we are guaranteed to have one.

		eResult = pMediaEntry->AddAlternateSource(pSourceSettings->pMediaMemory, pSourceSettings->uMediaSize, NULL );
		if( eResult != AK_Success )
		{
			break;
		}
	}

	if( eResult != AK_Success )
	{
		// Undo everything we have done!
		for(AkUInt32 i = 0; i <= AddedCnt; i++)
		{
			AkSourceSettings * pSourceSettings = &in_pSourceSettings[i];
			AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( pSourceSettings->sourceID );
			if( pMediaEntry )
			{
				pMediaEntry->RemoveAlternateSourceWithoutSlot(pSourceSettings);
				ReleaseMediaHashTableEntry(pMediaEntry);
			}
		}
	}

	return eResult;
}

AKRESULT CAkBankMgr::RemoveMediaFromTable( AkSourceSettings * in_pSourceSettings, AkUInt32 in_uNumSourceSettings )
{
	if( in_uNumSourceSettings == 0 )
	{
		// If the size is 0, well... there is nothing to prepare.
		return AK_Success;
	}

	if( in_pSourceSettings == NULL )
	{
		return AK_Fail;
	}

	AkAutoLock<CAkLock> gate( m_MediaLock );

	for(AkUInt32 i = 0; i < in_uNumSourceSettings; i++)
	{
		AkSourceSettings * pSourceSettings = &in_pSourceSettings[i];
		AkMediaEntry* pMediaEntry = m_MediaHashTable.Exists( pSourceSettings->sourceID );
		if( pMediaEntry )
		{
			pMediaEntry->RemoveAlternateSourceWithoutSlot(pSourceSettings);
			ReleaseMediaHashTableEntry(pMediaEntry);
		}
	}

	return AK_Success;
}

AkUInt32 CAkBankMgr::ReleaseMediaHashTableEntry(AkMediaEntry* in_pMediaEntryToRemove)
{
	AkUInt32 uRefCount = in_pMediaEntryToRemove->Release();
	if( uRefCount == 0 ) // if the memory was released
	{
		m_MediaHashTable.Unset( in_pMediaEntryToRemove->GetSourceID() );
	}

	return uRefCount;
}


void CAkBankMgr::SignalLastBankUnloaded()
{
	CAkFunctionCritical SpaceSetAsCritical;	
	// Clear bank states
	g_pStateMgr->RemoveAllStateGroups( true );
}

AKRESULT CAkBankMgr::LoadSource(AkUInt8*& io_pData, AkUInt32 &io_ulDataSize, AkBankSourceData& out_rSource)
{
	memset(&out_rSource, 0, sizeof(out_rSource));

	//Read Source info
	out_rSource.m_PluginID = READBANKDATA(AkUInt32, io_pData, io_ulDataSize);

	AkUInt32 StreamType = READBANKDATA(AkUInt8, io_pData, io_ulDataSize);

	out_rSource.m_MediaInfo.uFileID	= out_rSource.m_MediaInfo.sourceID = READBANKDATA( AkUInt32, io_pData, io_ulDataSize );
	out_rSource.m_MediaInfo.uInMemoryMediaSize = READBANKDATA( AkUInt32, io_pData, io_ulDataSize );//in bytes

	AkUInt8 byBitVector = READBANKDATA( AkUInt8, io_pData, io_ulDataSize );
	out_rSource.m_MediaInfo.bIsLanguageSpecific = GETBANKDATABIT( byBitVector, BANK_BITPOS_SOURCE_LANGUAGE );
	out_rSource.m_MediaInfo.bNonCachable = GETBANKDATABIT( byBitVector, BANK_BITPOS_SOURCE_ISNONCACHABLE );
	out_rSource.m_MediaInfo.bPrefetch = ( StreamType == SourceType_PrefetchStreaming );

	AkPluginType PluginType = (AkPluginType) ( out_rSource.m_PluginID & AkPluginTypeMask );

	if( PluginType == AkPluginTypeCodec )
	{
		if ( StreamType == SourceType_Data )
		{
			// In-memory source.
			out_rSource.m_MediaInfo.Type = SrcTypeMemory;
		}
		else
		{
			// Streaming file source.
			if( StreamType != SourceType_Streaming &&
				StreamType != SourceType_PrefetchStreaming )
			{
				return AK_Fail;
			}
			out_rSource.m_MediaInfo.Type = SrcTypeFile;
		}
	}
	// Set source (according to type).
	else if ( PluginType == AkPluginTypeSource )
	{
		out_rSource.m_uSize = READBANKDATA(AkUInt32, io_pData, io_ulDataSize);
		out_rSource.m_pParam = io_pData;

		// Skip BLOB.
		io_pData += out_rSource.m_uSize;
		io_ulDataSize -= out_rSource.m_uSize;
	}
	else if ( PluginType != AkPluginTypeNone )
	{
		//invalid PluginType
		//do not assert here as this will cause a pseudo-deadlock
		//if we are loading the banks in synchronous mode (WG-1592)
		return AK_Fail;
	}

	return AK_Success;
}

AKRESULT CAkBankMgr::KillSlotSync(CAkUsageSlot * in_pUsageSlot)
{
#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
	AkAutoDenyAllocFailure akAutoDeny;
#endif

	AK::SoundEngine::AkSyncCaller syncLoader;
	AKRESULT eResult = syncLoader.Init();
	AKASSERT(eResult == AK_Success);

	eResult = m_CallbackMgr.AddCookie(&syncLoader);// If the cookie fails to be added, the callback will not be called
	AKASSERT(eResult == AK_Success);

	eResult = KillSlotAsync(in_pUsageSlot, AK::SoundEngine::DefaultBankCallbackFunc, &syncLoader);

	return syncLoader.Wait(eResult);
}

AKRESULT CAkBankMgr::KillSlotAsync( CAkUsageSlot * in_pUsageSlot, AkBankCallbackFunc in_pCallBack, void* in_pCookie )
{
	in_pUsageSlot->RemoveContent();

	// Prepare bank for final release notification
	in_pUsageSlot->m_pfnBankCallback	= in_pCallBack;
	in_pUsageSlot->m_pCookie			= in_pCookie;
	in_pUsageSlot->m_bUsageProhibited = true;

	MONITOR_LOADEDBANK( in_pUsageSlot, false );// Must be done before enqueuing the command as the pUsageSlot could be destroyed after that.

	AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue( QueuedMsgType_KillBank, AkQueuedMsg::Sizeof_KillBank() );
	pItem->killbank.pUsageSlot = in_pUsageSlot;
	g_pAudioMgr->FinishQueueWrite();

	return g_pAudioMgr->RenderAudio( false ); // Do not allow synchronous render from bank thread
}

#ifndef AK_OPTIMIZED
static const char* g_szTypeNames[AkNodeCategory_AuxBus + 1] = {
	"Bus",
	"ActorMixer",
	"Random/Sequence Container",
	"Sound",
	"Switch Container",
	"Blend Container",
	"MusicTrack",
	"MusicSegment",
	"Music Random Sequence Container",
	"Music Switch Container",	
	"Aux Bus"
};

AkBankID CAkBankMgr::FindBankFromObject(AkUniqueID in_idObj)
{
	m_BankList.Lock();
	for (CAkBankList::AkListLoadedBanks::Iterator it = m_BankList.GetUNSAFEBankListRef().Begin(); it != m_BankList.GetUNSAFEBankListRef().End(); ++it)
	{
		for (AkUInt32 i = 0; i < (*it)->m_listLoadedItem.Length(); i++)
		{
			if ((*it)->m_listLoadedItem[i]->key == in_idObj)
			{
				m_BankList.Unlock();
				return (*it)->key.bankID;
			}
		}
	}
	m_BankList.Unlock();
	return AK_INVALID_BANK_ID;
}
#endif
void CAkBankMgr::ReportDuplicateObject(AkUniqueID in_idObj, AkNodeCategory in_eTypeExpected, AkNodeCategory in_eTypeExisting)
{
#ifndef AK_OPTIMIZED
	AkBankID idOldBank = FindBankFromObject(in_idObj);

	AKASSERT(!"Type mismatch while loading bank.");

	char msg[200];
	sprintf(msg, "Type mismatch while loading bank. Object %u is a %s in the currently loading bank. It was a %s in bank %u", in_idObj, g_szTypeNames[in_eTypeExpected], g_szTypeNames[in_eTypeExisting], idOldBank);
	MONITOR_ERRORMSG(msg);
#endif
}


AkUInt16 AkMediaInfo::GetFormatTag() const
{
	AkUInt16 uFmtTag = 0;
	if (pInMemoryData != NULL)
	{
		AKASSERT(uInMemoryDataSize > 0);
		AkFileParser::FormatInfo fmtInfo;
		AkUInt32 uLoopStart, uLoopEnd, uDataSize, uDataOffset;
		AKRESULT eResult = AkFileParser::Parse(pInMemoryData, uInMemoryDataSize, fmtInfo, NULL, &uLoopStart, &uLoopEnd, &uDataSize, &uDataOffset, NULL, NULL, true);
		if (eResult == AK_Success && fmtInfo.pFormat != NULL)
		{
			uFmtTag = fmtInfo.pFormat->wFormatTag;
		}
	}

	return uFmtTag;
}

///////////////////////////////////////////////////////////////////////////////////////
// class AkMediaEntry
///////////////////////////////////////////////////////////////////////////////////////

void AkMediaEntry::AddRef()
{
	++uRefCount;
}

AkUInt32 AkMediaEntry::Release()
{
	AKASSERT( uRefCount );// Should never be 0 at this point
	if( !(--uRefCount) )
	{
		if( m_preparedMediaInfo.pInMemoryData )
		{
			FreeMedia();
		}
		MONITOR_MEDIAPREPARED( *this );
	}
	return uRefCount;
}

AKRESULT AkMediaEntry::AddAlternateSource( AkUInt8* in_pData, AkUInt32 in_uSize, CAkUsageSlot* in_pSlot )
{
	BankSlotItem * item = m_BankSlots.Insert(0);
	if (!item)
		return AK_InsufficientMemory;
	item->info = AkMediaInfo(in_pData, in_uSize, AK_INVALID_POOL_ID);
	item->slot = in_pSlot;
	MONITOR_MEDIAPREPARED(*this);
	return AK_Success;
}

void AkMediaEntry::RemoveAlternateBank( CAkUsageSlot* in_pSlot )
{
	for (AkBankSlotsArray::Iterator it = m_BankSlots.Begin(); it != m_BankSlots.End(); ++it)
	{
		if ((*it).slot == in_pSlot)
		{
			m_BankSlots.EraseSwap(it);
			return;
		}
	}
}

void AkMediaEntry::SetPreparedData(
		AkUInt8* in_pData,
		AkUInt32 in_uSize,
		AkMemPoolId in_poolId
		)
	{
		m_preparedMediaInfo.uInMemoryDataSize = in_uSize;
		m_preparedMediaInfo.pInMemoryData = in_pData;
		m_preparedMediaInfo.poolId = in_poolId;
		AK_PERF_INCREMENT_PREPARED_MEMORY( in_uSize );
		MONITOR_MEDIAPREPARED( *this );
	}

void AkMediaEntry::RemoveAlternateSourceWithoutSlot(AkSourceSettings* in_pSourceToRemove)
{
	AkBankSlotsArray::Iterator it = m_BankSlots.Begin();
	while (it != m_BankSlots.End())
	{
		if( (*it).info.pInMemoryData == in_pSourceToRemove->pMediaMemory )
		{
			AKASSERT( uRefCount <= 1 || !"Caller is trying to free memory in-use, could cause corruption.");
			AKASSERT((*it).slot == NULL);
			m_BankSlots.EraseSwap(it);
			// A new item was swapped in at this position, do not increment the iterator
		}
		else
		{
			++it;
		}
	}

	MONITOR_MEDIAPREPARED(*this);
}

void AkMediaEntry::GetMedia( AkMediaInfo& out_mediaInfo, CAkUsageSlot* &out_pMediaSlot )
{
	AddRef();

	if( m_preparedMediaInfo.pInMemoryData != NULL )
	{
		// We have prepared media, let's use it
		AKASSERT( m_preparedMediaInfo.uInMemoryDataSize );
		out_mediaInfo = m_preparedMediaInfo;
	}
	else
	{
		// No dynamic data available, check for data from bank.
		bool bFound = false;
		AkInt32 iLargestInMemorySize = -1;
		for (AkBankSlotsArray::Iterator it = m_BankSlots.Begin(); it != m_BankSlots.End(); ++it)
		{
			CAkUsageSlot * pSlot = (*it).slot;
			bool bIsProhibited = pSlot && pSlot->m_bUsageProhibited;
			if( !bIsProhibited && (iLargestInMemorySize < (AkInt32)(*it).info.uInMemoryDataSize))
			{
				out_pMediaSlot = pSlot;
				out_mediaInfo = (*it).info; // Get data + size from bank

				//Get the largest size.  If we have both pre-fetch and non-prefect data loaded, take the non-prefetch.
				iLargestInMemorySize = (*it).info.uInMemoryDataSize;

				bFound = true;
			}
		}

		if( bFound )
		{
			if (out_pMediaSlot)
			{
				out_pMediaSlot->AddRef(); // AddRef the Slot as one sound will be using it
			}
		}
		else
		{
			// We end here in case of error or if data was not ready.

			// No AddRef() if not enough memory to trace it. So we simply release.
			// or
			// The entry is in the table, but no data is not yet ready.
			out_mediaInfo.pInMemoryData = NULL;
			out_mediaInfo.uInMemoryDataSize = 0;
			Release();// cancel the AddRef done at the beginning of the function.
		}
	}
}

void AkMediaEntry::FreeMedia()
{
	AKASSERT( m_preparedMediaInfo.uInMemoryDataSize );

	AKASSERT(m_preparedMediaInfo.poolId != AK_INVALID_POOL_ID);
	AkFalign(m_preparedMediaInfo.poolId, m_preparedMediaInfo.pInMemoryData);
	m_preparedMediaInfo.pInMemoryData = NULL;

	AK_PERF_DECREMENT_PREPARED_MEMORY( m_preparedMediaInfo.uInMemoryDataSize );
	m_preparedMediaInfo.uInMemoryDataSize = 0;

	m_preparedMediaInfo.poolId = AK_INVALID_POOL_ID;
}

AKRESULT AkMediaEntry::PrepareFromBank(
	AkUInt8*& out_pAllocatedData,
	AkUInt32& out_uMediaSize,
	AkMemPoolId in_poolId,
	AkCodecID in_codecID // Specify if you know it, otherwise pass 0
)
{
#ifndef PROXYCENTRAL_CONNECTED
	AKASSERT( !"Illegal to Prepare data when running the Wwise sound engine" );
#endif

	AKASSERT( !m_preparedMediaInfo.pInMemoryData && !m_preparedMediaInfo.uInMemoryDataSize );
	AKASSERT( m_BankSlots.Length() != 0 );

	out_uMediaSize = m_BankSlots[0].info.uInMemoryDataSize;

	AkUInt32 uAlignment = AK_BANK_PLATFORM_DATA_ALIGNMENT;
	AK_BANK_ADJUST_DATA_ALIGNMENT_FROM_CODECID_OR_MEDIA_SIZE( uAlignment, in_codecID, out_uMediaSize );

	out_pAllocatedData = (AkUInt8*)AkMalign(in_poolId, out_uMediaSize, uAlignment);
	if (!out_pAllocatedData)
		return AK_InsufficientMemory;

	memcpy( out_pAllocatedData, m_BankSlots[0].info.pInMemoryData, out_uMediaSize );

	return AK_Success;
}
