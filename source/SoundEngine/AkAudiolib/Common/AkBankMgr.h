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
// AkBankMgr.h
//
//////////////////////////////////////////////////////////////////////

#ifndef _BANK_MGR_H_
#define _BANK_MGR_H_

#include "AkBanks.h"
#include "AkBankReader.h"
#include <AK/SoundEngine/Common/AkCallback.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "AkCritical.h"
#include "AkSource.h"
#include "AkBankCallbackMgr.h"
#include "AkBankList.h"
#include "AkParameterNodeBase.h"

namespace AK { namespace SoundEngine { class AkSyncCaller; } }

using namespace AKPLATFORM;
using namespace AkBank;

#define AK_MINIMUM_BANK_SIZE ( sizeof(AkSubchunkHeader) + sizeof( AkBankHeader ) )

// Could be the ID of the SoundBase object directly, maybe no need to create it.
typedef AkUInt32 AkMediaID; // ID of a specific source

extern class CAkBankMgr* g_pBankManager;

namespace AkFileParser
{
	struct FormatInfo;
}

// AkFileNameString
//	A string reference class that may or may not own its own string buffer.
class AkFileNameString
{
public:
	AkFileNameString() : m_pStr(NULL), m_bOwner(false) {}

	// *Must* be called explicitly when you are done with this string to free memory if m_bOwner is true.
	void Term();

	inline const char* Get() const { return m_pStr; }

	// Copy string into a newly allocated buffer, adding the specified extension if not already present.
#ifdef AK_SUPPORT_WCHAR
	AKRESULT Copy(const wchar_t* in_pBankStr, const wchar_t* in_pEnsureFileExtIs = L".bnk");
#endif
	AKRESULT Copy(const char* in_pBankStr, const char* in_pEnsureFileExtIs = ".bnk");

	// If calling Set() with a string that doesn't have the <in_pEnsureFileExtIs> extension, the string will be
	//	copied to a newly allocated buffer and the extension will be appended.
	// If <in_pEnsureFileExtIs> is NULL, or in_pString already has the specified extension, than this AkFileNameString will
	//	reference the external buffer passed in to <in_pString>
	AKRESULT Set(const char* in_pString, const char* in_pEnsureFileExtIs = ".bnk");

private:
	const char* m_pStr; 
	bool m_bOwner; //Memory (m_pStr) is owned and must be freed.
};

struct AkMediaInfo
{
	AkMediaInfo() // NO INITIALIZATION!
	{
	}

	AkMediaInfo( AkUInt8* in_pInMemoryData, AkUInt32 in_uInMemoryDataSize, AkMemPoolId in_poolId )
		: pInMemoryData( in_pInMemoryData )
		, uInMemoryDataSize( in_uInMemoryDataSize )
		, poolId(in_poolId)
	{
	}

	AkUInt16 GetFormatTag() const; //parse the header data to get the format tag.

	AkUInt8*			pInMemoryData;
	AkUInt32			uInMemoryDataSize;
	AkMemPoolId			poolId;
};

class AkMediaEntry
{
	friend class AkMonitor; // for profiling purpose.
private:
	struct BankSlotItem
	{
		AkMediaInfo info;
		CAkUsageSlot* slot;
	};
	typedef AkArray< BankSlotItem, const BankSlotItem& > AkBankSlotsArray;

public:
	// Constructor
	AkMediaEntry()
		: m_preparedMediaInfo(NULL, 0, AK_INVALID_POOL_ID)
		, uRefCount(1)
	{
	}

	~AkMediaEntry()
	{
		AKASSERT( m_BankSlots.Length() == 0);
		m_BankSlots.Term();
	}

	void AddRef();
	AkUInt32 Release();

	void GetMedia( AkMediaInfo& out_mediaInfo, CAkUsageSlot* &out_pMediaSlot );
	void FreeMedia();

	AKRESULT AddAlternateSource( AkUInt8* in_pData, AkUInt32 in_uSize, CAkUsageSlot* in_pSlot );
	void RemoveAlternateBank( CAkUsageSlot* in_pSlot );
	void RemoveAlternateSourceWithoutSlot(AkSourceSettings* in_pSourceToRemove);

