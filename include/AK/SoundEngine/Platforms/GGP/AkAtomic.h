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

// AkAtomic.h

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef volatile	int32_t	AkAtomic32;
typedef volatile	int64_t	AkAtomic64;
typedef volatile	void*	AkAtomicPtr;

#define						AK_ATOMIC_FENCE_FULL_BARRIER()												__sync_synchronize();

static inline	int32_t		AkAtomicLoad32( AkAtomic32* pSrc )											{ return *pSrc; }
static inline	void		AkAtomicStore32( AkAtomic32* pDest, int32_t value )							{ *pDest = value; }
static inline	int32_t		AkAtomicInc32( AkAtomic32* pValue )											{ return __sync_add_and_fetch( ( int32_t* )pValue, 1 ); }
static inline	int32_t		AkAtomicDec32( AkAtomic32* pValue )											{ return __sync_sub_and_fetch( ( int32_t* )pValue, 1 ); }
static inline	int32_t		AkAtomicExchange32( AkAtomic32* pDest, int32_t value )						{ return __sync_lock_test_and_set( ( int32_t* )pDest, value ); }
static inline	int32_t		AkAtomicAdd32( AkAtomic32* pDest, int32_t value )							{ return __sync_add_and_fetch( ( int32_t* )pDest, value ); }
static inline	int32_t		AkAtomicSub32( AkAtomic32* pDest, int32_t value )							{ return __sync_sub_and_fetch( ( int32_t* )pDest, value ); }
static inline	int			AkAtomicCas32( AkAtomic32* pDest, int32_t proposed, int32_t expected )		{ return __sync_bool_compare_and_swap( ( int32_t* )pDest, expected, proposed ); }

static inline	int64_t		AkAtomicLoad64( AkAtomic64* pSrc )											{ return *pSrc; }
static inline	void		AkAtomicStore64( AkAtomic64* pDest, int64_t value )							{ *pDest = value; }
static inline	int64_t		AkAtomicInc64( AkAtomic64* pValue )											{ return __sync_add_and_fetch( ( int64_t* )pValue, 1 ); }
static inline	int64_t		AkAtomicDec64( AkAtomic64* pValue )											{ return __sync_sub_and_fetch( ( int64_t* )pValue, 1 ); }
static inline	int64_t		AkAtomicExchange64( AkAtomic64* pDest, int64_t value )						{ return __sync_lock_test_and_set( ( int64_t* )pDest, value ); }
static inline	int64_t		AkAtomicAdd64( AkAtomic64* pDest, int64_t value )							{ return __sync_add_and_fetch( ( int64_t* )pDest, value ); }
static inline	int64_t		AkAtomicSub64( AkAtomic64* pDest, int64_t value )							{ return __sync_sub_and_fetch( ( int64_t* )pDest, value ); }
static inline	int			AkAtomicCas64( AkAtomic64* pDest, int64_t proposed, int64_t expected )		{ return __sync_bool_compare_and_swap( ( int64_t* )pDest, expected, proposed ); }

static inline	void*		AkAtomicLoadPtr( AkAtomicPtr* pSrc )										{ return ( void* )*pSrc; }
static inline	void		AkAtomicStorePtr( AkAtomicPtr* pDest, void* value )							{ *pDest = value; }
static inline	void*		AkAtomicExchangePtr( AkAtomicPtr* pDest, void* value )						{ return ( void* )__sync_lock_test_and_set( ( void** )pDest, value ); }
static inline	int			AkAtomicCasPtr( AkAtomicPtr* pDest, void* proposed, void* expected )		{ return __sync_bool_compare_and_swap( ( void** )pDest, expected, proposed ); }

#ifdef __cplusplus
}
#endif
