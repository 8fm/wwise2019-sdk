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

// BaseMenuPage.h
/// \file 
/// Defines the Base page that is loaded by the menu system when it is created.

#pragma once

#include "Page.h"


/// This class defines the first page that the Menu system automatically loads
/// when the Menu class is instantiated.
class BaseMenuPage : public Page
{
public:

	/// BaseMenuPage class constructor
	BaseMenuPage( Menu& in_ParentMenu );

	/// Initializes the demo.
	/// \return True if successful and False otherwise.
	virtual bool Init();

	/// Releases resources used by the demo.
	virtual void Release();

	/// Override of the parent's update method.
	virtual bool Update();

private:

	/// Initializes the various components on the page.
	virtual void InitControls();

	/// Delegate function for the "Motion Demo" button.
	void OpenMotionDemoPage( void* in_pSender, ControlEvent* in_pEvent );

	/// Delegate function for the "Exit" button.
	void ExitButton_OnPress( void* in_pSender, ControlEvent* in_pEvent );
};
