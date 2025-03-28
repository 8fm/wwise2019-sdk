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
  Copyright (c) 2006-2019 Audiokinetic Inc.
*******************************************************************************/

// AkTls.cpp -- "C" wrappers for Win32 tls functions

#include "stdafx.h"

#include <AK/Tools/Common/AkTls.h>

#include <windows.h>

int AkTlsAllocateSlot(AkTlsSlot * out_pSlot)
{
    *out_pSlot = ::TlsAlloc();
    return *out_pSlot == TLS_OUT_OF_INDEXES ? -1 : 0;
}

void AkTlsFreeSlot(AkTlsSlot in_slot)
{
    ::TlsFree(in_slot);
}

uintptr_t AkTlsGetValue(AkTlsSlot in_slot)
{
    return (uintptr_t) ::TlsGetValue(in_slot);
}

void AkTlsSetValue(AkTlsSlot in_slot, uintptr_t in_value)
{
    ::TlsSetValue(in_slot, (LPVOID) in_value);
}
