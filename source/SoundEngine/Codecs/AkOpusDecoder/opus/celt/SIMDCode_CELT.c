#include "config.h"

#if defined(OPUS_X86_MAY_HAVE_SSE)

#include "x86/pitch_sse.c"
#include "x86/pitch_sse2.c"
#include "x86/vq_sse2.c"
#include "x86/x86_celt_map.c"
#include "x86/x86cpu.c"

#elif defined(OPUS_ARM_MAY_HAVE_NEON_INTR)

#include "arm/arm_celt_map.c"
#include "arm/armcpu.c"
#include "arm/celt_neon_intr.c"
#include "arm/pitch_neon_intr.c"

#endif
