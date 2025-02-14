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
// AkBankStructs.h
//
//////////////////////////////////////////////////////////////////////
#ifndef AK_BANK_STRUCTS_H_
#define AK_BANK_STRUCTS_H_

#include "AkModulatorProps.h"
#include "AkVirtualAcousticsProps.h"
#include "AkRanSeqCntr.h"
#include "AkSwitchCntr.h"
#pragma warning(push)
#pragma warning(disable:4530)
#include <vector>
#pragma warning(pop)
#include "AkBanks.h"
#include "AkPath.h"
#include "AkDecisionTree.h"
#include "IWAttenuation.h"
#include "IWProject.h"
#include "AkTransition.h"
#include "AKBase/waSortedVectorMap.h"

namespace AkBank
{
	struct AKBKStateItem
	{
		AkUInt32 State;	// Type, sunny or something else
		AkUInt32 ID;	// Unique ID of the state

		AKBKStateItem()
			:State( AK_INVALID_UNIQUE_ID )
			,ID( AK_INVALID_UNIQUE_ID )
		{}
	};

	struct AKBKStateGroupItem
	{
		AkUInt32	ulStateGroup;
		AkUInt32	eStateSyncType;
		std::vector<AKBKStateItem>	StateList;

		AKBKStateGroupItem()
            : ulStateGroup(AK_INVALID_UNIQUE_ID)
			,eStateSyncType( 0 )
		{
		}
	};

	struct AKBKStateProperty
	{
		AKBKStateProperty() = default;
		AKBKStateProperty(bool in_inDb, AkUInt16 in_propertyId, AkRtpcAccum in_rtpcAccum, AkReal32 in_paramValue)
		: inDb(in_inDb)
		, propertyId(in_propertyId)
		, rtpcAccum(in_rtpcAccum)
		, paramValue(in_paramValue)
		{}

		bool		inDb;
		AkUInt16	propertyId;
		AkRtpcAccum	rtpcAccum;
		AkReal32	paramValue;
	};

	struct AKBKFxParamValue
	{
		AkRtpcAccum	rtpcAccum;
		AkReal32	paramValue;

		AKBKFxParamValue()
			: rtpcAccum( AkRtpcAccum_None )
			, paramValue( 0.f )
		{}
	};

	struct AKBKSource
	{
		AkUInt32	TypeID; 		// Typically zero if not a plug in
		AkUInt32	SourceID;
		AkUInt32	uInMemoryMediaSize;
		AkUInt32	SourceParamsSize;
		AkUInt8*	pSourceParams;
		AkUInt8		StreamingType; 	// In-mem/prefetch/streamed
		AkUInt8		bIsLanguageSpecific;
		AkUInt8		bIsNonCachable;

		AKBKSource()
			:TypeID(0)
			,SourceID( AK_INVALID_UNIQUE_ID )
			,uInMemoryMediaSize( 0 )
			,SourceParamsSize(0)
			,pSourceParams(NULL)
			,StreamingType( SourceType_Data )
			,bIsLanguageSpecific( 0 )
			,bIsNonCachable(0)
		{
		}
	};

	struct AKBKSourceToIndex
	{
		AKBKSourceToIndex()
			:index(0)
			,sourceID(AK_INVALID_SOURCE_ID)
		{}
		AkUInt8 index;
		AkUInt32 sourceID;
	};

	struct AKBKMixer
	{
		AKBKMixer(): FXID(0),
					 bIsShareSet(false) {};
		AkUInt32 FXID;
		AkUInt8 bIsShareSet;
	};

	struct AKBKFX
	{
		AkUInt32 FXID;
		AkUInt8 bIsShareSet;
		AkUInt8	bIsBypassed;
		AkUInt8	bIsRendered;
		AkUInt8 ucIndex;

	//Constructor
		AKBKFX()
			:FXID(AK_INVALID_UNIQUE_ID)
			,bIsBypassed(false)
			,bIsRendered(false)
			,ucIndex(0)
		{}
	};

	struct AKBKRTPC
	{
		AkUInt32 RTPCID;
		AkRtpcType RTPCType;
		AkRtpcAccum RTPCAccum;
		AkRTPC_ParameterID ParamID;
		AkUInt32 RTPCCurveID;
		AkCurveScaling Scaling;
		AkReal32 fInitValue;
		std::vector<AkRTPCGraphPoint> ConversionTable;
		AKBKRTPC()
			:RTPCID(0)
			,RTPCType(AkRtpcType_GameParameter)
			,RTPCAccum(AkRtpcAccum_Exclusive)
			,ParamID(RTPC_Volume)
			,RTPCCurveID(0)
			,Scaling(AkCurveScaling_None)
			,fInitValue(0.f)
		{}
	};

	struct AKBKFXParamSet
	{
		AkUniqueID	ParamSetID;
		AkUInt32	FXID;
		std::vector<AKBKSourceToIndex>		dataSourcesID;
		std::vector<AKBKRTPC>				RTPCList;
		std::vector<AKBKStateProperty>		StatePropertyList;
		std::vector<AKBKStateGroupItem>		StateGroupList;
		std::map<AkPluginParamID,AkBank::AKBKFxParamValue>	ParamValueMap;

