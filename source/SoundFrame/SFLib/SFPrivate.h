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

static const int kStrSize = 129; ///< Length of a Sound Frame string buffer (includes trailing zero).

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundFrame/SF.h>
#include <AK/IBytes.h>

enum SFObjectType
{
	SFType_Unknown = 0,

	SFType_Event = 1,
	SFType_SoundObject = 2,
	SFType_StateGroup = 3,
	SFType_GameObject = 4,
	SFType_SwitchGroup = 5,
	SFType_GameParameter = 6,
	SFType_Trigger = 7,
	SFType_AuxBus = 8,
	SFType_OriginalFiles = 9,
	SFType_DialogueEvent = 10,
	SFType_Argument = 11,
	SFType_ConversionSettings = 12,
	SFType_SoundBank = 13,
	// Add new types here ...
};

// For the SF_COPYDATA_REQGUID request, specify what kind of "get guid"
// request it is. See SF_COPYDATA_REQGUID for details on the request
// parameters in each case.
enum SFGetGUIDType
{
	SFGetGUIDType_Type,		// AkUniqueID uniqueness is maintained across all objects of the same type
	SFGetGUIDType_Sibling	// AkUniqueID uniqueness is maintained across siblings
};

#pragma pack(1)
struct SFNotifData
{
	long eNotif;
	long eType;
	AkUInt64 objectID;
};
#pragma pack()

// Windows message

#define WM_SFBASE           (WM_USER+0x514)

#define WM_SF_STARTUP		(WM_SFBASE+0)
#define WM_SF_SHUTDOWN		(WM_SFBASE+1)
#define WM_SF_REQEVENTS		(WM_SFBASE+2)
#define WM_SF_REQPROJ		(WM_SFBASE+3)
#define WM_SF_PLAY			(WM_SFBASE+4)
#define WM_SF_STOP			(WM_SFBASE+5)
#define WM_SF_CLIENTVERSION	(WM_SFBASE+6)
#define WM_SF_REQSTATES     (WM_SFBASE+7)
#define WM_SF_REQGAMEOBJECTS (WM_SFBASE+8)
#define WM_SF_REQSWITCHES	(WM_SFBASE+9)
#define WM_SF_REQGAMEPARAMETERS	(WM_SFBASE+10)
#define WM_SF_REQTRIGGERS	(WM_SFBASE+11)
#define WM_SF_REQAUXBUS	(WM_SFBASE+12)
#define WM_SF_REQDIALOGUEEVENTS	(WM_SFBASE+13)
#define WM_SF_REQARGUMENTS	(WM_SFBASE+14)
#define WM_SF_REQCONVERSIONSETTINGS	(WM_SFBASE+15)
#define WM_SF_REQSOUNDBANKS	(WM_SFBASE+16)

// COPYDATASTRUCT.dwData values:

#define SF_COPYDATA_BASE		0x10