	bool IsDataPrepared()
	{ 
		return m_preparedMediaInfo.pInMemoryData != NULL; 
	}

	void SetPreparedData( 
		AkUInt8* in_pData, 
		AkUInt32 in_uSize, 
		AkMemPoolId in_poolId
		);

	AkUInt8* GetPreparedMemoryPointer()
	{ 
		return m_preparedMediaInfo.pInMemoryData; 
	}

	bool HasBankSource()
	{
		return (m_BankSlots.Length() != 0);
	}

	AkUInt32 GetNumBankOptions()
	{
		return m_BankSlots.Length();
	}

	AKRESULT PrepareFromBank(
		AkUInt8*& out_pAllocatedData,
		AkUInt32& out_uMediaSize,
		AkMemPoolId in_poolId,
		AkCodecID in_codecID // Specify if you know it, otherwise pass 0
		);

	AkUniqueID GetSourceID(){ return m_sourceID; }
	void SetSourceID( AkUniqueID in_sourceID ){ m_sourceID = in_sourceID; }


private:

	// Members
	AkMediaInfo m_preparedMediaInfo;

	AkBankSlotsArray m_BankSlots;

	AkUInt32 uRefCount;

	AkUniqueID m_sourceID;
};

struct AkBankCompletionNotifInfo
{
	AkBankCompletionNotifInfo(): pfnBankCallback(NULL), pCookie(NULL) {}
	AkBankCallbackFunc pfnBankCallback;
    void * pCookie;
};

struct AkPrepareEventQueueItemLoad
{
	// TODO use the pointer instead of the ID here, will simplify things...
	AkUInt32	numEvents;
	// To avoid doing 4 bytes allocation, when the size is one, the ID itself will be passed instead of the array.
	union
	{
		AkUniqueID	eventID;
		AkUniqueID* pEventID;
	};
};

struct AkPrepareGameSyncQueueItem
{
	AkGroupType eGroupType;
	AkUInt32	uGroupID;
	bool		bSupported;

	AkUInt32	uNumGameSync;
	union
	{
		AkUInt32 uGameSyncID;
		AkUInt32* pGameSyncID;
	};
};

class CAkUsageSlot AK_FINAL
{
public:
	AkBankKey key;		// for AkListLoadedBanks
	CAkUsageSlot * pNextItem;

	AkUInt8 *	m_pData;
	AkBank::MediaHeader*	m_paLoadedMedia;
	AkUInt32	m_uLoadedDataSize;
	AkUInt32	m_uLoadedMetaDataSize;
	bool		m_bUsageProhibited;

	AkUInt32		m_uNumLoadedItems;
	AkUInt32		m_uIndexSize;
	
	// These two members are there only for the notify completion callback
    AkBankCallbackFunc m_pfnBankCallback;
    void * m_pCookie;

	typedef AkArray<CAkIndexable*, CAkIndexable*, ArrayPoolDefault, AkGrowByPolicy_NoGrow> AkListLoadedItem;
	AkListLoadedItem m_listLoadedItem;	//	Contains the list of CAkIndexable to release in case of unload

	CAkUsageSlot( AkBankKey in_BankKey, AkInt32 in_mainRefCount, AkInt32 in_prepareEventRefCount, AkInt32 in_prepareBankRefCount );
	~CAkUsageSlot();

	void AddRef();
	void Release( bool in_bSkipNotification );
	void AddRefPrepare();
	void ReleasePrepare( bool in_bIsFinal = false );

	void StopContent();
	void RemoveContent();
	void Unload();
	void DeleteDataBlock();

	bool WasLoadedAsABank() const { return m_bWasLoadedAsABank; }
	void WasLoadedAsABank( bool in_bWasLoadedAsABank ){ m_bWasLoadedAsABank = in_bWasLoadedAsABank; }

	bool WasPreparedAsABank(){ return m_iWasPreparedAsABankCounter != 0; }
	void WasPreparedAsABank( bool in_bWasPreparedAsABank )
	{ 
		if( in_bWasPreparedAsABank )
			++m_iWasPreparedAsABankCounter;
		else
			--m_iWasPreparedAsABankCounter;
		AKASSERT( m_iWasPreparedAsABankCounter >=0 );
	}