		AKBKFXParamSet()
			:ParamSetID(AK_INVALID_UNIQUE_ID)
			,FXID(AK_INVALID_PLUGINID)
			,FXPresetSize(0)
			,parrayFXPreset(NULL)
		{}

		AKBKFXParamSet( const AKBKFXParamSet& in_rhs )
			: dataSourcesID( in_rhs.dataSourcesID )
			, RTPCList( in_rhs.RTPCList )
			, StatePropertyList( in_rhs.StatePropertyList )
			, StateGroupList( in_rhs.StateGroupList )
			, ParamValueMap( in_rhs.ParamValueMap )
		{
			FXID = in_rhs.FXID;
			FXPresetSize = in_rhs.FXPresetSize;
			parrayFXPreset = NULL;

			SetFXPreset( in_rhs.parrayFXPreset, in_rhs.FXPresetSize );
			dataSourcesID = in_rhs.dataSourcesID;
		}

		~AKBKFXParamSet()
		{
			if ( parrayFXPreset )
			{
				free( parrayFXPreset );
			}
		}

		void SetFXPreset( AkUInt8* in_pData, AkUInt32 in_ulSize )
		{
			if ( in_ulSize && in_pData )
			{
				parrayFXPreset = (AkUInt8*)malloc( in_ulSize );
				AKPLATFORM::AkMemCpy( parrayFXPreset, in_pData, in_ulSize );
				FXPresetSize = in_ulSize;
			}
			else if ( parrayFXPreset )
			{
				free ( parrayFXPreset );
				parrayFXPreset = NULL;
				FXPresetSize = 0;
			}
		}
		AkUInt32 GetFXPresetSize() const { return FXPresetSize; }
		AkUInt8* GetFXPresetData() const { return parrayFXPreset; }

	private:
		AkUInt32	FXPresetSize;
		AkUInt8*	parrayFXPreset;
	};

	struct AKBKState
	{
		AkUniqueID	ID;
		wa::SortedVectorMap<AkStatePropertyId, AkPropValue> props;

		AKBKState() : ID( AK_INVALID_UNIQUE_ID ) {}
	};

	struct AKBKPath
	{
		std::vector<AkPathVertex>		listVertexes;
		AkReal32						fXRange;
		AkReal32						fYRange;
		AkReal32						fZRange;
		AKBKPath()
			: fXRange(0.f)
			, fYRange(0.f)
			, fZRange(0.f)
		{}
	};

	struct AKBKPathPlaylistItem
	{
		AkUInt32 ulOffset;		//in vertices
		AkUInt32 ulNumVertices;
		AKBKPathPlaylistItem()
			:ulOffset(0)
			,ulNumVertices(0)
		{}
	};

	struct AKBKAttenuation
	{
		AkUniqueID			ID;

		// Cone params
		AkUInt8				bIsConeEnabled;
		AkReal32			cone_fInsideAngle;
		AkReal32			cone_fOutsideAngle;
		AkReal32			cone_fOutsideVolume;
		AkLPFType			cone_LoPass;
		AkLPFType			cone_HiPass;

		IWAttenuation::CurveToUse		aCurveToUse[IWAttenuation::Curve_Max];
		AkCurveScaling					aScalingCurve[IWAttenuation::Curve_Max];
		std::vector<AkRTPCGraphPoint>	Curves[IWAttenuation::Curve_Max];
		std::vector<AKBKRTPC>			RTPCList;

		AKBKAttenuation()
			:ID(0)
			,bIsConeEnabled(false)
			,cone_fInsideAngle(0)
			,cone_fOutsideAngle(0)
			,cone_fOutsideVolume(0)
			,cone_LoPass(0)
			,cone_HiPass(0)
		{}
	};

	struct AKBKAuxInfo
	{
		AkUInt8	bOverrideGameAuxSends;
		AkUInt8 bUseGameAuxSends;
		AkUInt8 bOverrideUserAuxSends;
		AkUInt8 bOverrideEarlyReflections;
		AkUniqueID aAuxBusses[AK_NUM_USER_AUX_SEND_PER_OBJ];
		AkUniqueID reflectionsAuxBus;

		AKBKAuxInfo()
			:bOverrideGameAuxSends(false)
			,bUseGameAuxSends(false)
			,bOverrideUserAuxSends(false)
			,bOverrideEarlyReflections(false)
		{
			for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
			{
				aAuxBusses[i] = AK_INVALID_UNIQUE_ID;
			}

			reflectionsAuxBus = AK_INVALID_UNIQUE_ID;
		}
	};

	struct AKBKPositioningInfo : public AkBank::AKBKPositioningBits
	{
		//3D (user-defined)
		AkPathMode	ePathMode;

		AkTimeMs	transitionTime;

		std::vector<AKBKPath>	listPath;

