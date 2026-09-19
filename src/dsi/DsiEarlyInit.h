#pragma once
#ifdef DSI_PLATFORM

#include <nds/ndstypes.h>

// Brings up video (top screen 3D via the libnds GL wrapper, bottom screen flat
// black), at most once. Safe to call again. See DsiEarlyVideo.cpp for what
// "flat black" means in terms of VRAM banks and display power.
void dsiEnsureEarlyVideo();

// Upper bound of the malloc heap right now, in bytes. Built against
// dsi_arm9.specs (see src/dsi/Makefile) and launched DSi-enhanced, this is the
// project brief's 8-9 MB budget, deliberately capped there by reduceHeapSize()
// even though the DSi has 16 MB -- see DsiEarlyMemory.cpp. Built/launched as
// plain NDS-compatible instead, it is the natural ~3.5-4 MB ceiling.
u32 dsiGetHeapCeiling();

// Bytes currently committed to the malloc heap (getHeapEnd() - getHeapStart()).
u32 dsiGetHeapCommitted();

#endif // DSI_PLATFORM