	bool WasIndexAllocated() { return m_bWasIndexAllocated; }
	void WasIndexAllocated(bool in_bWasIndexAllocated ){ m_bWasIndexAllocated = in_bWasIndexAllocated; }

	bool IsMediaPrepared() { return m_bIsMediaPrepared; }
	void IsMediaPrepared( bool in_bInMediaPrepared ){ m_bIsMediaPrepared = in_bInMediaPrepared; }

	void CheckFreeIndexArray();

	void UnloadCompletionNotification();

	inline void SetUseDeviceMemory()
	{
		m_bUseDeviceMemory = 1;
	}

	inline bool UseDeviceMemory() const { return m_bUseDeviceMemory != 0; }

	inline void SetAligment(AkUInt32 in_uAlignment) { m_uAlignment = in_uAlignment; }
	inline AkUInt32 Alignment() const { return m_uAlignment; }

private:
	AkAtomic32 m_iRefCount;
	AkAtomic32 m_iPrepareRefCount;
	
	AkUInt8 m_bWasLoadedAsABank		:1;
	AkUInt8 m_bWasIndexAllocated	:1;
	AkUInt8 m_bIsMediaPrepared		:1;
	AkUInt8 m_bUseDeviceMemory		:1;

private:
	AkUInt32 m_uAlignment; // Media memory alignment
	AkInt32 m_iWasPreparedAsABankCounter;
};

enum AkBankLoadFlag
{
	AkBankLoadFlag_None,
	AkBankLoadFlag_UsingFileID,
	AkBankLoadFlag_FromMemoryInPlace, //In-place: read bank from memory, use buffer passed in to store media
	AkBankLoadFlag_FromMemoryOutOfPlace //out-of-place: read bank from memory, but allocate or use existing memory pool for media
};

struct AkBankSourceData
{
	AkUniqueID			m_srcID; 
	AkUInt32			m_PluginID;
	AkMediaInformation	m_MediaInfo;
	void *				m_pParam;
	AkUInt32			m_uSize;
};

// The Bank Manager
//		Manage bank slots allocation
//		Loads and unloads banks
class CAkBankMgr
{
	friend class CAkUsageSlot;
	friend class AkMonitor;
public:
	enum AkBankQueueItemType
	{
		QueueItemInvalid = -1,
		QueueItemLoad,
		QueueItemUnload,
		QueueItemPrepareEvent,
		QueueItemUnprepareEvent,
		QueueItemSupportedGameSync,
		QueueItemUnprepareAllEvents,
		QueueItemPrepareBank,
		QueueItemUnprepareBank,
		QueueItemClearBanks,
		QueueItemLoadMediaFile,
		QueueItemUnloadMediaFile,
		QueueItemLoadMediaFileSwap

	};

	// Structure used to Store the information about a loaded bank
	// This information will mostly be used to identify a free Slot and keep information
	// on what has to be removed from the hyerarchy once removed

	struct AkBankQueueItemLoad
	{
		const void* pInMemoryBank;
		AkUInt32 ui32InMemoryBankSize;
	};

	struct AkBankPreparation
	{
		AK::SoundEngine::AkBankContent	uFlags;
		bool							bDecode;
	};

	struct AkLoadMediaFile
	{
		AkUniqueID	MediaID;
	};

	struct AkBankQueueItem
	{
		AkBankQueueItem() : eType(QueueItemInvalid), bankID(AK_INVALID_BANK_ID) {}

		AkBankQueueItemType			eType;
		AkBankID					bankID;
		AkFileNameString			fileName;
		AkBankCompletionNotifInfo	callbackInfo;
		AkBankLoadFlag				bankLoadFlag;
		union
		{
			AkPrepareEventQueueItemLoad prepare;
			AkBankQueueItemLoad load;
			AkPrepareGameSyncQueueItem gameSync;
			AkBankPreparation bankPreparation;
			AkLoadMediaFile loadMediaFile;
		};

		const void* GetFromMemPtr()
		{
			return bankLoadFlag == AkBankLoadFlag_FromMemoryInPlace ? load.pInMemoryBank : NULL;
		}

		AkBankKey GetBankKey()
		{
			return AkBankKey(bankID, GetFromMemPtr());
		}

