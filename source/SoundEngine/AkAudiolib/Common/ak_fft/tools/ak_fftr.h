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

#ifndef AK_FFTR_H
#define AK_FFTR_H

#include <AK/SoundEngine/Common/AkTypes.h>
#include "ak_fft.h"
//#ifdef __cplusplus
//extern "C" {
//#endif

    
/* 
    
 Real optimized version can save about 45% cpu time vs. complex fft of a real seq.

 
 
 */

typedef struct ak_fftr_state *ak_fftr_cfg;

AK_ALIGN_SIMD(
struct ak_fftr_state
{
    ak_fft_cfg substate;
    ak_fft_cpx * tmpbuf;
    ak_fft_cpx * super_twiddles;
}
);

namespace DSP
{

namespace BUTTERFLYSET_NAMESPACE
{

ak_fftr_cfg ak_fftr_alloc(int nfft,int inverse_fft,void * mem, size_t * lenmem);
/*
 nfft must be even

 If you don't care to allocate space, use mem = lenmem = NULL 
*/


void ak_fftr(ak_fftr_cfg cfg,const ak_fft_scalar *timedata,ak_fft_cpx *freqdata);
/*
 input timedata has nfft scalar points
 output freqdata has nfft/2+1 complex points
*/

void ak_fftri(ak_fftr_cfg cfg,const ak_fft_cpx *freqdata,ak_fft_scalar *timedata);
/*
 input freqdata has  nfft/2+1 complex points
 output timedata has nfft scalar points
*/

} // namespace BUTTERFLYSET_NAMESPACE

} // namespace DSP

#define ak_fftr_free free

//#ifdef __cplusplus
//}
//#endif

#endif // AK_FFTR_H
