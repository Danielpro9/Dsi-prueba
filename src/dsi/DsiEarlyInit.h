#pragma once
#ifdef DSI_PLATFORM

#include <nds/ndstypes.h>

// Brings up video (top screen 3D via the libnds GL wrapper, bottom screen flat
// black), at most once. Safe to call again. See DsiEarlyVideo.cpp for what
// "flat black" means in terms of VRAM banks and display power.
void dsiEnsureEarlyVideo();

// Upper bound of the malloc heap right now, in bytes. Built against
// dsi_arm9.specs (see src/dsi/Makefile) and launched DSi-enhanced, this is the
// enforced 12 MB budget from DsiEarlyMemory.cpp (reduceHeapSize() caps it there
// even though the DSi genuinely has 16 MB -- see that file for why it is 12 and
// not the full 16). Built/launched as plain NDS-compatible instead, it is the
// natural ~3.5-4 MB ceiling.
u32 dsiGetHeapCeiling();

// Bytes currently committed to the malloc heap (getHeapEnd() - getHeapStart()).
u32 dsiGetHeapCommitted();

// Mounts the DSi's SD card and creates the save directory, at most once. Safe
// and cheap to call again; the first call does the work. Returns whether the
// card mounted and the save directory exists.
bool dsiEnsureStorage();

// Where world saves (and therefore streamed-out chunk region files -- see
// DsiWorldTuning.h) live: "sd:/OptiCraft". Always returns a usable path, even
// when nothing mounted, so callers get a path that fails to open rather than a
// null pointer to check. Calls dsiEnsureStorage() for you.
const char* dsiGetSaveDir();

// Reads the touch screen and updates the per-frame stylus-drag delta
// InputBackend_DSI.cpp's platformGamepadSnapshot() reports as the camera's
// "right stick". Must run exactly once per frame -- Display_dsi.cpp's
// processMessages() is the one call site, same place scanKeys() already runs
// once per frame for the same reason (see that function's own comment).
// Calling this more than once a frame, or from platformGamepadSnapshot()
// itself, would compute the delta against a position already moved past by
// an earlier call the same frame, undercounting fast drags.
void dsiUpdateTouchCameraDelta();

// Turns the L/R/A/X/Y/hotbar-chord action buttons into the same
// lwjgl::Keyboard/Mouse events a real keyboard/mouse press would queue (see
// InputBackend_DSI.cpp's header comment for the full button scheme). Must
// run once a frame, same place and for the same reason as
// dsiUpdateTouchCameraDelta() above.
void dsiPushGameplayKeyEvents();

#endif // DSI_PLATFORM
