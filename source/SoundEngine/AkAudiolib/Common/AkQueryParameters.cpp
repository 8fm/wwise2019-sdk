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

// AkQueryParameters.cpp : Get some parameters values from the sound engine
//
#include "stdafx.h"

#include <AK/SoundEngine/Common/AkQueryParameters.h>
#include "Ak3DListener.h"
#include "AkCritical.h"
#include "AkEvent.h"
#include "AkSwitchMgr.h"
#include "AkParameterNode.h"
#include "AkURenderer.h"
#include "AkPlayingMgr.h"
#include "AkRegistryMgr.h"

namespace AK
{
	namespace SoundEngine
	{
		namespace Query
		{
			AKRESULT GetPosition( 
				AkGameObjectID in_GameObj, 
				AkSoundPosition& out_rPosition
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkEmitter * pObj = g_pRegistryMgr->GetGameObjComponent<CAkEmitter>( in_GameObj );
				if( !pObj )
					return AK_IDNotFound;
				
				out_rPosition = pObj->GetPosition().GetFirstPositionFixme();
				return AK_Success;
			}

			AKRESULT GetListeners(
				AkGameObjectID in_GameObjectID,				///< Source game object identifier
				AkGameObjectID* out_ListenerObjectIDs,		///< Pointer to an array of AkGameObjectID's.  Will be populated with the IDs of the listeners of in_GameObjectID. Pass NULL to querry the size required.
				AkUInt32& oi_uNumListeners					///< Pass in the the available number of elements in the array 'out_ListenerObjectIDs'. After return, the number of valid elements filled in the array.
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkRegisteredObj * pObj = g_pRegistryMgr->GetObjAndAddref(in_GameObjectID);
				if( !pObj )
					return AK_IDNotFound;

				AkUInt32 uArraySize = oi_uNumListeners;
				oi_uNumListeners = pObj->GetListeners().Length();

				if (out_ListenerObjectIDs != NULL)
				{
					for (AkUInt32 i = 0; i < AkMin(oi_uNumListeners, uArraySize); ++i)
						out_ListenerObjectIDs[i] = pObj->GetListeners()[i];
				}

				pObj->Release();

				return uArraySize >= oi_uNumListeners ? AK_Success : AK_Fail;
			}

			AKRESULT GetListenerPosition( 
				AkGameObjectID in_ulIndex,
				AkListenerPosition& out_rPosition
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkListener* p = g_pRegistryMgr->GetGameObjComponent<CAkListener>(in_ulIndex);
				if (p != NULL)
				{
					out_rPosition = p->GetTransform();
					return AK_Success;
				}
				return AK_Fail;
			}

			AKRESULT GetListenerSpatialization(
				AkUInt32 in_ulIndex,						///< Listener index. 
				bool& out_rbSpatialized,
				AK::SpeakerVolumes::VectorPtr& out_pVolumeOffsets,	///< Per-speaker vector of volume offsets, in decibels. Use the functions of AK::SpeakerVolumes::Vector to interpret it.
				AkChannelConfig &out_channelConfig			///< Channel configuration associated with out_rpVolumeOffsets. 
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				return CAkListener::GetListenerSpatialization( in_ulIndex, out_rbSpatialized, out_pVolumeOffsets, out_channelConfig );
			}

