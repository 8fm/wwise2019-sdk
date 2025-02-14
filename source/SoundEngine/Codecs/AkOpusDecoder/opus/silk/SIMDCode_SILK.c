#include "config.h"

#if defined(OPUS_X86_MAY_HAVE_SSE)

#include "x86/x86_silk_map.c"

#elif defined(OPUS_ARM_MAY_HAVE_NEON_INTR)

#include "arm/LPC_inv_pred_gain_neon_intr.c"
#include "arm/NSQ_del_dec_neon_intr.c"
#include "arm/NSQ_neon.c"
#include "arm/biquad_alt_neon_intr.c"
#endif
