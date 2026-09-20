// Display_dsi.cpp — DSi implementation of lwjgl::Display.
//
// Thin wrapper, same shape as Display_wii.cpp: DsiEarlyVideo.cpp owns the
// actual video/GL setup. What lives here is the lwjgl-shaped surface plus:
//
//   * "Close requested" maps to START, the same button every DsiBringup.cpp
//     demo already uses to exit. There is no real menu/HOME-button affordance
//     on this console the way Wii has, and no alternative has been designed
//     yet -- this is a placeholder good enough to let the game loop exit
//     cleanly, not a considered control choice.
//
//   * The video mode is not a real choice: the DS/DSi top screen is always
//     256x192, so setDisplayMode() has nothing to apply.
#ifdef DSI_PLATFORM

#include "lwjgl/Display.h"

#include <nds.h>

#include "dsi/DsiEarlyInit.h"
#include "platform/Log.h"

namespace
{
bool g_created = false;
bool g_closeRequested = false;

// No debugger and no serial console on real hardware, and a hang leaves
// nothing else on screen to say the game is still alive -- the difference
// between "drawing the same frame forever" and "stuck in an infinite loop
// with vblank never reached again" is otherwise invisible from outside the
// console. This writes one line to sd:/OptiCraft/debug.log (see
// DsiEarlyStorage.cpp) roughly once a second; if that line stops advancing,
// whatever ran between it and the next one is where things stopped. Cheap
// enough to leave on permanently: MC_LOG_SYNC_WRITES commits it immediately
// either way (see Log.h), so this adds one open/write/flush per ~60 frames,
// not per frame.
void heartbeat()
{
	static unsigned int frame = 0;
	++frame;
	if (frame % 60 != 0)
		return;
	MC_LOG_INFO("dsi", "heartbeat frame=%u heap=%u/%uKB\n",
		frame, (unsigned)(dsiGetHeapCommitted() / 1024u), (unsigned)(dsiGetHeapCeiling() / 1024u));
}
}

namespace lwjgl
{
namespace Display
{

void create()
{
	if (g_created)
		return;

	dsiEnsureEarlyVideo();
	g_created = true;
}

void setDisplayMode(const DisplayMode&)
{
	// The screen resolution is fixed. See the header comment.
}

DisplayMode getDisplayMode()
{
	return DisplayMode(getWidth(), getHeight());
}

void setTitle(const jstring&) {}
void setFullscreen(bool) {}

bool isCloseRequested() { return g_closeRequested; }
bool isVisible() { return true; }
bool isActive() { return true; }

// No real input backend yet (see InputBackend_DSI.cpp -- controls are
// deferred), so this only does the one thing needed to let the game loop
// exit: reads START directly through libnds rather than through
// platform/Input.h's snapshot, which reports nothing yet.
void processMessages()
{
	scanKeys();
	if (keysDown() & KEY_START)
		g_closeRequested = true;
}

void swapBuffers()
{
	glFlush(0); // waits for vblank and swaps
	heartbeat();
}

void update(bool doProcessMessages)
{
	swapBuffers();
	if (doProcessMessages)
		processMessages();
}

int_t getX() { return 0; }
int_t getY() { return 0; }
int_t getWidth() { return 256; }
int_t getHeight() { return 192; }

} // namespace Display
} // namespace lwjgl

#endif // DSI_PLATFORM
