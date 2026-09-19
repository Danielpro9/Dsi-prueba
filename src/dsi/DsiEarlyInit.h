#pragma once
#ifdef DSI_PLATFORM

#include <nds/ndstypes.h>

// Brings up video (top screen 3D via the libnds GL wrapper, bottom screen flat
// black), at most once. Safe to call again. See DsiEarlyVideo.cpp for what
// "flat black" means in terms of VRAM banks and display power.
void dsiEnsureEarlyVideo();

// Upper bound of the malloc heap right now, in bytes. On the DSi this is the
// newlib sbrk ceiling (getHeapLimit()), which is ~3.5-4 MB unless the ROM is
// running DSi-enhanced with SCFG_EXT-granted access to the extended RAM region
// -- see the comment block in DsiEarlyMemory.cpp before assuming this reads
// close to the 8-9 MB target from the project brief.
u32 dsiGetHeapCeiling();

// Bytes currently committed to the malloc heap (getHeapEnd() - getHeapStart()).
u32 dsiGetHeapCommitted();

#endif // DSI_PLATFORM
