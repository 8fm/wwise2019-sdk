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
#ifndef AK_OPTIMIZED

#include "FxBaseProxyConnected.h"
#include "AkAudioLib.h"
#include "AkFxBase.h"
#include "CommandDataSerializer.h"
#include "IFxBaseProxy.h"

FxBaseProxyConnected::FxBaseProxyConnected(AkUniqueID in_id, FxBaseProxyType in_eType)
{
	CAkIndexable * pIndexable = NULL;;

	switch (in_eType)
	{
	case FxBaseProxyType_AudioDevice:
		pIndexable = AK::SoundEngine::GetIndexable(in_id, AkIdxType_AudioDevice);
		if (!pIndexable)
			pIndexable = CAkAudioDevice::Create(in_id);
		break;
	case FxBaseProxyType_EffectShareset:
		pIndexable = AK::SoundEngine::GetIndexable(in_id, AkIdxType_FxShareSet);
		if (!pIndexable)
			pIndexable = CAkFxShareSet::Create(in_id);
		break;
	case FxBaseProxyType_EffectCustom:
		pIndexable = AK::SoundEngine::GetIndexable(in_id, AkIdxType_FxCustom);
		if (!pIndexable)
			pIndexable = CAkFxCustom::Create(in_id);
		break;
	}

	SetIndexable( pIndexable );
}

FxBaseProxyConnected::~FxBaseProxyConnected()
{
}

void FxBaseProxyConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& /*out_rReturnSerializer*/ )
{
	CAkFxBase * pFX = static_cast<CAkFxBase *>( GetIndexable() );

	switch( in_uMethodID )
	{
	case IFxBaseProxy::MethodSetFX:
		{
			FxBaseProxyCommandData::SetFX setfx;
			if (in_rSerializer.Get( setfx ))
				pFX->SetFX( setfx.m_param1, NULL, 0 );
			break;
		}

	case IFxBaseProxy::MethodSetFXParam:
		{
			FxBaseProxyCommandData::SetFXParam setfxparam;
			if (in_rSerializer.Get( setfxparam ))
				pFX->SetFXParam( setfxparam.m_param2, setfxparam.m_param1.pBlob, setfxparam.m_param1.uBlobSize );
			break;
		}

	case IFxBaseProxy::MethodSetRTPC:
		{
			FxBaseProxyCommandData::SetRTPC setRTPC;
			if (in_rSerializer.Get( setRTPC ))
				pFX->SetRTPC( setRTPC.m_RTPCID, setRTPC.m_RTPCType, setRTPC.m_RTPCAccum, setRTPC.m_paramID, setRTPC.m_RTPCCurveID, setRTPC.m_eScaling, setRTPC.m_pArrayConversion, setRTPC.m_ulConversionArraySize );
			break;
		}

	case IFxBaseProxy::MethodUnsetRTPC:
		{
			FxBaseProxyCommandData::UnsetRTPC unsetrtpc;
			if (in_rSerializer.Get( unsetrtpc ))
				pFX->UnsetRTPC( (AkRTPC_ParameterID) unsetrtpc.m_param1, unsetrtpc.m_param2 );
			break;
		}

	case IFxBaseProxy::MethodAddStateGroup:
		{
			FxBaseProxyCommandData::AddStateGroup addStateGroup;
			if( in_rSerializer.Get( addStateGroup ) )
					pFX->AddStateGroup( addStateGroup.m_param1 );
			break;
		}

	case IFxBaseProxy::MethodRemoveStateGroup:
		{
			FxBaseProxyCommandData::RemoveStateGroup removeStateGroup;
			if( in_rSerializer.Get( removeStateGroup ) )
					pFX->RemoveStateGroup( removeStateGroup.m_param1 );
			break;
		}

	case IFxBaseProxy::MethodUpdateStateGroups:
		{
			FxBaseProxyCommandData::UpdateStateGroups updateStateGroups;
			if( in_rSerializer.Get( updateStateGroups ) )
				pFX->UpdateStateGroups( updateStateGroups.m_uGroups, updateStateGroups.m_pGroups, updateStateGroups.m_pStates );
			break;
		}

	case IFxBaseProxy::MethodSetStateProperties:
		{
			FxBaseProxyCommandData::SetStateProperties setStateProperties;
			if( in_rSerializer.Get( setStateProperties ) )
				pFX->SetStateProperties( setStateProperties.m_uProperties, setStateProperties.m_pProperties );
			break;
		}

	case IFxBaseProxy::MethodAddState:
		{
			FxBaseProxyCommandData::AddState addState;
			if( in_rSerializer.Get( addState ) )
					pFX->AddState( addState.m_param1, addState.m_param2, addState.m_param3 );

			break;
		}

	case IFxBaseProxy::MethodRemoveState:
		{
			FxBaseProxyCommandData::RemoveState removeState;
			if( in_rSerializer.Get( removeState ) )
					pFX->RemoveState( removeState.m_param1, removeState.m_param2 );

			break;
		}

	case IFxBaseProxy::MethodSetMediaID:
		{
			FxBaseProxyCommandData::SetMediaID setmediaid;
			if (in_rSerializer.Get(setmediaid))
				pFX->SetMediaID(setmediaid.m_param1, setmediaid.m_param2);
			break;
		}

	default:
		AKASSERT( false );
	}
}
#endif // #ifndef AK_OPTIMIZED
