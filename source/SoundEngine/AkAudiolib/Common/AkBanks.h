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
// AkBanks.h
//
//////////////////////////////////////////////////////////////////////
#ifndef AK_BANKS_H_
#define AK_BANKS_H_

#define AK_BANK_INVALID_OFFSET (AK_UINT_MAX)

// Version History:
//   1:   Original
//   2:   Added Layers to Layer Containers
//   3:   Remove unused isTransitionEnabled flag
//   4:   Removed Divergence Front/Rear
//   5:   Added Crossfading data to Layers
//   6:   - Moved interpolation type from curves to curve points (for segment-specific interpolation)
//        - Added scaling mode to RTPC curves (for optional dB scaling of volume-related curves curves)
//        - Unifying enum used to specify interpolation in curves and in fade actions
//   7:   Added curve scaling type to radius and obs/occ curves
//   8:   Support more interpolation types in curves. Note that this does not change the format
//        of the bank, only the possible values for interpolation.
//   9:   Support multiple RTPC curves on the same parameter (added curve ID to each RTPC)
//  10:   Add RTPC for environmentals (WG-4485)
//  11:	  Removed unused DeviceID for sources
//  12:   Move FX at beginning of sound (WG-4513)
//  13:   Removed UseState flag from SoundBanks (WG-4597)
//  14:   Cleanup (WG-4716)
//          - Removed bIsDopplerEnabled from AKBKPositioningInfo
//          - Removed bSendOverrideParent, Send, SendModMin and SendModMax from AKBKParameterNodeParams
//  15:   Added State synchronization type.
//  16:   Added Triggers Strings in banks and trigger actions.
//  17:   Added Interactive music bank packaging.
//		  ( all banks gets invalidated since the source content format changed ).
//  18:   Added Random/Sequence support at track level for IM
//  19:   Removed Override Parent in AkStinger
//  20:   Added new positioning information
//	21:	  Added new advanced settings information for kick newest/oldest option
//  22:   Removed old positioning info
//  23:   ---
//  24:	  Fixed Trigger action packaging
//  25:   Added Interactive music track look ahead time.
//  26:   Added Wii compressor for all busses
//  27:   MAJOR BANK REFACTORING
//	28:	  MAJOR BANK REFACTORING( Now hashing strings )	
//	29:	  Changed sources' format bits packaging in accordance to new AkAudioFormat.
//  30:	  Added Feedback Generator
//  31:	  Added Feedback flag in bank header.
//  32:   If the feedback bus isn't set, the feedback properties aren't saved anymore
//  33:   Some plug-ins have additional properties that must be packaged into banks
//	34:   Music tracks: move playlist before base node params (bug fix WG-10631)
//	35:   Changing bank version for 2008.3. (The version should have been incremented in 2008.2.1 for Wii vorbis but didn't, so better late than never).
//	36:   WG-12262 More vorbis changes
//	37:   Reverting Index and Data order in banks allowing to load on a per sound mode.
//	38:   Modified RPTC subscription format
//	39:   Introduced RTPC on Rnd/Seq Duration
//  40:	  Added Game Parameters default values.
//	41:   Changed AkPitchValue from Int32 to Real32.
//	42:   Added Bus Mas duck attenuation.
//  43:	  Added Follow Listener Orientation flag in positionning (WG-9800 3D User-Defined Positioning | Add a "Follow Listener" option.)
//  44:	  Added random range around points in 2D paths. (WG-13292 3D User-Defined Positioning | Add a randomizer on X and Y coordinates for each points)
//	45:   New XMA2 encoder (introduces with new header format structures).
//	46:   Added dialog event "Weighting" property
//  47:   Banks don't contain the source file format anymore (WG-16161)
//  48:   Seek actions (WG-10870)
//  49:   Channel Mask on Bus
//  50:   FX Sharesets
//  51:   Changes in VorbisInfo.h
//  52:   Removed ScalingDB255 interpolation table, changed Layers curve mapping.
//  53:   Removed global states.
//  54:   Added New "Adopt virtual behavior when over limit".
//  55:   SetGameParameter actions (WG-2700)
//  56:   Added Global Voice limitation system.
//  57:   Modified Weigth to go from 0.001 to 100 (was 1-100)
//  58:   Use AkPropID map in hierarchy chunk for props and ranges
//  59:   Use AkPropID map in states
//  60:   Use AkPropID map for basegenparams
//  61:   Use AkPropID map for action props and ranges
//	62:   Removed default value for RTPC on Layers.
//	63:   Removed LFE Separate Control volume.
//	64:   Interactive music markers/cues
//	65:   Interactive music clip automation
//	66:   Busses now with Hashable names.
//	67:   Platform NGP renamed to VitaSW, VitaHW added
//	68:   Auxilliary sends support.
//	69:   Layer crossfades and music clip fades curves are now linear [0-1].
//	70:   Changed table dB scaling computation.
//	71:   Added bus VoiceVolume handling.
//	72:   Migrated to BusVolume system and Adding Duck target prop.
//  73:   Added Initial Delays