			AKRESULT GetRTPCValue( 
				AkRtpcID in_RTPCid, 
				AkGameObjectID in_GameObj,
				AkPlayingID in_playingID,
				AkReal32& out_rfValue, 
				RTPCValue_type&	io_rValueType
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				AKRESULT result = AK_IDNotFound;

				switch( io_rValueType )
				{
				case RTPCValue_Global:
					in_GameObj = AK_INVALID_GAME_OBJECT; // Make sure we don't force GameObject specific, the user asked for global.
					// No Break here.

				case RTPCValue_GameObject:
					in_playingID = AK_INVALID_PLAYING_ID;// Make sure we don't force PlayingID specific, the user asked for GameObject.
					// No Break here.

				case RTPCValue_PlayingID:
				{
					if (in_playingID != AK_INVALID_PLAYING_ID && in_GameObj == AK_INVALID_GAME_OBJECT)
					{
						//So the user can set in_GameObj = AK_INVALID_GAME_OBJECT, and not worry about passing the correct game object.
						in_GameObj = g_pPlayingMgr->GetGameObjectFromPlayingID(in_playingID);
					}

					CAkRegisteredObj * pObj = g_pRegistryMgr->GetObjAndAddref(in_GameObj);
					AkRTPCKey rtpcKey(pObj);
					rtpcKey.PlayingID() = in_playingID;
					bool bAutomatedParam;
					bool bResult = g_pRTPCMgr->GetRTPCValue<CurrentValue>(in_RTPCid, RTPC_MaxNumRTPC, CAkRTPCMgr::SubscriberType_CAkParameterNodeBase, rtpcKey, out_rfValue, bAutomatedParam);
					
					if (rtpcKey.PlayingID() != AK_INVALID_PLAYING_ID)
						io_rValueType = RTPCValue_PlayingID;
					else if (rtpcKey.GameObj() != NULL)
						io_rValueType = RTPCValue_GameObject;
					else
						io_rValueType = RTPCValue_Global;
					
					if (pObj)
						pObj->Release();

					if (bResult)
					{
						result = AK_Success;
						break;
					}
				}

				// Failed to get GameObject/Global/PlayingId value, fallback on Default.

				// No Break here.

				case RTPCValue_Default:
				default:
					bool bRTPCFound;
					out_rfValue = g_pRTPCMgr->GetDefaultValue( in_RTPCid, &bRTPCFound );
					if ( bRTPCFound )
					{
						io_rValueType = RTPCValue_Default;
						result = AK_Success;
					}
					else
						io_rValueType = RTPCValue_Unavailable;
					break;
				}
				
				return result;
			}

#ifdef AK_SUPPORT_WCHAR
			AKRESULT GetRTPCValue( 
				const wchar_t* in_pszRTPCName, 
				AkGameObjectID in_GameObj,
				AkPlayingID in_playingID,
				AkReal32& out_rfValue, 
				RTPCValue_type&	io_rValueType
				)
			{
				AkRtpcID id = AK::SoundEngine::GetIDFromString( in_pszRTPCName );
				return GetRTPCValue(id, in_GameObj, in_playingID, out_rfValue, io_rValueType);
			}
#endif //AK_SUPPORT_WCHAR

			AKRESULT GetRTPCValue( 
				const char* in_pszRTPCName, 
				AkGameObjectID in_GameObj,
				AkPlayingID in_playingID,
				AkReal32& out_rfValue, 
				RTPCValue_type&	io_rValueType
				)
			{
				AkRtpcID id = AK::SoundEngine::GetIDFromString( in_pszRTPCName );
				return GetRTPCValue(id, in_GameObj, in_playingID, out_rfValue, io_rValueType);
			}

			AKRESULT GetSwitch( 
				AkSwitchGroupID in_SwitchGroup, 
				AkGameObjectID in_GameObj,
				AkSwitchStateID& out_rSwitchState
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkRegisteredObj * pObj = g_pRegistryMgr->GetObjAndAddref( in_GameObj );
				if( !pObj )
					return AK_IDNotFound;

				bool result = g_pSwitchMgr->GetSwitch( in_SwitchGroup, AkRTPCKey(pObj), out_rSwitchState );
				pObj->Release();
				return result ? AK_Success : AK_IDNotFound;
			}

#ifdef AK_SUPPORT_WCHAR
			AKRESULT GetSwitch( 
				const wchar_t* in_pstrSwitchGroupName, 
				AkGameObjectID in_GameObj,
				AkSwitchStateID& out_rSwitchState
				)
			{
				AkSwitchGroupID switchGroup = AK::SoundEngine::GetIDFromString( in_pstrSwitchGroupName );
				return GetSwitch( switchGroup, in_GameObj, out_rSwitchState );
			}
#endif //AK_SUPPORT_WCHAR

			AKRESULT GetSwitch( 
				const char* in_pstrSwitchGroupName, 
				AkGameObjectID in_GameObj,
				AkSwitchStateID& out_rSwitchState
				)
			{
				AkSwitchGroupID switchGroup = AK::SoundEngine::GetIDFromString( in_pstrSwitchGroupName );
				return GetSwitch( switchGroup, in_GameObj, out_rSwitchState );
			}

			AKRESULT GetState( 
				AkStateGroupID in_StateGroup, 
				AkStateID& out_rState )
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				bool result = g_pStateMgr->GetState( in_StateGroup, out_rState );
				return result ? AK_Success : AK_IDNotFound;
			}

#ifdef AK_SUPPORT_WCHAR
			AKRESULT GetState( 
				const wchar_t* in_pstrStateGroupName, 
				AkStateID& out_rState )
			{
				AkStateGroupID stateGroup = AK::SoundEngine::GetIDFromString( in_pstrStateGroupName );
				return GetState( stateGroup, out_rState );
			}
#endif //AK_SUPPORT_WCHAR

			AKRESULT GetState( 
				const char* in_pstrStateGroupName, 
				AkStateID& out_rState )
			{
				AkStateGroupID stateGroup = AK::SoundEngine::GetIDFromString( in_pstrStateGroupName );
				return GetState( stateGroup, out_rState );
			}

