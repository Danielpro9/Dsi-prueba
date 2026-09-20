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

namespace
{
bool g_created = false;
bool g_closeRequested = false;
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
