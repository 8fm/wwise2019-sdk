/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Copyright (c) 2020 Audiokinetic Inc.
*******************************************************************************/

// BaseMenuPage.cpp
/// \file 
/// Defines the methods declared in BaseMenuPage.h

#include"stdafx.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h>    // Sound engine

#include "BaseMenuPage.h"
#include "MenuPages.h"
#include "Menu.h"
#include "Platform.h"

/////////////////////////////////////////////////////////////////////
// BaseMenuPage Public Methods
/////////////////////////////////////////////////////////////////////

BaseMenuPage::BaseMenuPage( Menu& in_ParentMenu ):Page( in_ParentMenu, "Wwise Integration Demo" )
{
	m_szHelp  = "Welcome to the Wwise Integration Demonstration! This "
				"application contains samples that demonstrate how to "
				"integrate various Wwise features and concepts into a "
				"game.\n\n"
				"Navigation:\n"
				"  - UP / DOWN on the <<DIRECTIONAL_TYPE>>: move between items on the page.\n"
				"  - <<ACTION_BUTTON>>: activate the selected item.\n"
				"  - <<BACK_BUTTON>>: go back a page.\n\n"
				"Controls:\n"
				"  Certain controls (toggles, sliders) allow you to "
				"change values. Press LEFT/RIGHT on the <<DIRECTIONAL_TYPE>> "
				"to change these controls' values.\n\n"
				"Pressing <<HELP_BUTTON>> at any time will display help for the current page.\n";
}

bool BaseMenuPage::Init()
{
	// Load the Init sound bank
	// NOTE: The Init sound bank must be the first bank loaded by Wwise!
	AkBankID bankID;
	if ( AK::SoundEngine::LoadBank( "Init.bnk", bankID ) != AK_Success )
	{
		SetLoadFileErrorMessage( "Init.bnk" );
		return false;
	}

	// Initialize the page
	return Page::Init();
}

void BaseMenuPage::Release()
{
	// Unload the init soundbank
	AK::SoundEngine::UnloadBank( "Init.bnk", NULL );

	// Release the page
	Page::Release();
}

bool BaseMenuPage::Update()
{
	// Avoid returning false when the "back" button is pressed, but return false if an error occurs!
	Page::Update();
	return ! ErrorOccured();
}

void BaseMenuPage::InitControls()
{
	ButtonControl* newBtn;

#ifdef AK_PLATFORM_SUPPORTS_MOTION
	// Add button linking to Motion demo
	newBtn = new ButtonControl( *this );
	newBtn->SetLabel( "Motion Demo" );
	newBtn->SetDelegate( (PageMFP)&BaseMenuPage::OpenMotionDemoPage );
	m_Controls.push_back( newBtn );
#endif

	// Add an Exit button
	newBtn = new ButtonControl( *this );
	newBtn->SetLabel( "Exit" );
	newBtn->SetDelegate( (PageMFP)&BaseMenuPage::ExitButton_OnPress );
	m_Controls.push_back( newBtn );
}


/////////////////////////////////////////////////////////////////////
// BaseMenuPage Private Methods
/////////////////////////////////////////////////////////////////////

void BaseMenuPage::OpenMotionDemoPage( void*, ControlEvent* )
{
#ifdef AK_PLATFORM_SUPPORTS_MOTION
	DemoMotion* pg = new DemoMotion( *m_pParentMenu );
	m_pParentMenu->StackPage( pg );
#endif
}

void BaseMenuPage::ExitButton_OnPress(void*, ControlEvent*)
{
	m_pParentMenu->QuitApplication();
}
