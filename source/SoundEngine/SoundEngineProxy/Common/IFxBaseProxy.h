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

#ifndef AK_OPTIMIZED

#include "IObjectProxy.h"

#include "AkRTPC.h"

struct AkFXParamBlob
{
	void * pBlob;
	AkUInt32 uBlobSize;
	bool bInterpretAsString;
	bool bInterpretAsFxData;
	bool bAllocated;
	
	//
	AkFXParamBlob()
	:bInterpretAsString(false)
	,bInterpretAsFxData(false)
	,bAllocated(false)
	{}

	~AkFXParamBlob()
	{
#ifndef WWISE_AUTHORING
		if (bAllocated)
		{
			AkFalign(AkMemID_Profiler, pBlob);
		}
#endif
		pBlob = NULL;
		uBlobSize = 0;
		bAllocated = false;
	}

	template <class	T>
	bool Serialize( T& in_rSerializer ) const
	{
		void * pSerialized;
		AkUInt32 uSerializedSize;

		T paramSerializer( in_rSerializer.GetSwapEndian() );

		if ( bInterpretAsString )
		{
			paramSerializer.Put( (const AkUtf16*)pBlob );
		}
		else if ( bInterpretAsFxData )
		{
			// This section was added to fix 
			// WG-18526
			// When the data arrives from a plug-in serializer, the data is already byteswapped.
			// The problem was if the blob size was either 8,4,3 or 1 (see below)
			// the Data was byteswapped twice.
			pSerialized = pBlob;
			uSerializedSize = uBlobSize;
		}
		else
		{
			switch( uBlobSize )
			{
			case 8:
				paramSerializer.Put( *(AkInt64*) pBlob );
				break;
		
			case 4:
				paramSerializer.Put( *(AkUInt32*) pBlob );
				break;
		
			case 2:
				paramSerializer.Put( *(AkUInt16*) pBlob );
				break;
		
			case 1:
				paramSerializer.Put( *(AkUInt8*) pBlob );
				break;
		
			default:
				pSerialized = pBlob;
				uSerializedSize = uBlobSize;
				break;
			}
		}

		if( paramSerializer.GetWrittenSize() > 0 )
		{
			pSerialized = paramSerializer.GetWrittenBytes();
			uSerializedSize = paramSerializer.GetWrittenSize();
		}

		return in_rSerializer.Put( pSerialized, uSerializedSize );
	}

	template <class	T>
	bool Deserialize( T& in_rSerializer )
	{
		bool bRet = in_rSerializer.Get( pBlob, uBlobSize );
#ifndef WWISE_AUTHORING
		if (bRet && ((AkUIntPtr)pBlob & 0x3))
		{
			//Unaligned.  We ensure that the FX param blocks are aligned to 4 bytes.
			void *pAligned = AkMalign(AkMemID_Profiler, uBlobSize, 4);
			if (pAligned)
			{
				memcpy(pAligned, pBlob, uBlobSize);
				pBlob = pAligned;
				bAllocated = true;
			}
			else
				bRet = false;
		}
#endif

		return bRet;
	}
};

enum FxBaseProxyType
{
	FxBaseProxyType_EffectShareset,
	FxBaseProxyType_EffectCustom,
	FxBaseProxyType_AudioDevice
};

class IFxBaseProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
	virtual void SetFX( 
		AkPluginID in_FXID
		) = 0;

	virtual void SetFXParam( 
		AkFXParamBlob in_blobParams, // Placed first due to align-4 constraint on the blob
		AkPluginParamID in_uParamID
		) = 0;

	virtual void SetRTPC( 
		AkRtpcID			in_RTPC_ID,
		AkRtpcType			in_RTPCType,
		AkRtpcAccum			in_RTPCAccum,
		AkRTPC_ParameterID	in_ParamID,
		AkUniqueID			in_RTPCCurveID,
		AkCurveScaling		in_eScaling,
		AkRTPCGraphPoint* in_pArrayConversion = NULL, 
		AkUInt32 in_ulConversionArraySize = 0
		) = 0;

	virtual void UnsetRTPC( 
		AkRTPC_ParameterID in_ParamID,
		AkUniqueID in_RTPCCurveID
		) = 0;

	virtual void SetStateProperties(
		AkUInt32 in_uStateProperties,
		AkStatePropertyUpdate* in_pStateProperties
		) = 0;

	virtual void AddStateGroup(
		AkStateGroupID in_stateGroupID
		) = 0;

	virtual void RemoveStateGroup(
		AkStateGroupID in_stateGroupID
		) = 0;

	virtual void UpdateStateGroups(
		AkUInt32 in_uGroups,
		AkStateGroupUpdate* in_pGroups,
		AkStateUpdate* in_pUpdates
		) = 0;	

	virtual void AddState(
		AkStateGroupID in_stateGroupID,
		AkUniqueID in_stateInstanceID,
		AkStateID in_stateID
		) = 0;

	virtual void RemoveState(
		AkStateGroupID in_stateGroupID,
		AkStateID in_stateID
		) = 0;

	virtual void SetMediaID( 
		AkUInt32 in_uIdx, 
		AkUniqueID in_mediaID 
		) = 0;

	virtual void LoadPluginMedia( 
		AkUInt32 in_uIdx, 
		const AkOSChar* in_pszSourceFile, 
		AkUniqueID in_mediaID, 
		unsigned long in_hash 
		) = 0;
	
	virtual void ReleaseAllPluginMedia() = 0;

	enum MethodIDs
	{
		MethodSetFX = __base::LastMethodID,
		MethodSetFXParam,

		MethodSetRTPC,
		MethodUnsetRTPC,
		MethodSetMediaID,

		MethodSetStateProperties,
		MethodAddStateGroup,
		MethodRemoveStateGroup,
		MethodUpdateStateGroups,
		MethodAddState,
		MethodRemoveState,

		LastMethodID
	};
};

namespace FxBaseProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IFxBaseProxy::MethodSetFX, 
		AkPluginID
	> SetFX;

	typedef ObjectProxyCommandData::CommandData2
	< 
		IFxBaseProxy::MethodSetFXParam, 
		AkFXParamBlob, // Placed first due to align-4 constraint on the blob
		AkPluginParamID
	> SetFXParam;

	struct SetRTPC : public ObjectProxyCommandData::CommandData, public ProxyCommandData::CurveData<AkRTPCGraphPoint>
	{
		SetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCID;
		AkRtpcType m_RTPCType;
		AkRtpcAccum m_RTPCAccum;
		AkRTPC_ParameterID m_paramID;
		AkUniqueID m_RTPCCurveID;

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData2
	< 
		IFxBaseProxy::MethodUnsetRTPC, 
		AkUInt32,
		AkUniqueID
	> UnsetRTPC;

	typedef ObjectProxyCommandData::CommandData2
	<
		IFxBaseProxy::MethodSetMediaID,
		AkUInt32,
		AkUniqueID
	> SetMediaID;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IFxBaseProxy::MethodAddStateGroup, 
		AkStateGroupID 
	> AddStateGroup;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IFxBaseProxy::MethodRemoveStateGroup, 
		AkStateGroupID 
	> RemoveStateGroup;

	struct SetStateProperties : public ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodSetStateProperties>
	{
		SetStateProperties();
		SetStateProperties(IObjectProxy * in_pProxy);
		~SetStateProperties();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_uProperties;
		AkStatePropertyUpdate * m_pProperties;		

		bool m_bDeserialized;

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodSetStateProperties> );
	};

	struct UpdateStateGroups : public ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodUpdateStateGroups>
	{
		UpdateStateGroups();
		UpdateStateGroups(IObjectProxy * in_pProxy);
		~UpdateStateGroups();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_uGroups;
		AkStateGroupUpdate * m_pGroups;		
		AkStateUpdate *m_pStates;

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodUpdateStateGroups> );
	};
	
	typedef ObjectProxyCommandData::CommandData3
	< 
		IFxBaseProxy::MethodAddState,
		AkStateGroupID,
		AkUniqueID, 
		AkStateID 
	> AddState;
	
	typedef ObjectProxyCommandData::CommandData2
	< 
		IFxBaseProxy::MethodRemoveState,
		AkStateGroupID,
		AkStateID 
	> RemoveState;
}

#endif // #ifndef AK_OPTIMIZED
