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

#include "stdafx.h"
#include "DemoOptionsUtil.h"

#include <AK/SoundEngine/Common/AkSpeakerConfig.h>

namespace demoOptionsUtil
{
	void PopulateOutputSpeakerOptions(ToggleControl& out_toggle)
	{
		out_toggle.AddOption("Automatic", (void*)0);
		out_toggle.AddOption("Stereo", (void*)AK_SPEAKER_SETUP_STEREO);
		out_toggle.AddOption("5.1 Surround", (void*)AK_SPEAKER_SETUP_5POINT1);
	}
}