		const char* GetBankString()
		{
			return fileName.Get();
		}

		// It is the AkBankMgr's responisibility to call Term() once this AkBankQueueItem is no longer needed.
		void Term()
		{
			fileName.Term();
		}
	};

	// Constructor and Destructor
	CAkBankMgr();
	virtual	~CAkBankMgr();

	virtual AKRESULT Init();
	virtual void Term();


	virtual AKRESULT QueueBankCommand( AkBankQueueItem in_Item )
	{
		if (in_Item.callbackInfo.pfnBankCallback)
			m_CallbackMgr.AddCookie(in_Item.callbackInfo.pCookie);

		BankMonitorNotification(in_Item);
		ExecuteCommand(in_Item); 
		in_Item.Term();

		//Callers expect that this function returns AK_Success if *queuing* of the command succeeds. 
		//This is trivially true in the non-threaded bank manager.
		//The result of ExecuteCommand() is saved to m_OperationResult.
		return AK_Success;
	};

	virtual AKRESULT InitSyncOp(AK::SoundEngine::AkSyncCaller& /*in_syncLoader*/)							{return AK_Success;}
	virtual AKRESULT WaitForSyncOp(AK::SoundEngine::AkSyncCaller& /*in_syncLoader*/, AKRESULT in_eResult)	{ return in_eResult == AK_Success ? m_OperationResult : in_eResult; }

	// Safe way to actually do the callback
	// Same prototype than the callback itself, with the exception that it may actually not do the callback if the
	// event was canceled
	void DoCallback(
		AkBankCallbackFunc	in_pfnBankCallback,
		AkBankID			in_bankID,
		const void *		in_pInMemoryPtr,
		AKRESULT			in_eLoadResult,
		void *				in_pCookie
		)
	{
		// Save the result to m_OperationResult.  This allows us to retrieve the return code is we are not using a callback. (ie in the non-threaded bank manager)
		m_OperationResult = in_eLoadResult;
		m_CallbackMgr.DoCallback(
			in_pfnBankCallback,
			in_bankID,
			in_pInMemoryPtr,
			in_eLoadResult,
			in_pCookie
			);
	}

	void ClearPreparedEvents();

	AKRESULT SetBankLoadIOSettings( AkReal32 in_fThroughput, AkPriority in_priority ) { return m_BankReader.SetBankLoadIOSettings( in_fThroughput, in_priority ); }

	void StopBankContent( AkUniqueID in_BankID );

	// This template is to be used by all STD audionodes
	// Any audio node that has special requirements on bank loading ( as sounds and track ) should not use this template.
	// It is exposed in header file so that external plugins can use this template too
	template <class T_Type, class T_Index_Type>
	AKRESULT StdBankRead( const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot, CAkIndexItem<T_Index_Type*>& in_rIndex )
	{
		AKRESULT eResult = AK_Success;

		const void* pData = m_BankReader.GetData( in_rSection.dwSectionSize );

		if(!pData)
		{
			return AK_Fail;
		}

		AkUniqueID ulID = AK::ReadUnaligned<AkUInt32>((AkUInt8*)pData);

		T_Type* pObject = static_cast<T_Type*>( in_rIndex.GetPtrAndAddRef( ulID ) );
		if( pObject == NULL )
		{
			CAkFunctionCritical SpaceSetAsCritical;
			pObject = T_Type::Create(ulID);
			if( !pObject )
			{
				eResult = AK_Fail;
			}
			else
			{
				eResult = pObject->SetInitialValues( (AkUInt8*)pData, in_rSection.dwSectionSize );
				if( eResult != AK_Success )
				{
					pObject->Release();					
				}
			}
		}
		
		if(eResult == AK_Success)
		{
			AddLoadedItem( in_pUsageSlot,pObject ); //This will allow it to be removed on unload
		}

		m_BankReader.ReleaseData();

		if (eResult == AK_DuplicateUniqueID)
		{
			//Not a critical error.  Return success so that other objects can load.		
			return AK_Success;
		}

		return eResult;
	}