			AKRESULT GetGameObjectAuxSendValues( 
				AkGameObjectID		in_GameObj,					///< the unique object ID
				AkAuxSendValue*	out_paEnvironmentValues,	///< variable-size array of AkAuxSendValue(s)
				AkUInt32&			io_ruNumEnvValues			///< number of elements in struct
				)
			{
				AKRESULT eReturn = AK_Success;

				if( io_ruNumEnvValues == 0 || !out_paEnvironmentValues )
					return AK_InvalidParameter;

				/// OPTI: Have policy that lets AkAuxSendArray allocate on the stack
				AkAuxSendArray auxSends;

				{
					CAkFunctionCritical SetSpaceAsCritical; //use global lock

					CAkRegisteredObj * pObj = g_pRegistryMgr->GetObjAndAddref(in_GameObj);
					if (!pObj)
						return AK_IDNotFound;
					
					pObj->GetConnectedListeners().GetAuxGains(auxSends, 1.f, 0.f, 0.f);
					
					pObj->Release();
				}

				io_ruNumEnvValues = AkMin(io_ruNumEnvValues, auxSends.Length());
				for (unsigned int iAuxIdx = 0; iAuxIdx < io_ruNumEnvValues; ++iAuxIdx)
					out_paEnvironmentValues[iAuxIdx] = auxSends[iAuxIdx];

				auxSends.Term();

				return eReturn;
			}

			AKRESULT GetGameObjectDryLevelValue( 
				AkGameObjectID		in_EmitterObj,			///< the emitter unique object ID
				AkGameObjectID		in_ListenerObj,			///< the listener unique object ID
				AkReal32&			out_rfControlValue	///< control value for dry level
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkConnectedListeners * pComp = g_pRegistryMgr->GetGameObjComponent<CAkConnectedListeners>(in_EmitterObj);
				if (!pComp)
					return AK_IDNotFound;

				out_rfControlValue = pComp->GetUserGain(in_ListenerObj);

				return AK_Success;
			}

			AKRESULT GetObjectObstructionAndOcclusion(  
				AkGameObjectID in_ObjectID,				///< Game object ID.
				AkGameObjectID in_uListener,			///< Listener ID.
				AkReal32& out_rfObstructionLevel,		///< ObstructionLevel : [0.0f..1.0f]
				AkReal32& out_rfOcclusionLevel			///< OcclusionLevel   : [0.0f..1.0f]
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkEmitter * pObj = g_pRegistryMgr->GetGameObjComponent<CAkEmitter>(in_ObjectID);
				if( !pObj )
					return AK_IDNotFound;

				AkGameRayParams rayParams;
				pObj->GetPosition().GetGameRayParams(in_uListener, 1, &rayParams);
				out_rfObstructionLevel = rayParams.obstruction;
				out_rfOcclusionLevel = rayParams.occlusion;
				
				return AK_Success;
			}

			// Advanced functions

			AKRESULT QueryAudioObjectIDs(
				AkUniqueID in_eventID,
				AkUInt32& io_ruNumItems,
				AkObjectInfo* out_aObjectInfos 
				)
			{
				if( io_ruNumItems && !out_aObjectInfos )
					return AK_InvalidParameter;

				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
				if( !pEvent )
					return AK_IDNotFound;

				AKRESULT eResult = pEvent->QueryAudioObjectIDs( io_ruNumItems, out_aObjectInfos );
				pEvent->Release();
				return eResult;
			}

#ifdef AK_SUPPORT_WCHAR
			AKRESULT QueryAudioObjectIDs(
				const wchar_t* in_pszEventName,
				AkUInt32& io_ruNumItems,
				AkObjectInfo* out_aObjectInfos 
				)
			{
				AkUniqueID eventID = AK::SoundEngine::GetIDFromString( in_pszEventName );
				if ( eventID == AK_INVALID_UNIQUE_ID )
					return AK_IDNotFound;

				return QueryAudioObjectIDs( eventID, io_ruNumItems, out_aObjectInfos );
			}
#endif //AK_SUPPORT_WCHAR

			AKRESULT QueryAudioObjectIDs(
				const char* in_pszEventName,
				AkUInt32& io_ruNumItems,
				AkObjectInfo* out_aObjectInfos 
				)
			{
				AkUniqueID eventID = AK::SoundEngine::GetIDFromString( in_pszEventName );
				if ( eventID == AK_INVALID_UNIQUE_ID )
					return AK_IDNotFound;

				return QueryAudioObjectIDs( eventID, io_ruNumItems, out_aObjectInfos );
			}

