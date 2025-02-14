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

#pragma once
#include <AK/Tools/Common/AkKeyArray.h>
#include <AK/SoundEngine/Common/AkTypes.h>


class AkCustomPluginDataStore
{
public:
	struct AkPluginCustomGameDataListKey
	{
		AkPluginCustomGameDataListKey() :
			uniqueID(0),
			gameObjectID(0),
			uSlot(0)
		{
		}

		AkPluginCustomGameDataListKey(AkUniqueID in_uniqueID, AkGameObjectID in_gameObjectID, AkUInt32 in_uSlot)
		{
			uniqueID = in_uniqueID;
			gameObjectID = in_gameObjectID;
			uSlot = in_uSlot;
		}

		bool operator==(AkPluginCustomGameDataListKey in_key)
		{
			return uniqueID == in_key.uniqueID && uSlot == in_key.uSlot && gameObjectID == in_key.gameObjectID;
		}

		AkUniqueID uniqueID;
		AkGameObjectID gameObjectID;
		AkUInt32 uSlot;
	};

	struct AkPluginCustomGameData
	{
		void* data;
		AkUInt32 size;
		bool bManaged;
	};

	typedef CAkKeyArray<AkPluginCustomGameDataListKey, AkPluginCustomGameData> AkPluginCustomGameDataList;

	static AKRESULT SetPluginCustomGameData(AkUniqueID in_busID, AkGameObjectID in_busObjectID, AkPluginType in_eType, AkUInt32 in_uCompanyID, AkUInt32 in_uPluginID, void* in_pData, AkUInt32 in_Size, bool bManaged /*<- true if sound engine should free data.*/);
	static void GetPluginCustomGameData(AkUniqueID in_busID, AkGameObjectID in_busObjectID, AkUInt32 in_uMergedPluginID, void* &out_rpData, AkUInt32 &out_rDataSize);	
	static void TermPluginCustomGameData();

private:	

	static AkPluginCustomGameDataList m_PluginCustomGameData; // Custom game data for plugins	
};