		AKBKPositioningInfo()
			:ePathMode(AkContinuousSequence)
			,transitionTime(0)
		{}
	};

	struct AKBKPropAndRange
	{
		wa::SortedVectorMap<AkUInt8, AkPropValue> props;
		wa::SortedVectorMap<AkUInt8, RANGED_MODIFIERS<AkPropValue>> ranges;
	};

	struct AKBKParameterNodeBase
		: public AKBKPropAndRange
	{
		AKBKParameterNodeBase()
			:bOverrideAttachableMixerInput( false )
		{}

		std::vector<AKBKFX>				FXList;
		std::vector<AKBKRTPC>			RTPCList;
		std::vector<AKBKStateProperty>	StatePropertyList;
		std::vector<AKBKStateGroupItem>	StateGroupList;

		AkUInt8					bOverrideAttachableMixerInput;
		AKBKPositioningInfo		Positioning;

		AKBKAuxInfo				Aux;
	};

	struct AKBKParameterNode
		: public AKBKParameterNodeBase
	{
		AkUInt32				ulParentBusID; //0 if using parent one
		AkUInt32				ulDirectParentID;

		AkUInt8					bPriorityOverrideParent;
		AkUInt8					bPriorityApplyDistFactor;

		AkUInt32				eVirtualQueueBehavior;
		AkUInt8					bKillNewest;
		AkUInt8					bUseVirtualBehavior;
		AkUInt16				MaxNumInstance;// Zero being no max.
		AkUInt8					bIsGlobalLimit;
		AkUInt32				eBelowThresholdBehavior;
		AkUInt8					bIsMaxNumInstOverrideParent;
		AkUInt8					bIsVVoiceOptOverrideParent;
		AkUInt8					bOverrideMidiEventsBehavior;
		AkUInt8					bOverrideMidiNoteTracking;
		AkUInt8					bEnableMidiNoteTracking;
		AkUInt8					bMidiBreakOnNoteOff;

		AkUInt8					bOverrideHdrEnvelope;
		AkUInt8					bOverrideAnalysis;
		AkUInt8					bNormalizeLoudness;
		AkUInt8					bEnableEnvelope;

		AkUInt8						bIsFXOverrideParent;
		AkUInt8						bBypassAllFX;

	//Constructor
		AKBKParameterNode()
            : ulParentBusID( AK_INVALID_UNIQUE_ID )
			,ulDirectParentID( AK_INVALID_UNIQUE_ID )
			,bPriorityOverrideParent( false )
			,bPriorityApplyDistFactor( false )
			,eVirtualQueueBehavior( AkVirtualQueueBehavior_FromBeginning )
			,bKillNewest( false )
			,bUseVirtualBehavior( false )
			,MaxNumInstance( 0 )// Zero being no max.
			,bIsGlobalLimit( false )
			,eBelowThresholdBehavior( AkBelowThresholdBehavior_ContinueToPlay )
			,bIsMaxNumInstOverrideParent( false )
			,bIsVVoiceOptOverrideParent( false )
			,bOverrideMidiEventsBehavior( false )
			,bOverrideMidiNoteTracking( false )
			,bEnableMidiNoteTracking( false )
			,bMidiBreakOnNoteOff( false )
			,bOverrideHdrEnvelope( false )
			,bOverrideAnalysis( false )
			,bNormalizeLoudness( false )
			,bEnableEnvelope( false )
			,bIsFXOverrideParent( false )
			,bBypassAllFX( false )
		{}
	};

	struct AKBKSound
	{
		AkUniqueID			ID;
		AKBKSource			SourceInfo;
		AKBKParameterNode 	ParamNode;
	//Constructor
		AKBKSound()
			:ID(0)
		{}
	};

	struct AKBKTrackSrcInfo
	{
		AkUInt32	trackID;
		AkUInt32  	sourceID;			// ID of the source
		AkUInt32	eventID;			// ID of the event
		AkReal64    fPlayAt;            // Play At (ms).
		AkReal64    fBeginTrimOffset;   // Begin Trim offset (ms).
		AkReal64    fEndTrimOffset;     // End Trim offset (ms).
		AkReal64    fSrcDuration;       // Duration (ms).

		AKBKTrackSrcInfo()
			:trackID( AK_INVALID_UNIQUE_ID )
			,sourceID( AK_INVALID_UNIQUE_ID )
			,eventID( AK_INVALID_UNIQUE_ID )
			,fPlayAt( 0 )
			,fBeginTrimOffset( 0 )
			,fEndTrimOffset( 0 )
			,fSrcDuration( 0 )
		{}
	};

	struct AKBKClipAutomation
	{
		AkUInt32				uClipIndex;
		AkUInt32				eAutoType;
		std::vector<AkRTPCGraphPoint> AutomationData;
	};

	struct AKBKMusicFade
	{
		AKBKMusicFade()
			: iTransitionTime( 0 )
			, eFadeCurve( AkCurveInterpolation_Linear )
			, iFadeOffset( 0 )