//  74:   Added Bus Panning and Bus Channel config.
//  75:   HDR.
//  76:   HDR property fixes.
//  77:   HDR property changes (override analysis, envelope enable).
//	78:	  Changed HDR defaults.
//	79:	  HDR property changes.
//	80:   Added switch tracks.
//	81:   Added switch track transitions.
//  82:	  Multi switch music switch containers
//  83:   Allow music transition rules to have a list of source/dest nodes.
//  84:   Do not include music transitions in bank if there are none
//  85:   Sort the short id's in the music transitions
//  86:   Split the Wet attenuation curves into a User defined and Game defines sends.
//  86:   MIDI clip tempo
//	87:	  Package positioning RTPC.
//	88:	  Remove positioning bits.
//	89:	  Modulators.
//  90:   Looping Containers Randomizer.
//  91:   Licensing.
//  92:   RTPC Ramping.
//  93:   Branch merge to main.
//	94:   Mixer plugin.
//	95:   New MIDI settings, MIDI target moved
//	96:	  Attachable properties.
//	97:   Mixer plugin part 2.
//	98:	  Stream cache locking mechanism.
//	99:   Multiple RTPCs on plugin parameters.
//	100:  Added HPF
//	101:  Switch groups on modulators.
//	(101:  TEMP z-range in user defined positioning.)
//	102:  Device memory indicator (for XMA, etc.) in bank header.
//	103:  MIDI note tracking.
//	104:  Game parameter "bind to built-in parameter"
//	105:  Reduce bank size: combine bools
//	106:  Changed bus channel config.
//	107:  Changed channel config definitions (back->side).
//	108:  Removed MIDI event fades, added property to envelopes.
//	109:  Removed MIDI event fades.
//	110:  Changed IOSONO parameter set.
//	111:  Changed IOSONO + SCE Audio3d parameter sets.
//	112:  Changed SynthOne (added over-sampling property).
//	113:  Changed Custom Bank versions now included in banks.
//  114:  RTPC parameter id reorganization.
//	115:  Sink information included in bank (for plugins)
//	116:  Change of channel config representation in WEM headers.
//  117:  Addition of the required AudioDevice Sharesets in the Init banks.
//  118:  Add 3D-position height (Z) dimension to RTPCable parameters.
//  119:  
//  120:  Convolution reverb property changes.
//  121:  Acoustic textures property changes.
//  122:  Removing EnableWiiCompressor Parameter.
//  123:  Add state property list to state aware objects
//  124:  Add base texture frequency prop to Reflect.
//  125:  Event refactor
//  126:  States in plugins
//  127:  Add warning limit (number) for Resume and Play-from-Beginning virtual voices
//  128:  Motion objects removal
//  129:  Enable spatialization -> Spatialization mode.
//	130:  Positioning revamp.
//  131:  Time Modulator
//  132:  Add AkMotionSource
//  133:  Added music transition destination sync "last exit position" 
//  134:  Add MusicEventCueType to music track
//  135:  Media alignment
//	136:  Add Spatial Audio properties into  sound bank and authoring tool.
// This has been moved to SDK\include\AK\SoundEngine\Common\AkTypes.h, 
// to make it accessible to the SDK as well. This is useful for ensuring that
// banks match before runtime (when performing a LoadBank).
#define AK_BANK_READER_VERSION AK_SOUNDBANK_VERSION

