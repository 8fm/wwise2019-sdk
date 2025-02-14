/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#ifndef USE_PIX
	#define USE_PIX
#endif

#include <pix.h>

class AkInstrumentScope
{
public:
	inline AkInstrumentScope( PCWSTR in_pszZoneName ) 
	{
		PIXBeginEvent( 0, in_pszZoneName );
	}

	inline ~AkInstrumentScope()
	{
		PIXEndEvent();
	}
};

#define AK_PIX_COLOR( __r__, __g__, __b__ ) PIX_COLOR( __r__, __g__, __b__ )
#define AK_PIX_SET_MARKER_HOOK( _pixColor_, _szMsg_, _ctxId_, _voiceCnt_ ) PIXSetMarker( _pixColor_, L##_szMsg_, _ctxId_, _voiceCnt_ )

#define AK_INSTRUMENT_BEGIN( _zone_name_ ) PIXBeginEvent(0, L##_zone_name_)
#define AK_INSTRUMENT_BEGIN_C( _color_, _zone_name_ ) PIXBeginEvent(_color_, L##_zone_name_)
#define AK_INSTRUMENT_END( _zone_name_ ) PIXEndEvent()
#define AK_INSTRUMENT_SCOPE( _zone_name_ ) AkInstrumentScope akInstrumentScope_##__LINE__(L##_zone_name_)

#define AK_INSTRUMENT_IDLE_BEGIN( _zone_name_ )
#define AK_INSTRUMENT_IDLE_END( _zone_name_ )
#define AK_INSTRUMENT_IDLE_SCOPE( _zone_name_ )

#define AK_INSTRUMENT_STALL_BEGIN( _zone_name_ )
#define AK_INSTRUMENT_STALL_END( _zone_name_ )
#define AK_INSTRUMENT_STALL_SCOPE( _zone_name_ )

#define AK_INSTRUMENT_THREAD_START( _thread_name_ )
