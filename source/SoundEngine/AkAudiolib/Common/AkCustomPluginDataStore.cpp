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

#include "stdafx.h"
#include "AkCustomPluginDataStore.h"
#include <AK/SoundEngine/Common/AkCommonDefs.h>

AkCustomPluginDataStore::AkPluginCustomGameDataList AkCustomPluginDataStore::m_PluginCustomGameData; // Custom game data for plugins	

AKRESULT AkCustomPluginDataStore::SetPluginCustomGameData(AkUniqueID in_busID, AkGameObjectID in_busObjectID, AkPluginType in_eType, AkUInt32 in_uCompanyID, AkUInt32 in_uPluginID, void* in_pData, AkUInt32 in_Size, bool in_bManaged)
{
	AKRESULT res = AK_Success;

	AkUInt32 uMergedPluginID = AKMAKECLASSID(in_eType, in_uCompanyID, in_uPluginID);
	AkPluginCustomGameDataListKey key = AkPluginCustomGameDataListKey(in_busID, in_busObjectID, uMergedPluginID);
	AkPluginCustomGameData* pListItem = m_PluginCustomGameData.Exists(key);

	if (pListItem)
	{
		// Delete old copy and save new one
		if (pListItem->bManaged)
		{
			AkFree(AkMemID_Processing, pListItem->data);
		}

		if (in_pData == NULL)
		{
			m_PluginCustomGameData.Unset(key);
		}
		else
		{
			pListItem->data = in_pData;
			pListItem->size = in_Size;
			pListItem->bManaged = in_bManaged;
		}
	}
	else if (in_pData != NULL)
	{
		pListItem = m_PluginCustomGameData.Set(key);
		if (!pListItem)
		{
			if (in_bManaged)
			{
				AkFree(AkMemID_Processing, in_pData);
			}
			res = AK_InsufficientMemory;
		}
		else
		{
			pListItem->data = in_pData;
			pListItem->size = in_Size;
			pListItem->bManaged = in_bManaged;
		}
	}

	return res;
}

void AkCustomPluginDataStore::GetPluginCustomGameData(AkUniqueID in_busID, AkGameObjectID in_busObjectID, AkUInt32 in_uMergedPluginID, void* &out_rpData, AkUInt32 &out_rDataSize)
{
	AkPluginCustomGameDataListKey key = AkPluginCustomGameDataListKey(in_busID, in_busObjectID, in_uMergedPluginID);
	AkPluginCustomGameData* pListItem = m_PluginCustomGameData.Exists(key);

	if (pListItem)
	{
		out_rpData = pListItem->data;
		out_rDataSize = pListItem->size;
	}
	else
	{
		if (in_busObjectID != AK_INVALID_GAME_OBJECT)
		{
			GetPluginCustomGameData(in_busID, AK_INVALID_GAME_OBJECT, in_uMergedPluginID, out_rpData, out_rDataSize);
		}
		else
		{
			out_rpData = NULL;
			out_rDataSize = 0;
		}
	}
}

void AkCustomPluginDataStore::TermPluginCustomGameData()
{
	for (AkCustomPluginDataStore::AkPluginCustomGameDataList::Iterator it = m_PluginCustomGameData.Begin(); it != m_PluginCustomGameData.End(); ++it)
	{
		AkPluginCustomGameData& pListItem = (*it).item;
		if (pListItem.bManaged && pListItem.data)
			AkFree(AkMemID_Processing, pListItem.data);
	}
	m_PluginCustomGameData.Term();
}