#define AK_BANK_READER_VERSION_LAST_MEDIA_ONLY_COMPATIBLE 118 // WG-24479 bank version 103 was the last version where we modified the AkBankHeader structure.
															  // There are minor WEM incompatibilities introduced over time, but it was decided unproductive to prevent backward compatibility for these cases while it will work in most cases.
															  // We are targeting to support banks back to 2016.1.X (Version 118)
#define AK_BANK_READER_VERSION_MEDIA_ALIGNMENT_FIELD 135 // WG-42957 introduced media alignment field (reusing reserved field)

class CAkFileBase;

#pragma pack(push, 1) // pack all structures read from disk

// Could be the ID of the SoundBase object directly, maybe no need to create it.
typedef AkUInt32 AkMediaID; // ID of a specific source

namespace AkBank
{
	static const AkUInt32 BankHeaderChunkID		= AkmmioFOURCC('B', 'K', 'H', 'D');
	static const AkUInt32 BankDataIndexChunkID	= AkmmioFOURCC('D', 'I', 'D', 'X');
	static const AkUInt32 BankDataChunkID		= AkmmioFOURCC('D', 'A', 'T', 'A');
	static const AkUInt32 BankHierarchyChunkID	= AkmmioFOURCC('H', 'I', 'R', 'C');
	static const AkUInt32 BankStrMapChunkID		= AkmmioFOURCC('S', 'T', 'I', 'D');
	static const AkUInt32 BankStateMgrChunkID	= AkmmioFOURCC('S', 'T', 'M', 'G');
	static const AkUInt32 BankEnvSettingChunkID	= AkmmioFOURCC('E', 'N', 'V', 'S');
	static const AkUInt32 BankCustomPlatformName= AkmmioFOURCC('P', 'L', 'A', 'T');
	static const AkUInt32 BankInitChunkID		= AkmmioFOURCC('I', 'N', 'I', 'T');


	struct AkBankHeader
	{
		// IMPORTANT!
		//
		// Do not modify this structure, doing so would possibly cause bank backward compatibility issues with banks containing media only.
		// dwBankGeneratorVersion must ALWAYS be first even when breaking compatibility.

		AkUInt32 dwBankGeneratorVersion;
		AkUInt32 dwSoundBankID;	 // Required for in-memory banks
        AkUInt32 dwLanguageID;   // Wwise Language ID in which the bank was created ( 393239870 for SFX )
		AkUInt16 uAlignment;     // Required media memory alignment
		AkUInt16 bDeviceAllocated;//Data must be allocated from device-specific memory (exact meaning depends on platform) (Boolean, but kept at 16 bits for alignment)
		AkUInt32 dwProjectID;	 // ID of the project that generated the bank
	};

	struct AkSubchunkHeader
	{
		AkUInt32 dwTag;			// 4 character TAG
		AkUInt32 dwChunkSize;	// Size of the SubSection in bytes
	};

	// Contents of the BankDataIndexChunkID chunk
	struct MediaHeader
	{
		AkMediaID id;
		AkUInt32 uOffset;
		AkUInt32 uSize;
	};

	struct AkPathHeader
	{
		AkUniqueID	ulPathID;		// Id
		int			iNumVertices;	// How many vertices there are
	};
	//////////////////////////////////////////////////////////////
	// HIRC structures
	//////////////////////////////////////////////////////////////

	enum AKBKHircType
	{
		HIRCType_State			= 1,
		HIRCType_Sound			= 2,
		HIRCType_Action			= 3,
		HIRCType_Event			= 4,
		HIRCType_RanSeqCntr		= 5,
		HIRCType_SwitchCntr		= 6,
		HIRCType_ActorMixer		= 7,
		HIRCType_Bus			= 8,
		HIRCType_LayerCntr		= 9,
		HIRCType_Segment		= 10,
		HIRCType_Track			= 11,
		HIRCType_MusicSwitch	= 12,
		HIRCType_MusicRanSeq	= 13,
		HIRCType_Attenuation	= 14,
		HIRCType_DialogueEvent	= 15,
		HIRCType_FxShareSet		= 16,
		HIRCType_FxCustom		= 17,
		HIRCType_AuxBus			= 18,
		HIRCType_Lfo			= 19,
		HIRCType_Envelope		= 20,
		HIRCType_AudioDevice	= 21,
		HIRCType_TimeMod		= 22

