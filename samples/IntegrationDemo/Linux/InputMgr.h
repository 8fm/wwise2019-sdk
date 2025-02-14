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

// InputMgr.h
/// \file 

#pragma once

#include "UniversalInput.h"

#include <SDL2/SDL.h>
#include <vector>

class UniversalInput;

class InputMgr
{
public:

	/// InputMgr class destructor
	~InputMgr();

	/// Initializes the Input Manager object.
	/// \return True if the Input Manager has been properly initialized and false otherwise.
	bool Init( 
		void* in_pParam,						///< - Extra parameter, in this case for the application's main window handle
		AkOSChar* in_szErrorBuffer,				///< - Buffer where error details will be written (if the function returns false)
		unsigned int in_unErrorBufferCharCount	///< - Number of characters available in in_szErrorBuffer, including terminating NULL character
		);

	/// Accessor method for the UniversalInput object.
	/// \return A pointer to the UniversalInput object being used by the Input Manager.
	UniversalInput* UniversalInputAdapter() const;

	/// Updates the input from all devices and relays the new input states to the Menu system.
	/// \remark This method should be called every frame before the Menu system's Update call.
	void Update();

	/// Ends the Input Manager class and releases all used resources.
	void Release();
	
	/// Flush all Inputs
	void Flush();

private:
	UGBtnState ScanCodeToBtnState( SDL_Scancode in_scanCode ) const;

	/// Pointer to the Universal Input object to interface with the Menu System
	/// \sa UniversalInput
	UniversalInput* m_pUInput;

	bool m_bDidMotion;

	// Keyboard input history; entry 0 is the most recent
	UGBtnState m_btnKeyboard;
};
