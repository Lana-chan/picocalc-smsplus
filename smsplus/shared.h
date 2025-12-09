#ifndef _SHARED_H_
#define _SHARED_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <malloc.h>
#include <math.h>
#include <limits.h>

#include "../sms_options.h"

#ifndef in_ram
#include <pico.h>
#define in_ram __not_in_flash_func
#endif

#if PICO_RP2040
	#undef FULL_FRAMEBUFFER
	#undef ENABLE_FM
#endif
#if PICO_RP2350
	#define FULL_FRAMEBUFFER 1
	#define ENABLE_FM 1
#endif

#ifndef PATH_MAX
#ifdef  MAX_PATH
#define PATH_MAX    MAX_PATH
#else
#define PATH_MAX    1024
#endif
#endif

#include "types.h"
#include "macros.h"
#include "z80.h"
#include "sms.h"
#include "pio.h"
#include "memz80.h"
#include "vdp.h"
#include "render.h"
#include "tms.h"
#include "sn76489.h"
#include "ym2413.h"
#include "fmintf.h"
#include "sound.h"
#include "system.h"
#include "error.h"

#include "loadrom.h"
#include "state.h"

#endif /* _SHARED_H_ */
