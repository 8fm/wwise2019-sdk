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
////////////////////////////////////////////////////////////////////////
// DlgPerforceConfig.h
//
// Header file for the CDlgPerforceConfig dialog, used to let the user
// enter a submit descripton.
//
///////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/Wwise/SourceControl/ISourceControlDialogBase.h>
#include <AK/Wwise/SourceControl/ISourceControlUtilities.h>

using namespace AK;
using namespace Wwise;

// CDlgPerforceConfig dialog
class CDlgPerforceConfig
	: public ISourceControlDialogBase
{
public:
	// Constructor
	CDlgPerforceConfig( ISourceControlUtilities* in_pUtilities );

	// Destructor
	virtual ~CDlgPerforceConfig();

	// ISourceControlDialogBase
	virtual HINSTANCE GetResourceHandle() const;
	virtual void GetDialog( UINT & out_uiDialogID ) const;
	virtual bool HasHelp() const;
	virtual bool Help( HWND in_hWnd ) const;
	virtual bool WindowProc( HWND in_hWnd, UINT in_message, WPARAM in_wParam, LPARAM in_lParam, LRESULT & out_lResult );

private:

	// Overrides
	void OnInitDialog( HWND in_hWnd );

	bool OnBnClickedOk( HWND in_hWnd );
	bool OnBnClickedDiffBrowse( HWND in_hWnd );

	// Data
	ISourceControlUtilities* m_pUtilities;
};