		{}
		AkInt32		iTransitionTime;	// how long this should take (in ms)
		AkUInt32	eFadeCurve;			// (AkCurveInterpolation)what shape it should have
		AkInt32		iFadeOffset;		// Fade offset.
	};

	struct AKBKMusicTrackTransitionRule
	{
		// Important to add all new members in the constructor.
		// If unused values are not set to a common correct value,
		AKBKMusicTrackTransitionRule()
			: eSrcSyncType( 0 )
			, uSrcCueFilterHash( 0 )
		{}

		AKBKMusicFade	srcFade;
		AKBKMusicFade	destFade;
		AkUInt32		eSrcSyncType;
		AkUInt32		uSrcCueFilterHash;
	};

	struct AKBKMusicFlags
	{
		AKBKMusicFlags()
			: uOverrideClockSettings(0)
			, uOverrideMidiTempo(0)
			, uOverrideMidiTarget(0)
			, uMidiTargetTypeBus(0)
		{}

		AkUInt8	uOverrideClockSettings:1;
		AkUInt8	uOverrideMidiTempo:1;
		AkUInt8	uOverrideMidiTarget:1;
		AkUInt8	uMidiTargetTypeBus:1;
	};

	struct AKBKMusicTrack
	{
		AkUniqueID						ID;
		AKBKMusicFlags					Flags;
		std::vector<AKBKSource>			Sources;
		std::vector<AKBKTrackSrcInfo>	PlayList;
		AkUInt32						uNumSubTrack;
		std::vector<AKBKClipAutomation> ClipAutomation;
		AKBKParameterNode 				ParamNode;
		AkUInt8							eTrackType;
		AkUInt8							eGroupType;
		AkUniqueID						idSwitchGroup;
		AkUniqueID						idDefaultSwitch;
		std::vector<AkUniqueID>			SwitchAssoc;
		AKBKMusicTrackTransitionRule	TransRule;
		AkTimeMs						iLookAheadTime;
	//Constructor
		AKBKMusicTrack()
			:ID( AK_INVALID_UNIQUE_ID )
			,iLookAheadTime( 0 )
		{}
	};

	struct AKBKPlaylistItem
	{
		AkUniqueID		ID;			// Unique ID of the PlaylistItem
		AkUInt32		weight;		// Weight(only for random containers, field unused in sequence mode)
	//Constructor
		AKBKPlaylistItem()
			:ID(0)
			,weight(DEFAULT_RANDOM_WEIGHT)
		{}
	};

	struct AKBKRanSeqParams
	{
		AkReal32	TransitionTime;
		AkReal32	TransitionTimeModMin;
		AkReal32	TransitionTimeModMax;

		AkUInt16	wAvoidRepeatCount;

		AkUInt8	eTransitionMode;
		AkUInt8	eRandomMode;
		AkUInt8	eMode;
		AkUInt8	bIsUsingWeight;
		AkUInt8	bResetPlayListAtEachPlay;
		AkUInt8	bIsRestartBackward;
		AkUInt8	bIsContinuous;
		AkUInt8	bIsGlobal;
	//Constructor
		AKBKRanSeqParams()
			:TransitionTime(0.0f)
			,TransitionTimeModMin(0.0f)
			,TransitionTimeModMax(0.0f)
			,wAvoidRepeatCount(0)
			,eTransitionMode(Transition_Disabled)
			,eRandomMode(RandomMode_Normal)
			,eMode(ContainerMode_Random)
			,bIsUsingWeight(false)
			,bResetPlayListAtEachPlay(false)
			,bIsRestartBackward(false)
			,bIsContinuous(false)
			,bIsGlobal(true)
		{}
	};

	struct AKBKRanSeqCntr
	{
		AkUniqueID				ID;
		AKBKParameterNode 		ParamNode;
		AkUInt16				LoopCount;
		AkUInt16				LoopCountRangeMin;
		AkUInt16				LoopCountRangeMax;
		AKBKRanSeqParams		ParamRanSeqCntr;
		std::vector<AkUniqueID>		ChildList;
		std::vector<AKBKPlaylistItem>	PlayList;
	//Constructor
		AKBKRanSeqCntr()
			:ID(AK_INVALID_UNIQUE_ID)
			,LoopCount( AkLoopVal_NotLooping )
			,LoopCountRangeMin( 0 )
			,LoopCountRangeMax( 0 )
		{}
	};

	struct AKBKSwitchParams
	{
		AkUInt8			eGroupType;				// Is it binded to state or to switches, 0 = switch, 1 = state, internally defined in enum "AkGroupType"
		AkUInt32		ulGroupID;				// May either be a state group or a switch group
		AkUInt32		ulDefaultSwitch;		// Default value, to be used if none is available,
												// it may be none, in which case we must play nothing...
		AkUInt8		bIsContinuousValidation;	// Is the validation continuous
	//Constructor
		AKBKSwitchParams()
			:eGroupType( AkGroupType_Switch )
			,ulGroupID( 0 )
			,ulDefaultSwitch( 0 )
			,bIsContinuousValidation( false )
		{}
	};

