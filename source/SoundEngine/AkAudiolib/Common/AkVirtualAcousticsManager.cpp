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

#define DEFINE_VIRTUAL_ACOUSTICS_PROP_DEFAULTS

#include "AkVirtualAcousticsManager.h"
#include "AkCritical.h"
#include "AkAudioLibIndex.h"

CAkVirtualAcoustics* CAkVirtualAcoustics::Create(AkUniqueID in_ulID, AkVirtualAcousticsType in_eType)
{
	CAkVirtualAcoustics * pVirtualAcoustics = NULL;
	
	switch (in_eType)
	{
	case AkVirtualAcousticsType_Texture:
		pVirtualAcoustics = AkNew(AkMemID_Structure, CAkAcousticTexture(in_ulID));
		break;
	default:
		AKASSERT(false && "Unknown virtual acoustics type.");
	}
	
	if (pVirtualAcoustics)
	{
		if (pVirtualAcoustics->Init() != AK_Success)
		{
			pVirtualAcoustics->Release();
			pVirtualAcoustics = NULL;
		}
	}

	return pVirtualAcoustics;
}

CAkVirtualAcoustics::CAkVirtualAcoustics(AkUniqueID in_ulID)
	: CAkIndexable(in_ulID)
{}

CAkVirtualAcoustics::~CAkVirtualAcoustics()
{
}

AKRESULT CAkVirtualAcoustics::Init()
{
	AddToIndex();
	return AK_Success;
}

void CAkVirtualAcoustics::AddToIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->m_idxVirtualAcoustics.SetIDToPtr(this);
}

void CAkVirtualAcoustics::RemoveFromIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->m_idxVirtualAcoustics.RemoveID(ID());
}

/** FIXME Use prop bundle in banks
AKRESULT CAkVirtualAcoustics::SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize)
{
	AKRESULT eResult = AK_Success;

	//Read ID
	// We don't care about the ID, just skip it
	SKIPBANKDATA(AkUInt32, in_pData, in_ulDataSize);

	eResult = m_props.SetInitialParams(in_pData, in_ulDataSize);
	if (eResult != AK_Success)
		return AK_Fail;

	CHECKBANKDATASIZE(in_ulDataSize, eResult);

	return eResult;
}
**/

AkUInt32 CAkVirtualAcoustics::AddRef()
{
	AkAutoLock<CAkLock> IndexLock(g_pIndex->m_idxVirtualAcoustics.GetLock());
	return ++m_lRef;
}

AkUInt32 CAkVirtualAcoustics::Release()
{
	AkAutoLock<CAkLock> IndexLock(g_pIndex->m_idxVirtualAcoustics.GetLock());
	AkInt32 lRef = --m_lRef;
	AKASSERT(lRef >= 0);
	if (!lRef)
	{
		RemoveFromIndex();
		AkDelete(AkMemID_Structure, this);
	}
	return lRef;
}

CAkAcousticTexture::CAkAcousticTexture(AkUniqueID in_ulID)
	:CAkVirtualAcoustics(in_ulID)
{}

void CAkAcousticTexture::SetAkProp(AkVirtualAcousticsPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax)
{
	switch (in_eProp)
	{
	case AkAcousticTexturePropID_AbsorptionOffset:
		m_texture.fAbsorptionOffset = in_fValue;
		break;
	case AkAcousticTexturePropID_AbsorptionLow:
		m_texture.fAbsorptionLow = in_fValue;
		break;
	case AkAcousticTexturePropID_AbsorptionMidLow:
		m_texture.fAbsorptionMidLow = in_fValue;
		break;
	case AkAcousticTexturePropID_AbsorptionMidHigh:
		m_texture.fAbsorptionMidHigh = in_fValue;
		break;
	case AkAcousticTexturePropID_AbsorptionHigh:
		m_texture.fAbsorptionHigh = in_fValue;
		break;
	case AkAcousticTexturePropID_Scattering:
		m_texture.fScattering = in_fValue;
		break;
	default:
		AKASSERT(!"Invalid prop");
	}
}

void CAkAcousticTexture::SetAkProp(AkVirtualAcousticsPropID, AkInt32, AkInt32, AkInt32)
{
	AKASSERT(!"Invalid prop");
}

//-----------------------------------------------------------------------------
AKRESULT CAkVirtualAcousticsMgr::AddAcousticTexture(AkAcousticTextureID in_AcousticTextureID, const AkAcousticTexture& in_data)
{
	AKRESULT eResult = AK_Success;

	CAkVirtualAcoustics* pObject = static_cast<CAkVirtualAcoustics*>(g_pIndex->m_idxVirtualAcoustics.GetPtrAndAddRef(in_AcousticTextureID));
	if (pObject == NULL)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pObject = CAkVirtualAcoustics::Create(in_AcousticTextureID, AkVirtualAcousticsType_Texture);
		if (!pObject)
		{
			eResult = AK_Fail;
		}
		else
		{
			((CAkAcousticTexture*)pObject)->Set(in_data);

			/// FIXME Have its own HIRC chunk.
			/*
			pObject->SetAkProp(AkAcousticTexturePropID_OnOffBand1, (AkInt32)in_data.OnOffBand1, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_OnOffBand2, (AkInt32)in_data.OnOffBand2, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_OnOffBand3, (AkInt32)in_data.OnOffBand3, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_FilterTypeBand1, (AkInt32)in_data.FilterTypeBand1, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_FilterTypeBand2, (AkInt32)in_data.FilterTypeBand1, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_FilterTypeBand3, (AkInt32)in_data.FilterTypeBand1, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_FrequencyBand1, in_data.FrequencyBand1, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_FrequencyBand2, in_data.FrequencyBand2, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_FrequencyBand3, in_data.FrequencyBand3, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_GainBand1, in_data.GainBand1, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_GainBand2, in_data.GainBand2, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_GainBand3, in_data.GainBand3, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_QFactorBand1, in_data.QFactorBand1, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_QFactorBand2, in_data.QFactorBand2, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_QFactorBand3, in_data.QFactorBand3, 0, 0);
			pObject->SetAkProp(AkAcousticTexturePropID_Gain, in_data.OutputGain, 0, 0);
			*/
		}
	}

	return eResult;
}

const AkAcousticTexture* CAkVirtualAcousticsMgr::GetAcousticTexture(AkAcousticTextureID in_AcousticTextureID)
{
	CAkAcousticTexture * pTexture = (CAkAcousticTexture*)g_pIndex->m_idxVirtualAcoustics.GetPtrAndAddRef(in_AcousticTextureID);
	if (pTexture)
		return pTexture->Get();
	return NULL;
}


