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

#include "stdafx.h"
#include "AkChildCtx.h"
#include "AkMusicCtx.h"

CAkChildCtx::CAkChildCtx( CAkMusicCtx * in_pParentCtx )
    :m_pParentCtx( in_pParentCtx )
{
}


CAkChildCtx::~CAkChildCtx()
{
}

void CAkChildCtx::Connect()
{
    AKASSERT( m_pParentCtx );
    m_pParentCtx->AddChild( this );
}

void CAkChildCtx::Disconnect()
{
    AKASSERT( m_pParentCtx );
    m_pParentCtx->RemoveChild( this );
}