		// Note: stored as 8-bit value in AKBKSubHircSection
	};

	enum AKBKStringType
	{
		StringType_None			= 0,
		StringType_Bank         = 1
	};

	struct AKBKSubHircSection
	{
		AkUInt8		eHircType;
		AkUInt32	dwSectionSize;
	};

	enum AKBKSourceType
	{
		// In order of 'desirability' : data best, streaming worst
		SourceType_Data					= 0,
		SourceType_PrefetchStreaming	= 1,
		SourceType_Streaming			= 2
	};

	struct AKBKHashHeader
	{
		AkUInt32 uiType;
		AkUInt32 uiSize;
	};

	struct AKBKPositioningBits
	{
		AKBKPositioningBits()
			:bOverrideParent(false)
			,bListenerRelativeRouting(false)
			,ePanner(AK_DirectSpeakerAssignment)
			,e3DPositionType(AK_3DPositionType_Emitter)
			,eSpatializationMode(AK_SpatializationMode_None)
			,bEnableAttenuation(true)
			,bHoldEmitterPosAndOrient(false)
			,bHoldListenerOrientation(false)
			,bIsLooping(false)
			,bEnableDiffraction(false)
		{}

		AkUInt16	bOverrideParent : 1;	
		AkUInt16	bListenerRelativeRouting : 1;

		AkUInt16	ePanner : AK_PANNER_NUM_STORAGE_BITS;
		AkUInt16	e3DPositionType : AK_POSSOURCE_NUM_STORAGE_BITS;
		AkUInt16	eSpatializationMode : AK_SPAT_NUM_STORAGE_BITS;	// 0: none, 1: position only, 2: full (position and orientation)
		AkUInt16	bEnableAttenuation : 1;			// enable attenuation
		AkUInt16	bHoldEmitterPosAndOrient : 1;			// set position continuously
		AkUInt16	bHoldListenerOrientation : 1;	// Always written, but used with automation only
		AkUInt16	bIsLooping : 1;					// Always written, but used with automation only
		AkUInt16	bEnableDiffraction : 1;
	};

}//end namespace "AkBank"

#pragma pack(pop)


//////////////////////////////////////////////////////////////////////////////////////
// Bit position values for packed booleans
//////////////////////////////////////////////////////////////////////////////////////

#define BANK_BITPOS_SOURCE_LANGUAGE				0
#define BANK_BITPOS_SOURCE_ISNONCACHABLE		3

#define BANK_BITPOS_PARAMNODEBASE_PRIORITY_OVERRIDE_PARENT			0
#define BANK_BITPOS_PARAMNODEBASE_PRIORITY_APPLY_DIST_FACTOR		1
#define BANK_BITPOS_PARAMNODEBASE_OVERRIDE_MIDI_EVENTS_BEHAVIOR		2
#define BANK_BITPOS_PARAMNODEBASE_OVERRIDE_MIDI_NOTE_TRACKING		3
#define BANK_BITPOS_PARAMNODEBASE_ENABLE_MIDI_NOTE_TRACKING			4
#define BANK_BITPOS_PARAMNODEBASE_MIDI_BREAK_LOOP_NOTE_OFF			5

#define BANK_BITPOS_POSITIONING_OVERRIDE_PARENT				0
#define BANK_BITPOS_POSITIONING_LISTENER_RELATIVE_ROUTING	(BANK_BITPOS_POSITIONING_OVERRIDE_PARENT + 1)
#define BANK_BITPOS_POSITIONING_PANNER						(BANK_BITPOS_POSITIONING_LISTENER_RELATIVE_ROUTING + 1)
#define BANK_BITPOS_POSITIONING_3D_POS_SOURCE				(BANK_BITPOS_POSITIONING_PANNER + AK_PANNER_NUM_STORAGE_BITS)

