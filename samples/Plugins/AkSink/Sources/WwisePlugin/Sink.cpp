/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Version: v2019.2.5  Build: 7349
  Copyright (c) 2006-2020 Audiokinetic Inc.
*******************************************************************************/

// Sink.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "Sink.h"
#include "SinkPlugin.h"
#include <AK/Wwise/Utilities.h>
#include <AK/Wwise/AudioPlugin.h>
#include <AK/Tools/Common/AkAssert.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <AK/Tools/Common/AkFNVHash.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

// CAkSinkApp
BEGIN_MESSAGE_MAP(CAkSinkApp, CWinApp)
END_MESSAGE_MAP()


// CAkSinkApp construction

CAkSinkApp::CAkSinkApp()
{

}

// The one and only CAkSinkApp object
CAkSinkApp theApp;

// CAkSinkApp initialization
BOOL CAkSinkApp::InitInstance()
{
	CWinApp::InitInstance();
	AK::Wwise::RegisterWwisePlugin();
	return TRUE;
}

AK::Wwise::IPluginBase* __stdcall AkCreatePlugin( unsigned short in_usCompanyID, unsigned short in_usPluginID )
{
	return new AkSinkPlugin();
}

/// Dummy assert hook for Wwise plug-ins using AKASSERT (cassert used by default).
DEFINEDUMMYASSERTHOOK;
DEFINE_PLUGIN_REGISTER_HOOK

bool FillDescription(IMMDevice *in_pEndpoint, AK::Wwise::OutputDeviceDescriptor &out_rDesc)
{
	LPWSTR pwszID = NULL;
	// Get the endpoint ID string.
	HRESULT hr = in_pEndpoint->GetId(&pwszID);
	if(hr != S_OK)
		return false;

	AK::FNVHash32 hash;
	char szString[1024];
	AKPLATFORM::AkWideCharToChar(pwszID, 1023, szString);
	out_rDesc.idDevice = hash.Compute(szString, static_cast<AkUInt32>(strlen(szString)));
	CoTaskMemFree(pwszID);

	IPropertyStore *pProps = NULL;
	hr = in_pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
	if(hr != S_OK)
		return false;

	// Initialize container for property value.
	PROPVARIANT varName;
	PropVariantInit(&varName);

	// Get the endpoint's friendly-name property.
	hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
	if(hr == S_OK)
	{
		wcscpy_s(out_rDesc.name, AK_MAX_OUTPUTDEVICEDESCRIPTOR, varName.pwszVal);
		PropVariantClear(&varName);
	}

	pProps->Release();
	return hr == S_OK;
}
