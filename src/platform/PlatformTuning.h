#pragma once

#include "platform/PlatformConfig.h"

// DSi shares this table too: PLATFORM_CONSOLE_LOW is on for it (see
// PlatformConfig.h -- no hardware FPU), and PlatformGameTuning.h's "#if
// PLATFORM_PS2" CPU-shortcut branch below covers both consoles for the same
// reason. Ps2Tuning.h is pure #define tables (no PS2 hardware headers), so
// pulling it into a DSi/ARM9 build is safe.
#if PLATFORM_PS2 || PLATFORM_DSI
#include "ps2/render/Ps2Tuning.h"
#endif

#include "platform/tuning/PlatformInputTuning.h"
#include "platform/tuning/PlatformGameTuning.h"
#include "platform/tuning/PlatformWiiTuning.h"
#include "platform/tuning/PlatformDsiTuning.h"
#include "platform/tuning/PlatformClientTuning.h"
#include "platform/tuning/PlatformAsyncTuning.h"