	struct AKBKSwitchItemParam
	{
		AkUniqueID		ID;
		AkUInt8			bIsFirstOnly;
		AkUInt8			bIsContinuousPlay;
		AkUInt8			eOnSwitchMode;
		AkTimeMs		FadeOutTime;
		AkTimeMs		FadeInTime;

	//Constructor
		AKBKSwitchItemParam()
			:ID( 0 )
			,bIsFirstOnly( false )
			,bIsContinuousPlay( false )
			,eOnSwitchMode( AkOnSwitchMode_PlayToEnd )
			,FadeOutTime( 0 )
			,FadeInTime( 0 )
		{}
	};

	struct AKBKSwitchGroup
	{
		AkUInt32 ulSwitchID;
		std::vector<AkUniqueID>		SwitchItemList;
	//Constructor
		AKBKSwitchGroup()
			:ulSwitchID(0)
		{}
	};

	struct AKBKSwitchCntr
	{
		AkUniqueID					ID;
		AKBKParameterNode 			ParamNode;
		AKBKSwitchParams			ParamSwitchCntr;
		std::vector<AkUniqueID>			ChildList;
		std::vector<AKBKSwitchGroup>		SwitchGroupList;
		std::vector<AKBKSwitchItemParam>	SwitchParamList;
	//Constructor
		AKBKSwitchCntr()
			:ID(AK_INVALID_UNIQUE_ID)
		{}
	};

	struct AKBKMeterInfo
	{
		AkReal64    fGridPeriod;        // Grid period (1/frequency) (ms).
		AkReal64    fGridOffset;        // Grid offset (ms).
		AkReal32    fTempo;             // Tempo: Number of Quarter Notes per minute.
		AkUInt8     uTimeSigNumBeatsBar;// Time signature numerator.
		AkUInt8     uTimeSigBeatValue;  // Time signature denominator.
		AkUInt8		bOverrideParent;
	};

	struct AKBKStinger
	{
		AkUInt32	m_TriggerID;
		AkUInt32	m_SegmentID;
		AkUInt32	m_SyncPlayAt;		//AkSyncType
		AkUInt32	m_uCueFilterHash;
		AkInt32		m_DontRepeatTime;	//ms(AkTimeMs)

		AkUInt32	m_numSegmentLookAhead;
	};

	struct AKBKMusicNode
	{
		AKBKMusicFlags				Flags;
		AkUniqueID					ID;
		AKBKParameterNode 			ParamNode;
		std::vector<AkUniqueID>		ChildList;
		AKBKMeterInfo				meterInfo;
		std::vector<AKBKStinger>	stingers;

	//Constructor
		AKBKMusicNode()
			:ID( AK_INVALID_UNIQUE_ID )
		{}
	};

	struct AKBKMusicTransitionRule
	{
		// Important to add all new members in the constructor.
		// If unused values are not set to a common correct value,
		AKBKMusicTransitionRule()
			: eSrcSyncType( 0 )
			, uSrcCueFilterHash( 0 )
			, bSrcPlayPostExit( 0 )
			, uDestCueFilterHash( 0 )
			, uDestJumpToID( 0 )
			, eDestJumpToType( 0 )
			, eDestEntryType( 0 )
			, bDestPlayPreEntry( 0 )
			, bDestMatchSourceCueName( 0 )
			, bIsTransObjectEnabled( 0 )
			, segmentID( 0 )
			, bPlayPreEntry( 0 )
			, bPlayPostExit( 0 )
		{}

		std::vector<AkUInt32> srcID;    // Source (departure) node ID.
		std::vector<AkUInt32> destID;   // Destination (arrival) node ID.

		AKBKMusicFade	srcFade;
		AkUInt32		eSrcSyncType;
		AkUInt32		uSrcCueFilterHash;
		AkUInt8			bSrcPlayPostExit;

		AKBKMusicFade	destFade;
		AkUInt32		uDestCueFilterHash;
		AkUInt32		uDestJumpToID;		// JumpTo ID (applies to Sequence Containers only).
		AkUInt16		eDestJumpToType;	// Jump to type.
		AkUInt16		eDestEntryType;		// Entry type.
		AkUInt8			bDestPlayPreEntry;
		AkUInt8			bDestMatchSourceCueName;

		AkUInt8			bIsTransObjectEnabled;
		AkUInt32		segmentID;			// Node ID. Can only be a segment.
		AKBKMusicFade	transFadeIn;
		AKBKMusicFade	transFadeOut;
		AkUInt8			bPlayPreEntry;
		AkUInt8			bPlayPostExit;
	};

	struct AKBKMusicTransNode
	{
		AKBKMusicNode musicNode;
		std::vector<AKBKMusicTransitionRule> transRules;
	};