	template<class T>
	AKRESULT ReadSourceParent(const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot, AkUInt32 /*in_dwBankID*/ )
	{
		AKRESULT eResult = AK_Success;

		const void* pData = m_BankReader.GetData( in_rSection.dwSectionSize );
		if(!pData)
			return AK_Fail;

		AkUniqueID ulID = AK::ReadUnaligned<AkUInt32>((AkUInt8*)pData);

		T* pSound = static_cast<T*>( g_pIndex->GetNodePtrAndAddRef( ulID, AkNodeType_Default ) );
		if( pSound )
		{			
			//The sound already exists, simply update it
			if( !pSound->SourceLoaded() || !pSound->HasBankSource() )
			{
				CAkFunctionCritical SpaceSetAsCritical;
				eResult = pSound->SetInitialValues( (AkUInt8*)pData, in_rSection.dwSectionSize, in_pUsageSlot, true );			
			}

			if (eResult != AK_Success)
			{
				pSound->Release();
				m_BankReader.ReleaseData();
				if (eResult == AK_DuplicateUniqueID)
					return AK_Success;	//Other objects can load.

				return eResult;
			}				
		}
		else
		{
			CAkFunctionCritical SpaceSetAsCritical;
			pSound = T::Create(ulID);
			if(!pSound)
			{
				eResult = AK_Fail;
			}
			else
			{
				eResult = pSound->SetInitialValues( (AkUInt8*)pData, in_rSection.dwSectionSize, in_pUsageSlot, false );
				if(eResult != AK_Success)
				{
					pSound->Release();
				}
			}
		}

		if(eResult == AK_Success)
		{
			AddLoadedItem( in_pUsageSlot, pSound ); //This will allow it to be removed on unload
		}

		m_BankReader.ReleaseData();

		return eResult;
	}

	static AKRESULT LoadSource(AkUInt8* & io_pData, AkUInt32 &io_ulDataSize, AkBankSourceData& out_rSource);

	friend AKRESULT ReadTrack( const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot, AkUInt32 in_dwBankID );

	AKRESULT LoadSoundFromFile( AkSrcTypeInfo& in_rMediaInfo, AkUInt8* io_pData );


	AKRESULT LoadMediaIndex( CAkUsageSlot* in_pCurrentSlot, AkUInt32 in_uIndexChunkSize, bool in_bIsLoadedFromMemory );

	AKRESULT LoadMedia( AkUInt8* in_pDataBank, CAkUsageSlot* in_pCurrentSlot );
	AKRESULT PrepareMedia( CAkUsageSlot* in_pCurrentSlot, AkUInt32 in_dwDataChunkSize, bool in_bDecode );
	void UnloadMedia( CAkUsageSlot* in_pCurrentSlot );	// Works to cancel both Load and index.
	void UnPrepareMedia( CAkUsageSlot* in_pCurrentSlot );
	static AkUInt32 GetBufferSizeForDecodedMedia(AkFileParser::FormatInfo& in_fmtInfo, AkUInt32 in_uEncodedDataSize, AkUInt32 in_uDataOffset);
	static AKRESULT DecodeMedia( AkUInt8 * pDst, AkUInt32 uDstSize, AkUInt8 * pSrc, AkUInt32 uMediaSize, AkUInt32 uWaveFmtOffset, AkUInt32 uDataOffset, AkUInt32 uFrameSizePCM, const AkFileParser::FormatInfo& in_rFmtInfo);

	AkUInt32 ReleaseMediaHashTableEntry(AkMediaEntry* in_pMediaEntryToRemove);

	// Returns the best it can do. 
	// If the media is in memory, give it in memory.
	// If the media is prefetched, give the prefetch.
	AkMediaInfo GetMedia( AkMediaID in_mediaId, CAkUsageSlot* &out_pMediaSlot );

	// GetMedia() and ReleaseMedia() calls must be balanced. 
	void ReleaseMedia( AkMediaID in_mediaId );

	AKRESULT LoadSingleMedia( AkSrcTypeInfo& in_rmediaInfo );
	void ReleaseSingleMedia( AkUniqueID in_SourceID );

	AKRESULT AddMediaToTable( AkSourceSettings * in_pSourceSettings, AkUInt32 in_uNumSourceSettings );
	AKRESULT RemoveMediaFromTable( AkSourceSettings * in_pSourceSettings, AkUInt32 in_uNumSourceSettings );

