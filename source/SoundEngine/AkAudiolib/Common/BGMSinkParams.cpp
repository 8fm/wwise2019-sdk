/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Version:  Build: 
  Copyright (c) 2006-2019 Audiokinetic Inc.
*******************************************************************************/

#include "stdafx.h"
#include "BGMSinkParams.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>

AK::IAkPluginParam* AkCreateBGMParam(AK::IAkPluginMemAlloc * in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, BGMSinkParams());
}

// Constructor/destructor.
BGMSinkParams::BGMSinkParams()
{
}

BGMSinkParams::~BGMSinkParams()
{
}

// Copy constructor.
BGMSinkParams::BGMSinkParams(const BGMSinkParams & in_rCopy)
{
	m_bDVRRecordable = in_rCopy.m_bDVRRecordable;
}

// Create duplicate.
AK::IAkPluginParam * BGMSinkParams::Clone(AK::IAkPluginMemAlloc * in_pAllocator)
{	
	return AK_PLUGIN_NEW(in_pAllocator, BGMSinkParams(*this));
}

// Init/Term.
AKRESULT BGMSinkParams::Init(AK::IAkPluginMemAlloc * in_pAllocator, const void * in_pParamsBlock, AkUInt32 in_ulBlockSize)
{
	if (in_ulBlockSize == 0)
	{
		// Init default parameters.
		m_bDVRRecordable = true;
		return AK_Success;
	}

	return SetParamsBlock(in_pParamsBlock, in_ulBlockSize);
}

AKRESULT BGMSinkParams::Term(AK::IAkPluginMemAlloc * in_pAllocator)
{	
	AK_PLUGIN_DELETE(in_pAllocator, this);
	return AK_Success;
}

AKRESULT BGMSinkParams::SetParamsBlock(const void * in_pParamsBlock, AkUInt32 in_ulBlockSize)
{		
	AkUInt8 *pParamsBlock = (AkUInt8 *)in_pParamsBlock;
	m_bDVRRecordable = READBANKDATA(bool, pParamsBlock, in_ulBlockSize);
	AKRESULT eResult = AK_Success;
	CHECKBANKDATASIZE(in_ulBlockSize, eResult);
	return eResult;
}

AKRESULT BGMSinkParams::SetParam(AkPluginParamID in_ParamID, const void * in_pValue, AkUInt32 in_ulParamSize)
{	
	if(in_pValue == NULL || in_ParamID != 0)
	{
		return AK_InvalidParameter;
	}
	m_bDVRRecordable = (*(bool*)(in_pValue));	

	return AK_Success;
}
