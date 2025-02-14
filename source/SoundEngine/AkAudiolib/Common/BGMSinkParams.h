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

//////////////////////////////////////////////////////////////////////
//
// BGMSinkParams.h
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>

//-----------------------------------------------------------------------------
// Name: class BGMSinkParams
// Desc: Shared parameters implementation.
//-----------------------------------------------------------------------------
class BGMSinkParams : public AK::IAkPluginParam
{
public:
	// Constructor/destructor.
	BGMSinkParams();
	~BGMSinkParams();
	BGMSinkParams(const BGMSinkParams & in_rCopy);

	// Create duplicate.
	IAkPluginParam * Clone(AK::IAkPluginMemAlloc * in_pAllocator);

	// Init/Term.
	AKRESULT Init(AK::IAkPluginMemAlloc *	in_pAllocator,
		const void *			in_pParamsBlock,
		AkUInt32				in_ulBlockSize
		);
	AKRESULT Term(AK::IAkPluginMemAlloc * in_pAllocator);

	// Blob set.
	AKRESULT SetParamsBlock(const void * in_pParamsBlock,
		AkUInt32 in_ulBlockSize
		);

	// Update one parameter.
	AKRESULT SetParam(AkPluginParamID in_ParamID,
		const void * in_pValue,
		AkUInt32 in_ulParamSize
		);

	bool m_bDVRRecordable;
};