	AKRESULT PrepareSingleMedia( AkSrcTypeInfo& in_rmediaInfo );
	void UnprepareSingleMedia( AkUniqueID in_SourceID );	

	static void SignalLastBankUnloaded();

	// Get Bank ID, and optionally check for proper memory alignment (as per bank header).
	static AKRESULT GetBankInfoFromPtr(const void* in_pData, AkUInt32 in_uSize, bool in_bCheckAlignment, AkBankID & out_bankID);

	// Not used without the bank thread
	virtual void StopThread(){}
	virtual void CancelCookie( void* /*in_pCookie*/ ) {}

#ifndef AK_OPTIMIZED
	AkBankID FindBankFromObject(AkUniqueID in_idObj);
#endif	
	void ReportDuplicateObject(AkUniqueID in_idObj, AkNodeCategory in_eTypeExpected, AkNodeCategory in_eTypeExisting);

protected:
	

	enum AkLoadBankDataMode
	{
		AkLoadBankDataMode_OneBlock,			// Normal bank load, complete.
		AkLoadBankDataMode_MediaAndStructure,	// Prepare Structure AND Media.
		AkLoadBankDataMode_Structure,			// Prepare Structure only.
		AkLoadBankDataMode_Media				// Prepare Media only.
	};
	// Load the Specified bank
	AKRESULT LoadBank( AkBankQueueItem in_Item, CAkUsageSlot *& out_pUsageSlot, AkLoadBankDataMode in_eLoadMode, bool in_bIsFromPrepareBank, bool in_bDecode );	

	AKRESULT LoadBankPre( AkBankQueueItem& in_rItem );
	// Called upon an unload request, the Unload may be delayed if the Bank is un use.
	AKRESULT UnloadBankPre( AkBankQueueItem in_Item );

	AKRESULT ClearBanksInternal( AkBankQueueItem in_Item );

public:// Recursive Event preparation system needs them.
	AKRESULT PrepareEvents( AkBankQueueItem in_Item );
	AKRESULT PrepareEvent( AkBankQueueItem in_Item, AkUniqueID in_EventID );
	AKRESULT UnprepareEvents( AkBankQueueItem in_Item );
	AKRESULT UnprepareEvent( AkUniqueID in_EventID );
	void	 UnprepareEvent( CAkEvent* in_pEvent, bool in_bCompleteUnprepare = false );
protected:
	AKRESULT UnprepareAllEvents( AkBankQueueItem in_Item );
	AKRESULT PrepareEventInternal( AkBankQueueItem& in_Item, CAkEvent* in_pEvent );
	void UnprepareEventInternal( CAkEvent* in_pEvent );

	AKRESULT PrepareGameSync( AkBankQueueItem in_Item );

	AKRESULT PrepareBank( AkBankQueueItem in_Item );
	AKRESULT UnPrepareBank( AkBankQueueItem in_Item );

	AKRESULT LoadMediaFile( AkBankQueueItem in_Item );
	AKRESULT UnloadMediaFile( AkBankQueueItem in_Item );
	AKRESULT MediaSwap( AkBankQueueItem in_Item );

	struct AkAlignedPtrSet
	{
		AkUInt8* pAllocated;
		AkUInt32 uMediaSize;
	};

	AKRESULT LoadMediaFile( const char* in_pszFileName, AkAlignedPtrSet& out_AlignedPtrSet );

	// WG-9055 - do not pass AkBankQueueItem in_rItem by reference.
	AKRESULT PrepareBankInternal( AkBankQueueItem in_rItem, AkLoadBankDataMode in_LoadBankMode, bool in_bIsFromPrepareBank = false, bool in_bDecode = false );
	void UnPrepareBankInternal( AkUniqueID in_BankID, bool in_bIsFromPrepareBank = false, bool in_bIsFinal = false );

	//NOT TO BE CALLED DIRECTLY, internal tool only
	void UnloadAll();
	void UnPrepareAllBank();

	//Add the CAkIndexable* to the ToBeRemoved list associated with this slot
	void AddLoadedItem( CAkUsageSlot* in_pUsageSlot, CAkIndexable* in_pIndexable);

