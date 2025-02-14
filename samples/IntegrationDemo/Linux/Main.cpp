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

// main.cpp
/// \file 
/// Contains the entry point for the application.

/////////////////////////
//  INCLUDES
/////////////////////////

#include "stdafx.h"

#include <malloc.h>
#include "Helpers.h"
#include "IntegrationDemo.h"
#include "Platform.h"

#include <SDL2/SDL.h>

int main( int argc, char *argv[] )
{
	AkMemSettings memSettings;
	AkStreamMgrSettings stmSettings;
	AkDeviceSettings deviceSettings;
	AkInitSettings initSettings;
	AkPlatformInitSettings platformInitSettings;
	AkMusicSettings musicInit;
	IntegrationDemo::Instance().GetDefaultSettings(memSettings, stmSettings, deviceSettings, initSettings, platformInitSettings, musicInit);

	// Initialize the various components of the application and show the window
	int width = 853, height = 480;
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* win = SDL_CreateWindow("Integration Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);
	if (win == NULL)
	{
		printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
		return 1;
	}

	AkOSChar szError[500];
	if ( !IntegrationDemo::Instance().Init( memSettings, stmSettings, deviceSettings, initSettings, platformInitSettings, musicInit, (void*)(win), szError, IntegrationDemoHelpers::AK_ARRAYSIZE(szError), width, height ) )
	{
		printf( AKTEXT("Failed to initialize the Integration Demo.\r\n%s"), szError );
		return 1;
	}


	// The application's main loop
	// Each iteration represents 1 frame
	while ( true )
	{
		// Records the starting time of the current frame
		IntegrationDemo::Instance().StartFrame();

		// Process the current frame, quit if Update() is false.
		if ( ! IntegrationDemo::Instance().Update() )
		{
			break;
		}
		IntegrationDemo::Instance().Render();
		
		// Ends the frame and regulates the application's framerate
		IntegrationDemo::Instance().EndFrame();
	}

	// Terminate the various components of the application
	IntegrationDemo::Instance().Term();

	return 0;
}

