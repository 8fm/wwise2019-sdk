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

#pragma once

/// Defines the header of a block of markers.
struct AkMarkersHeader
{
	AkUInt32        uNumMarkers;            ///< Number of markers
};

/// Defines the parameters of a marker.
struct AkAudioMarker
{
	AkUInt32        dwIdentifier;           ///< Identifier.
	AkUInt32        dwPosition;             ///< Position in the audio data in sample frames.
	char*           strLabel;               ///< Label of the marker taken from the file.
};

/// A marker about to be notified
struct AkMarkerNotification
{
	AkAudioMarker marker;
	AkUInt32 uOffsetInBuffer;
	class CAkPBI * pCtx;
};

/// Range of marker notifications
struct AkMarkerNotificationRange
{
	AkMarkerNotification* pMarkers;                ///< Pointer to the first marker
	AkUInt32              uLength;                 ///< How many items in the range
};
