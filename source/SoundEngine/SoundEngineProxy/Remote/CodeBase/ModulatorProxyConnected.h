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

#ifndef AK_OPTIMIZED

#include "ObjectProxyConnected.h"
#include "AkModulator.h"

class ModulatorProxyConnected : public ObjectProxyConnected
{
public:
	ModulatorProxyConnected( AkUniqueID in_id, AkModulatorType in_type );
	virtual ~ModulatorProxyConnected();

	virtual void HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer );

private:
	DECLARE_BASECLASS( ObjectProxyConnected );
};

class ModulatorLfoProxyConnected : public ModulatorProxyConnected
{
public:
	ModulatorLfoProxyConnected( AkUniqueID in_id );
	virtual ~ModulatorLfoProxyConnected();

private:
	DECLARE_BASECLASS( ModulatorProxyConnected );
};

class ModulatorEnvelopeProxyConnected : public ModulatorProxyConnected
{
public:
	ModulatorEnvelopeProxyConnected( AkUniqueID in_id );
	virtual ~ModulatorEnvelopeProxyConnected();

private:
	DECLARE_BASECLASS( ModulatorProxyConnected );
};

class ModulatorTimeProxyConnected : public ModulatorProxyConnected
{
public:
	ModulatorTimeProxyConnected( AkUniqueID in_id );
	virtual ~ModulatorTimeProxyConnected();

private:
	DECLARE_BASECLASS(ModulatorProxyConnected);
};

#endif // #ifndef AK_OPTIMIZED