//#define SF_COPYDATA_EVENTLIST	(SF_COPYDATA_BASE+0)	// Do not use anymore
#define SF_COPYDATA_PLAYEVENTS	(SF_COPYDATA_BASE+1)
//#define SF_COPYDATA_REQEVENTS	(SF_COPYDATA_BASE+2)	// Do not use anymore
#define SF_COPYDATA_NOTIF		(SF_COPYDATA_BASE+3)
#define SF_COPYDATA_PROJ		(SF_COPYDATA_BASE+4)
//#define SF_COPYDATA_PROJNOTIF	(SF_COPYDATA_BASE+5)	// Do not use anymore
//#define SF_COPYDATA_REQSOUNDS	(SF_COPYDATA_BASE+6)	// Do not use anymore
//#define SF_COPYDATA_SOUNDLIST	(SF_COPYDATA_BASE+7)	// Do not use anymore
#define SF_COPYDATA_PLAYEVENTSBYSTRING	(SF_COPYDATA_BASE+8)
#define SF_COPYDATA_CONVERTEXTERNALSOURCES (SF_COPYDATA_BASE+9)
#define SF_COPYDATA_PROCESSDEFINITIONFILES (SF_COPYDATA_BASE+10)
#define SF_COPYDATA_GENERATESOUNDBANKS (SF_COPYDATA_BASE+11)
#define SF_COPYDATA_LISTENRADIUSFORSOUNDS (SF_COPYDATA_BASE+12)
#define SF_COPYDATA_OBJECTLIST	(SF_COPYDATA_BASE+13)
#define SF_COPYDATA_REQOBJECTS	(SF_COPYDATA_BASE+14)
#define SF_COPYDATA_REQOBJECTSBYSTRING (SF_COPYDATA_BASE+15)
#define SF_COPYDATA_REQCURRENTSTATE (SF_COPYDATA_BASE+16)
#define SF_COPYDATA_CURRENTSTATE (SF_COPYDATA_BASE+17)
#define SF_COPYDATA_SETCURRENTSTATE (SF_COPYDATA_BASE+18)
#define SF_COPYDATA_REGISTERGAMEOBJECT (SF_COPYDATA_BASE+19)
#define SF_COPYDATA_UNREGISTERGAMEOBJECT (SF_COPYDATA_BASE+20)
#define SF_COPYDATA_REQCURRENTSWITCH (SF_COPYDATA_BASE+21)
#define SF_COPYDATA_CURRENTSWITCH (SF_COPYDATA_BASE+22)
#define SF_COPYDATA_SETCURRENTSWITCH (SF_COPYDATA_BASE+23)
#define SF_COPYDATA_SETRTPCVALUE (SF_COPYDATA_BASE+24)
#define SF_COPYDATA_TRIGGER (SF_COPYDATA_BASE+25)
#define SF_COPYDATA_SETACTIVELISTENER (SF_COPYDATA_BASE+26)
#define SF_COPYDATA_SETPOSITION (SF_COPYDATA_BASE+27)
#define SF_COPYDATA_SETLISTENERPOSITION (SF_COPYDATA_BASE+28)
#define SF_COPYDATA_SETLISTENERSPATIALIZATION (SF_COPYDATA_BASE+29)
#define SF_COPYDATA_SETGAMEOBJECTENVIRONMENTSVALUES (SF_COPYDATA_BASE+30)
#define SF_COPYDATA_SETGAMEOBJECTDRYLEVELVALUE (SF_COPYDATA_BASE+31)
//#define SF_COPYDATA_SETENVIRONMENTVOLUME (SF_COPYDATA_BASE+32)
//#define SF_COPYDATA_BYPASSENVIRONMENT (SF_COPYDATA_BASE+33)
#define SF_COPYDATA_SETOBJECTOBSTRUCTIONANDOCCLUSION (SF_COPYDATA_BASE+34)
#define SF_COPYDATA_POSTMONITORINGMESSAGE (SF_COPYDATA_BASE+35)
#define SF_COPYDATA_STOPALL (SF_COPYDATA_BASE+36)
#define SF_COPYDATA_STOPPLAYINGID (SF_COPYDATA_BASE+37)
//#define SF_COPYDATA_SETENVIRONMENTVOLUMES (SF_COPYDATA_BASE+38)
#define SF_COPYDATA_EXECUTEACTIONONEVENT (SF_COPYDATA_BASE+39)
#define SF_COPYDATA_SETATTENUATIONSCALINGFACTOR (SF_COPYDATA_BASE+40)
#define SF_COPYDATA_SETLISTENERSCALINGFACTOR (SF_COPYDATA_BASE+41)
#define SF_COPYDATA_SETMULTIPLEPOSITIONS (SF_COPYDATA_BASE+42)

#define SF_COPYDATA_REQAKUNIQUEID (SF_COPYDATA_BASE+43)
#define SF_COPYDATA_AKUNIQUEID (SF_COPYDATA_BASE+44)

#define SF_COPYDATA_SEEKONEVENT_MS (SF_COPYDATA_BASE+45)
#define SF_COPYDATA_SEEKONEVENT_PCT (SF_COPYDATA_BASE+46)

#define SF_COPYDATA_REQORIGINALFROMEVENT (SF_COPYDATA_BASE+47)
#define SF_COPYDATA_ORIGINALLIST (SF_COPYDATA_BASE+48)
#define SF_COPYDATA_REQORIGINALFROMDIALOGUEEVENT (SF_COPYDATA_BASE+49)

#define SF_COPYDATA_OBJECTPATH (SF_COPYDATA_BASE+50)

#define SF_COPYDATA_REQEVENTHASVOICECONTENT (SF_COPYDATA_BASE+51)
#define SF_COPYDATA_REQDIALOGUEEVENTHASVOICECONTENT (SF_COPYDATA_BASE+52)
#define SF_COPYDATA_HASVOICECONTENT (SF_COPYDATA_BASE+53)

