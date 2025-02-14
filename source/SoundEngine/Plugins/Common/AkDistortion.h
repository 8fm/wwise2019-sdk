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

#ifndef _AK_DISTORTION_H
#define _AK_DISTORTION_H

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>

namespace DSP
{
	class CAkDistortion
	{
	public:
		enum DistortionType
		{
			DistortionType_None = 0,
			DistortionType_Overdrive,
			DistortionType_Heavy,
			DistortionType_Fuzz,
			DistortionType_Clip
		};

		CAkDistortion();
		void SetParameters( DistortionType in_eDistortionType, AkReal32 in_fDrive, AkReal32 in_fTone, bool in_bFirstSet = false );

		// All channels
		void ProcessBuffer(	AkAudioBuffer * io_pBuffer );

	private:

		void ProcessOverdrive( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames );
		void ProcessHeavy( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames );
		void ProcessFuzz( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames );
		void ProcessClip( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames );

	
		DistortionType	m_eDistortionType;
		AkReal32		m_fDrive;
		AkReal32		m_fPrevDrive;
		AkReal32		m_fDriveGain;
		AkReal32		m_fPrevDriveGain;
		AkReal32		m_fTone;
		AkReal32		m_fPrevTone;
	};

} // namespace DSP
#endif // _AK_DISTORTION_H