	AKRESULT ProcessBankHeader( AkBankHeader& in_rBankHeader, bool& out_bBackwardDataCompatibilityMode );
	AKRESULT ProcessDataChunk( AkUInt32 in_dwDataChunkSize, CAkUsageSlot* in_pUsageSlot);
	AKRESULT ProcessHircChunk( CAkUsageSlot* in_pUsageSlot, AkUInt32 in_dwBankID );
	AKRESULT ProcessStringMappingChunk( AkUInt32 in_dwDataChunkSize, CAkUsageSlot* in_pUsageSlot );
	AKRESULT ProcessGlobalSettingsChunk( AkUInt32 in_dwDataChunkSize );
	AKRESULT ProcessEnvSettingsChunk( AkUInt32 in_dwDataChunkSize );
	AKRESULT ProcessCustomPlatformChunk(AkUInt32 in_dwDataChunkSize );

	AKRESULT ReadState( const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot );
	AKRESULT ReadBus(const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot);
	AKRESULT ReadAction( const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot );
	AKRESULT ReadEvent( const AKBKSubHircSection& in_rSection, CAkUsageSlot* in_pUsageSlot );	

	AKRESULT ProcessPluginChunk(AkUInt32 in_dwDataChunkSize);

	CAkBankReader m_BankReader;

	AKRESULT SetFileReader(AkFileID in_FileID, const char* in_strBankName, AkUInt32 in_uFileOffset, AkUInt32 in_codecID, void * in_pCookie, bool in_bIsLanguageSpecific = true);

	AKRESULT ExecuteCommand( CAkBankMgr::AkBankQueueItem& in_Item );

	void BankMonitorNotification( CAkBankMgr::AkBankQueueItem & in_Item );

	void NotifyCompletion( AkBankQueueItem & in_rItem, AKRESULT in_OperationResult );

	CAkLock m_MediaLock;

	// Hash table containing all ready media.
	// Actual problem : some of this information is a duplicate of the index content. but it would be a faster
	// seek in a hash table than in the linear indexes.
	// We could find better memory usage, but will be more complex too.
	typedef AkHashList< AkMediaID, AkMediaEntry > AkMediaHashTable;
	AkMediaHashTable m_MediaHashTable;

	CAkBankList m_BankList;

	////////////////////////////////////////////////////////////////////////////////////////
	// This funstion is to be used with precautions.
	// You must acquire the CAkBankList lock for all the time you are manipulating this list.
	CAkBankList& GetBankListRef(){ return m_BankList; }
	////////////////////////////////////////////////////////////////////////////////////////

	void FlushFileNameTable();
	
	virtual AKRESULT KillSlot(CAkUsageSlot * in_pUsageSlot, AkBankCallbackFunc in_pCallBack, void* in_pCookie){ return KillSlotSync(in_pUsageSlot); }

	// virtual KillSlot() will call one of these:
	AKRESULT KillSlotSync(CAkUsageSlot * in_pUsageSlot);
	AKRESULT KillSlotAsync(CAkUsageSlot * in_pUsageSlot, AkBankCallbackFunc in_pCallBack, void* in_pCookie);

	// This map is used to store bank names that are in 'linked' sound banks.  That is, banks that contain structures that are referenced from events in another bank.
	//	We do not currently keep track of what banks link to which other banks, so we do not free these string when a bank is unloaded.
	typedef AkHashList< AkBankID, char* > AkIDtoStringHash;
	AkIDtoStringHash m_BankIDToFileName;	

	struct AkSortedPreparationListGetKey
	{
		/// Default policy.
		static AkForceInline AkMediaInformation& Get( AkSrcTypeInfo& in_item ) 
		{
			return in_item.mediaInfo;
		}
	};
	typedef AkSortedKeyArray<AkMediaInformation, AkSrcTypeInfo, ArrayPoolDefault, AkSortedPreparationListGetKey > AkSortedPreparationList;

	void EnableAccumulation();
	void DisableAccumulation();
	AKRESULT ProcessAccumulated();

	AkSortedPreparationList m_PreparationAccumulator;
	bool					m_bAccumulating;
	AKRESULT				m_OperationResult;

protected:

	CAkBankCallbackMgr m_CallbackMgr;
};

#endif