			AKRESULT GetPositioningInfo( 
				AkUniqueID in_ObjectID,
				AkPositioningInfo& out_rPositioningInfo 
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				CAkParameterNodeBase * pObj = g_pIndex->GetNodePtrAndAddRef( in_ObjectID, AkNodeType_Default );
				if( !pObj )
					return AK_IDNotFound;

				CAkParameterNode* pParam = (CAkParameterNode *)( pObj );
				AKRESULT eResult = pParam->GetStatic3DParams( out_rPositioningInfo );
				pObj->Release();

				return eResult;
			}

			AKRESULT GetActiveGameObjects( 
				AkGameObjectsList& io_GameObjectList
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				return g_pRegistryMgr->GetActiveGameObjects( io_GameObjectList );
			}

			bool GetIsGameObjectActive(
				AkGameObjectID in_GameObjId
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				return g_pRegistryMgr->IsGameObjectActive( in_GameObjId );
			}

			AKRESULT GetMaxRadius(
				AkRadiusList & io_RadiusList
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				return CAkURenderer::GetMaxRadius( io_RadiusList );
			}

			AkReal32 GetMaxRadius(
				AkGameObjectID in_GameObjId
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock
				return CAkURenderer::GetMaxRadius( in_GameObjId );
			}

			AkUniqueID GetEventIDFromPlayingID( AkPlayingID in_playingID )
			{
				// Global Lock not required here, only acceding Playing manager which has its own lock.
				//CAkFunctionCritical SetSpaceAsCritical; //use global lock

				/// \return AK_INVALID_UNIQUE_ID on failure.
				if( g_pPlayingMgr )
					return g_pPlayingMgr->GetEventIDFromPlayingID( in_playingID );
				return AK_INVALID_UNIQUE_ID;
			}

			AkGameObjectID GetGameObjectFromPlayingID( AkPlayingID in_playingID )
			{
				// Global Lock not required here, only acceding Playing manager which has its own lock.
				//CAkFunctionCritical SetSpaceAsCritical; //use global lock

				/// \return AK_INVALID_GAME_OBJECT on failure.
				if( g_pPlayingMgr )
					return g_pPlayingMgr->GetGameObjectFromPlayingID( in_playingID );
				return AK_INVALID_GAME_OBJECT;
			}

			AKRESULT GetPlayingIDsFromGameObject(
				AkGameObjectID in_GameObjId,	
				AkUInt32& io_ruNumIDs,
				AkPlayingID* out_aPlayingIDs
				)
			{
				// Global Lock not required here, only acceding Playing manager which has its own lock.
				//CAkFunctionCritical SetSpaceAsCritical; //use global lock

				/// \aknote It is possible to call GetPlayingIDsFromGameObject with io_ruNumItems = 0 to get the total size of the
				/// structure that should be allocated for out_aPlayingIDs. \endaknote
				/// \return AK_Success if succeeded, AK_InvalidParameter if out_aPlayingIDs is NULL while io_ruNumItems > 0
				if( g_pPlayingMgr )
					return g_pPlayingMgr->GetPlayingIDsFromGameObject( in_GameObjId, io_ruNumIDs, out_aPlayingIDs );
				return AK_Fail;
			}
			
			AKRESULT GetCustomPropertyValue(
				AkUniqueID in_ObjectID,		///< Object ID
				AkUInt32 in_uPropID,			///< Property ID
				AkInt32& out_iValue				///< Property Value
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock

				CAkParameterNodeBase * pObj = g_pIndex->GetNodePtrAndAddRef( in_ObjectID, AkNodeType_Default );
				if( !pObj )
					return AK_IDNotFound;

				AkPropValue * pValue = pObj->FindCustomProp( in_uPropID );
				if ( pValue )
				{
					out_iValue = pValue->iValue;
					pObj->Release();
					return AK_Success;
				}
				else
				{
					pObj->Release();
					return AK_PartialSuccess;
				}
			}

			AKRESULT GetCustomPropertyValue(
				AkUniqueID in_ObjectID,		///< Object ID
				AkUInt32 in_uPropID,			///< Property ID
				AkReal32& out_fValue			///< Property Value
				)
			{
				CAkFunctionCritical SetSpaceAsCritical; //use global lock

				CAkParameterNodeBase * pObj = g_pIndex->GetNodePtrAndAddRef( in_ObjectID, AkNodeType_Default );
				if( !pObj )
					return AK_IDNotFound;

				AkPropValue * pValue = pObj->FindCustomProp( in_uPropID );
				if ( pValue )
				{
					out_fValue = pValue->fValue;
					pObj->Release();
					return AK_Success;
				}
				else
				{
					pObj->Release();
					return AK_PartialSuccess;
				}
			}

		} // namespace Query
	} // namespace SoundEngine
} // namespace AK