#define SF_COPYDATA_RESETRTPCVALUE (SF_COPYDATA_BASE+54)

#define SF_COPYDATA_CONVERTEXTERNALSOURCESRESULT (SF_COPYDATA_BASE+55)
#define SF_COPYDATA_GENERATESOUNDBANKSRESULT (SF_COPYDATA_BASE+56)

#define SF_COPYDATA_SHOWOBJECTS	(SF_COPYDATA_BASE+57)

#define SF_COPYDATA_GETSOUNDBANKGENERATIONSETTINGS (SF_COPYDATA_BASE+58)
#define SF_COPYDATA_SOUNDBANKGENERATIONSETTINGS (SF_COPYDATA_BASE+59)

#define SF_CLIENTWINDOWNAME _T("WWISECLIENT")
#define SF_SERVERWINDOWNAME _T("WWISESERVER")

// Persistence versions: ( always modify SF_SOUNDFRAME_*_VERSION_CURRENT to point to the latest one ! ) 

#define SF_SOUNDFRAME_VERSION_FIRST				0x01
#define SF_SOUNDFRAME_VERSION_AUXBUS			0x02
#define SF_SOUNDFRAME_VERSION_RESETRTPC			0x03
#define SF_SOUNDFRAME_VERSION_GENERATIONSETTINGS 0x04
#define SF_SOUNDFRAME_VERSION_CURRENT			SF_SOUNDFRAME_VERSION_GENERATIONSETTINGS

namespace AK
{
	namespace SoundFrame
	{
		inline void SoundBankGenerationSettings_Serialize(SoundBankGenerationSettings * in_pSettings, AK::IWriteBytes * in_pBytes)
		{
			in_pBytes->Write<bool>(in_pSettings->bUseReferenceLanguageAsStandIn);
			in_pBytes->Write<bool>(in_pSettings->bAllowExceedingSize);
			in_pBytes->Write<bool>(in_pSettings->bUseSoundbankNames);
			in_pBytes->Write<bool>(in_pSettings->bGenerateHeaderFile);

			in_pBytes->Write<bool>(in_pSettings->bGenerateContentTxtFile);
			in_pBytes->Write<int>(in_pSettings->eContentTxtFileFormat);

			in_pBytes->Write<bool>(in_pSettings->bGenerateXMLMetadata);
			in_pBytes->Write<bool>(in_pSettings->bGenerateJSONMetadata);
			in_pBytes->Write<bool>(in_pSettings->bGenerateMetadataFile);
			in_pBytes->Write<bool>(in_pSettings->bGeneratePerBankMetadataFile);

			in_pBytes->Write<bool>(in_pSettings->bMedatadaIncludeGUID);
			in_pBytes->Write<bool>(in_pSettings->bMedatadaIncludeMaxAttenuation);
			in_pBytes->Write<bool>(in_pSettings->bMedatadaIncludeEstimatedDuration);
		}

		inline void SoundBankGenerationSettings_Deserialize(SoundBankGenerationSettings * in_pSettings, AK::IReadBytes * in_pBytes)
		{
			in_pSettings->bUseReferenceLanguageAsStandIn = in_pBytes->Read<bool>();
			in_pSettings->bAllowExceedingSize = in_pBytes->Read<bool>();
			in_pSettings->bUseSoundbankNames = in_pBytes->Read<bool>();
			in_pSettings->bGenerateHeaderFile = in_pBytes->Read<bool>();

			in_pSettings->bGenerateContentTxtFile = in_pBytes->Read<bool>();
			in_pSettings->eContentTxtFileFormat = (SoundBankContentTxtFileFormat)in_pBytes->Read<int>();

			in_pSettings->bGenerateXMLMetadata = in_pBytes->Read<bool>();
			in_pSettings->bGenerateJSONMetadata = in_pBytes->Read<bool>();
			in_pSettings->bGenerateMetadataFile = in_pBytes->Read<bool>();
			in_pSettings->bGeneratePerBankMetadataFile = in_pBytes->Read<bool>();

			in_pSettings->bMedatadaIncludeGUID = in_pBytes->Read<bool>();
			in_pSettings->bMedatadaIncludeMaxAttenuation = in_pBytes->Read<bool>();
			in_pSettings->bMedatadaIncludeEstimatedDuration = in_pBytes->Read<bool>();
		}
	}
}