	struct AKBKMusicMarker
	{
		AKBKMusicMarker()
		: markerId( 0 )
		, fPosition( 0 )
		, pszName( NULL ) {}
		~AKBKMusicMarker()
		{
			if ( pszName )
				delete [] pszName;
		}
		// Copy / = implement deep copy.
		AKBKMusicMarker( const AKBKMusicMarker & in_copy )
		{
			markerId = in_copy.markerId;
			fPosition = in_copy.fPosition;
			if ( in_copy.pszName )
			{
				pszName = new char[strlen(in_copy.pszName)+1];
				strcpy( pszName, in_copy.pszName );
			}
			else
				pszName = NULL;
		}
		AKBKMusicMarker & operator=( const AKBKMusicMarker & in_copy )
		{
			if ( &in_copy != this )
			{
				markerId = in_copy.markerId;
				fPosition = in_copy.fPosition;
				if ( in_copy.pszName )
				{
					pszName = new char[strlen(in_copy.pszName)+1];
					strcpy( pszName, in_copy.pszName );
				}
				else
					pszName = NULL;
			}
			return *this;
		}

		AkUInt32	markerId;
		AkReal64	fPosition;
		char *		pszName;	// UTF-8 cue name.
	};

	struct AKBKMusicSegment
	{
		AKBKMusicNode musicNode;
		AkReal64 duration;
		std::vector<AKBKMusicMarker> markers;
	//Constructor
		AKBKMusicSegment()
			:duration( 0 )
		{}
	};

	struct AKBKMusicRanSeqPlaylistItem
	{
		AkUInt32 m_SegmentID;
		AkUInt32 m_playlistItemID;

		AkUInt32 m_NumChildren;
		AkUInt32 m_eRSType;
		AkInt16 m_Loop;
		AkInt16 m_LoopMin;
		AkInt16 m_LoopMax;
		AkUInt32 m_Weight;
		AkUInt16 m_wAvoidRepeatCount;

		AkUInt8 m_bIsUsingWeight;
		AkUInt8 m_bIsShuffle;
	};

	struct AKBKMusicRanSeq
	{
		AKBKMusicTransNode transNode;
		std::vector<AKBKMusicRanSeqPlaylistItem> playlist;
	//Constructor
		AKBKMusicRanSeq()
		{}
	};

	struct AKBKLayerAssoc
	{
		// Constructor
		AKBKLayerAssoc()
			: ChildID(0)
		{}

		AkUniqueID						ChildID;
		std::vector<AkRTPCGraphPoint>	CrossfadingCurve;
	};

	struct AKBKLayer
	{
		// Constructor
		AKBKLayer()
			: ID(AK_INVALID_UNIQUE_ID)
		{}

		AkUniqueID						ID;
		std::vector<AKBKRTPC>			RTPCList;
		AkRtpcID						CrossfadingRTPCID;
		AkRtpcType						CrossfadingRTPCType;
		std::vector<AKBKLayerAssoc>		AssociatedChildren;
	};

	struct AKBKLayerCntr
	{
		// Constructor
		AKBKLayerCntr()
			:ID(AK_INVALID_UNIQUE_ID)
			,bIsContinuousValidation(false)
		{}

		AkUniqueID					ID;
		AKBKParameterNode 			ParamNode;
		std::vector<AkUniqueID>		ChildList;
		std::vector<AKBKLayer>		LayerList;
		AkUInt8		bIsContinuousValidation;	// Is the validation continuous
	};

	struct AKBKDuckInfo
	{
		AkUInt32		ulTargetBusID;
		AkReal32 	DuckVolume;
		AkTimeMs 	FadeOutTime;
		AkTimeMs 	FadeInTime;
		AkUInt8		eFadeCurveType;
		AkUInt8		eTargetProp;
	//Constructor
		AKBKDuckInfo()
			:DuckVolume(0)
			,FadeOutTime(0)
			,FadeInTime(0)
			,eFadeCurveType(AkCurveInterpolation_Linear)
			,eTargetProp( (AkPropID)0 )
		{}
	};


	struct AKBKActorMixer
	{
		AkUniqueID	ID;
		AKBKParameterNode 	ParamNode;
		std::vector<AkUniqueID>	ChildList;
	//Constructor
		AKBKActorMixer()
			:ID(AK_INVALID_UNIQUE_ID)
		{}
	};

	struct AKBKActionParams
	{
		AkUInt8		FadeCurveType;

		//Used if it is set pitch or set volume only, if unused, let the default values
		AkUInt8			TargetValueMeaning;

		// TargetVolume or Target Pitch ... Not both of them!!!
		AkReal32		TargetReal32;
		AkReal32		TargetReal32Min;
		AkReal32		TargetReal32Max;

		//Used only if it is a resume or a pause action
		// bool on 8 bits
		union {
			struct { // only applies to resume actions
				AkUInt8		bApplyToStateTransition;
				AkUInt8		bApplyToDynamicSequence;
			} stopAction;
			struct { // only applies to resume actions
				AkUInt8		bMasterResume;
				AkUInt8		bApplyToStateTransition;
				AkUInt8		bApplyToDynamicSequence;
			} resumeAction;
			struct { // only applies to pause actions
				AkUInt8		bPauseDelayedResumeAction;
				AkUInt8		bApplyToStateTransition;
				AkUInt8		bApplyToDynamicSequence;
			} pauseAction;
			struct { // only applies to set/reset game parameter actions
				AkUInt8		bBypassGameParameterInternalTransition;
			} setResetGameParameterAction;
		} actionSpecific;