#define BANK_BITPOS_POSITIONING_3D_SPATIALIZATION_MODE		0
#define BANK_BITPOS_POSITIONING_3D_ENABLE_ATTENUATION		(BANK_BITPOS_POSITIONING_3D_SPATIALIZATION_MODE + AK_SPAT_NUM_STORAGE_BITS)
#define BANK_BITPOS_POSITIONING_3D_HOLD_EMITTER				(BANK_BITPOS_POSITIONING_3D_ENABLE_ATTENUATION + 1)
#define BANK_BITPOS_POSITIONING_3D_HOLD_LISTENER_ORIENT		(BANK_BITPOS_POSITIONING_3D_HOLD_EMITTER + 1)				
#define BANK_BITPOS_POSITIONING_3D_LOOPING					(BANK_BITPOS_POSITIONING_3D_HOLD_LISTENER_ORIENT + 1)
#define BANK_BITPOS_POSITIONING_3D_ENABLE_DIFFRACTION		(BANK_BITPOS_POSITIONING_3D_LOOPING + 1)

#define BANK_BITPOS_AUXINFO_OVERRIDE_GAME_AUX			0
#define BANK_BITPOS_AUXINFO_USE_GAME_AUX				1
#define BANK_BITPOS_AUXINFO_OVERRIDE_USER_AUX			2
#define BANK_BITPOS_AUXINFO_HAS_AUX						3
#define BANK_BITPOS_AUXINFO_OVERRIDE_REFLECTIONS		4

#define BANK_BITPOS_ADVSETTINGS_KILL_NEWEST					0
#define BANK_BITPOS_ADVSETTINGS_USE_VIRTUAL					1
#define BANK_BITPOS_ADVSETTINGS_GLOBAL_LIMIT				2
#define BANK_BITPOS_ADVSETTINGS_MAXNUMINST_IGNORE			3
#define BANK_BITPOS_ADVSETTINGS_VIRTVOICEOPT_OVERRIDE		4

#define BANK_BITPOS_BUSADVSETTINGS_KILL_NEWEST				0
#define BANK_BITPOS_BUSADVSETTINGS_USE_VIRTUAL				1
#define BANK_BITPOS_BUSADVSETTINGS_MAXNUMINST_IGNORE		2
#define BANK_BITPOS_BUSADVSETTINGS_BACKGROUND_MUSIC			3

#define BANK_BITPOS_HDR_ENVELOPE_OVERRIDE					0
#define BANK_BITPOS_HDR_ANALYSIS_OVERRIDE					1
#define BANK_BITPOS_HDR_NORMAL_LOUDNESS						2
#define BANK_BITPOS_HDR_ENVELOPE_ENABLE						3

#define BANK_BITPOS_BUSHDR_ENABLE							0
#define BANK_BITPOS_BUSHDR_RELEASE_MODE_EXP					1

#define BANK_BITPOS_RNDSEQCNTR_USING_WEIGHT					0
#define BANK_BITPOS_RNDSEQCNTR_RESET_AT_EACH_PLAY			1
#define BANK_BITPOS_RNDSEQCNTR_RESTART_BACKWARD				2
#define BANK_BITPOS_RNDSEQCNTR_CONTINUOUS					3
#define BANK_BITPOS_RNDSEQCNTR_GLOBAL						4

#define BANK_BITPOS_SWITCHITEM_FIRST_ONLY					0
#define BANK_BITPOS_SWITCHITEM_CONTINUOUS_PLAY				1

#define BANK_BITPOS_ACTION_PAUSE_PAUSEDELAYEDRESUMEACTION	0
#define BANK_BITPOS_ACTION_PAUSE_APPLYTOSTATETRANSITIONS	1
#define BANK_BITPOS_ACTION_PAUSE_APPLYTODYNAMICSEQUENCE		2

#define BANK_BITPOS_ACTION_RESUME_MASTERRESUME				0
#define BANK_BITPOS_ACTION_RESUME_APPLYTOSTATETRANSITIONS	1
#define BANK_BITPOS_ACTION_RESUME_APPLYTODYNAMICSEQUENCE	2

#define BANK_BITPOS_ACTION_STOP_APPLYTOSTATETRANSITIONS		1
#define BANK_BITPOS_ACTION_STOP_APPLYTODYNAMICSEQUENCE		2

//////////////////////////////////////////////////////////////////////////////////////
// Bit position values for packed booleans
//////////////////////////////////////////////////////////////////////////////////////

#endif