		std::vector<WwiseObjectID>	ExceptionList;
	//Constructor
		AKBKActionParams()
			:FadeCurveType(AkCurveInterpolation_Linear)
			,actionSpecific{}
            ,TargetValueMeaning( AkValueMeaning_Default )
			,TargetReal32(0)
			,TargetReal32Min(0)
			,TargetReal32Max(0)
		{}
	};

	struct AKBKActionSetStatesParams
	{
		AkUInt32		StateGroupID;
		AkUInt32		TargetStateID;
	//Constructor
		AKBKActionSetStatesParams()
			:StateGroupID(0)
			,TargetStateID(0)
		{}
	};

	struct AKBKActionSetSwitchParams
	{
		AkUInt32		SwitchGroupID;
		AkUInt32		TargetSwitchID;
	//Constructor
		AKBKActionSetSwitchParams()
			:SwitchGroupID(0)
			,TargetSwitchID(0)
		{}
	};

	struct AKBKActionBypassFXParams
	{
		AkUInt8 bIsBypass;
		AkUInt8 uTargetMask;
		std::vector<WwiseObjectID>	ExceptionList;
		AKBKActionBypassFXParams()
			:bIsBypass( true )
			,uTargetMask( 0xFF )
		{}
	};

	struct AKBKActionSeekParams
	{
		//(14)

		// Note: Seek action uses AkReal32 values even for time absolute seeking.
		// In such a case, it is stored in milliseconds (need to convert the property).
		AkReal32	fSeekValue;
		AkReal32	fSeekValueModMin;
		AkReal32	fSeekValueModMax;

		// Seek type.
		AkUInt8		bIsSeekRelativeToDuration;

		AkUInt8		bSnapToMarker;

		std::vector<WwiseObjectID>	ExceptionList;

		//Constructor
		AKBKActionSeekParams()
			:fSeekValue(0)
			,fSeekValueModMin(0)
			,fSeekValueModMax(0)
			,bIsSeekRelativeToDuration(false)
			,bSnapToMarker(false)
		{}
	};

	struct AKBKAction
		: public AKBKPropAndRange
	{
		AkUniqueID		ID;
		AkUInt16		eActionType;	// Action type
		WwiseObjectID	ElementID;		// Target element ID

		AkBankID		bankID;

		// SubSections: only one of the following, based on action type
		AKBKActionParams			SpecificParamStd;
			//or
		AKBKActionSetStatesParams	SpecificParamSetState;	// if set state action
			//or
		AKBKActionSetSwitchParams	SpecificParamSetSwitch;	// if set switch action
			//or
		AKBKActionBypassFXParams	SpecificParamBypassFX;	// if bypass FX action
			//or
		AKBKActionSeekParams		SpecificParamSeek;		// if seek action

		//Constructor
		AKBKAction()
			: ID(AK_INVALID_UNIQUE_ID)
			, eActionType(0)
			, bankID(AK_INVALID_BANK_ID)
		{}
	};

	struct AKBKEvent
	{
		AkUniqueID			ID;
		std::vector<AkUniqueID>	ActionList;
	//Constructor
		AKBKEvent()
			:ID(AK_INVALID_UNIQUE_ID)
		{}
	};

	struct DecisionTreeBankNode : public AkDecisionTree::Node
	{
		bool IsAudioNode;

	//Constructor
		DecisionTreeBankNode()
			: IsAudioNode( false )
		{
			key = AK_INVALID_UNIQUE_ID;
			children.uIdx = 0;
			children.uCount = 0;

			uWeight = 50;
			uProbability = 100;
		}
	};

	struct AKBKMultiSwitchParams
	{
		AkInt32				TreeDepth;	// Is also the size of the ArgumentList
		std::vector<AkUInt16>	ArgumentGroupType;	// Is the argument a state group or a switch group
		std::vector<AkUniqueID> ArgumentList;	// Won't add the size of the vector

		AkInt32				DecisionTreeBufferSize;	// Buffer size not number of nodes
		std::vector<DecisionTreeBankNode> DecisionTree;	// Won't add the size of the vector

		AkUInt8				uMode;

		//Constructor
		AKBKMultiSwitchParams(): TreeDepth(0)
			,DecisionTreeBufferSize(0)
			,uMode(0)

		{
		}
	};

	struct AKBKMusicSwitchCntr
	{
		AKBKMusicTransNode		transNode;
		AKBKMultiSwitchParams	multiSwitchParams;
		AkUInt8					bContinuePlayback;

		AKBKMusicSwitchCntr(): bContinuePlayback(true) {}
	};


	struct AKBKDialogueEvent
		: public AKBKPropAndRange
	{
		AKBKMultiSwitchParams	multiSwitchParams;
		AkUniqueID				ID;
		AkUInt8					uProbability;

		AKBKDialogueEvent():
			ID(AK_INVALID_UNIQUE_ID), uProbability(100)
		{}
	};

	struct AKBKBus
		: public AKBKParameterNodeBase
	{
		AkUniqueID			ID;
		AkUniqueID			ParentBusID;
		AkUniqueID			DeviceShareSet;

		AkUInt8				bKillNewest;
		AkUInt8				bUseVirtualBehavior;
		AkUInt16			MaxNumInstance;// Zero being no max.
		AkUInt8				bIsMaxNumInstOverrideParent;
		AkChannelConfig		channelConfig;
		AkUInt8				bIsBackgroundMusic;
		AkUInt8				bEnableHdr;
		AkUInt8				bHdrReleaseModeExponential;

		AkTimeMs			DuckRecoveryTime;
		AkReal32			DuckMaxAttenuation;
		AkUInt8				bBypassAllFX;

		AKBKMixer			Mixer;

		std::vector<AKBKDuckInfo>	DuckList;

	//Constructor
		AKBKBus()
			:ID(AK_INVALID_UNIQUE_ID)
			,ParentBusID(AK_INVALID_UNIQUE_ID)
			,bKillNewest(false)
			,bUseVirtualBehavior(false)
			,MaxNumInstance(0)
			,bIsMaxNumInstOverrideParent(false)
			,bIsBackgroundMusic(false)
			,bEnableHdr(false)
			,bHdrReleaseModeExponential(false)
			,DuckRecoveryTime(0)
			,DuckMaxAttenuation( AK_DEFAULT_MAX_BUS_DUCKING )
		{}
	};

////////////////////////////////////////////////////////
// StateMgr specific structures
////////////////////////////////////////////////////////

	struct AKBKStateTransition
	{
		AkUInt32		StateFrom;
		AkUInt32		StateTo;
		AkTimeMs	TransitionTime;
		AKBKStateTransition()
			:StateFrom( 0 )
			,StateTo( 0 )
			,TransitionTime( 0 )
		{}
	};

	struct AKBKStateGroup
	{
		AkUInt32		StateGroupID;
		AkTimeMs	DefaultTransitionTime;
		std::vector<AKBKStateTransition>	StateTransitions;
		AKBKStateGroup()
			:StateGroupID( AK_INVALID_UNIQUE_ID )
			,DefaultTransitionTime( 0 )
		{}
	};

	struct AKBKSwitchRTPCGroup
	{
		AkUInt32		SwitchGroupID;
		AkUInt32		RTPC_ID;
		AkUInt8			RTPC_Type;
		std::vector<AkSwitchGraphPoint> ConversionTable;
		AKBKSwitchRTPCGroup()
			:SwitchGroupID( 0 )
			,RTPC_ID( 0 )
			,RTPC_Type( AkRtpcType_GameParameter )
		{}
	};

	struct AKBKGameParameter
	{
		AkReal32				fDefaultValue;
		AkTransitionRampingType rampType;
		AkReal32				fRampUp;
		AkReal32				fRampDown;
		AkBuiltInParam			eBuiltInParam;
	};

	struct AKBKAcousticTexture : public AKBKPropAndRange
	{
		AkUniqueID						ID;

		AkReal32						fAbsorptionOffset;
		AkReal32						fAbsorptionLow;
		AkReal32						fAbsorptionMidLow;
		AkReal32						fAbsorptionMidHigh;
		AkReal32						fAbsorptionHigh;
		AkReal32						fScattering;

		//Constructor
		AKBKAcousticTexture()
			:ID(AK_INVALID_UNIQUE_ID)
		{}
	};
	
	struct AKBKStateMgr
	{
		AkReal32	VolumeThreshold;
		AkUInt16	GlobalVoiceLimit;
		AkUInt16    GlobalDangerousVirtVoiceLimit;
		std::vector<AKBKStateGroup>			StateGroups;
		std::vector<AKBKSwitchRTPCGroup>	SwitchGroups;
		std::map<AkUInt32, AKBKGameParameter>	GameParamDefaults;
		std::vector<AKBKAcousticTexture>		AcousticTextureGroups;

		AKBKStateMgr()
		{}
	};

	struct AKBKEnvSettings
	{
		struct CurveData
		{
			CurveData()
				: bUseCurve( false )
				, eScaling( AkCurveScaling_None )
			{}

			bool bUseCurve;
			AkCurveScaling eScaling;
			std::vector<AkRTPCGraphPoint> Curve;
		};

		CurveData Curves[IWProject::MAX_CURVE_X_TYPES][IWProject::MAX_CURVE_Y_TYPES];
	};

	struct AKBKModulator : public AKBKPropAndRange
	{
		AkUniqueID						ID;
		std::vector<AKBKRTPC>			RTPCList;
		//Constructor
		AKBKModulator()
			:ID( AK_INVALID_UNIQUE_ID )
		{}
	};

}//end namespace "AkBank"

#endif
