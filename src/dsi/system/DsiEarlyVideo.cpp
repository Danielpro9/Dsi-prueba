#ifdef DSI_PLATFORM

#include "platform/Log.h"
#include "dsi/DsiEarlyInit.h"

#include <nds.h>

namespace
{
bool g_videoReady = false;
}

// Top screen: 3D engine only, driving the game's render output. Bottom screen:
// no background layer, no sprites, no console -- the sub 2D engine has nothing
// to composite, which reads as flat black on real hardware and in an emulator.
// This is the PS2-derived split carried over as-is: the PS2 port never spent
// GS/VU time on a second view either, and here that idle screen is free
// instead of merely cheap.
void dsiEnsureEarlyVideo()
{
	if (g_videoReady)
		return;
	g_videoReady = true;

	// Not video-related, but this is the one function every DSi entry point
	// calls first (see DsiBringup.cpp), so it is the natural place for other
	// one-time hardware setup that has to happen before anything measures
	// time. platform/PlatformCompat.h's DSi getMonotonicMicros()/getTicks()
	// read this counter and silently return garbage before it is set up.
	systemCounterSetup();

	powerOn(POWER_ALL);

	videoSetMode(MODE_0_3D);
	videoSetModeSub(MODE_0_2D);

	// VRAM_A-D as texture banks is the standard GL allocation (128 KB each,
	// 512 KB of texture space total); nothing is assigned to the sub engine, so
	// no bank feeds the bottom screen. MODE_0_2D on its own draws nothing --
	// dsiEnsureEarlyVideo() never calls bgInitSub()/bgInit() for any layer -- so
	// the sub engine has no background to composite and reads as flat black.
	vramSetBankA(VRAM_A_TEXTURE);
	vramSetBankB(VRAM_B_TEXTURE);
	vramSetBankC(VRAM_C_TEXTURE);
	vramSetBankD(VRAM_D_TEXTURE);

	// Texture PALETTE memory -- a separate pool from the image data banks
	// above, required for RenderAPI_DSI.cpp's paletted (GL_RGB256) texture
	// upload path: glColorTableNtr() writes the palette here, and without a
	// bank mapped for it that call has nowhere to put the data. 64 KB is far
	// more than this port needs (each GL_RGB256 palette is at most 512
	// bytes -- 256 colours * 2 bytes -- so this covers well over a hundred
	// distinct paletted textures resident at once, when only a handful ever
	// will be).
	vramSetBankE(VRAM_E_TEX_PALETTE);

	glInit();
	glEnable(GL_TEXTURE_2D);
	glClearColor(0, 0, 0, 31);
	glClearDepth(GL_MAX_DEPTH);
	glViewport(0, 0, 255, 191);

	MC_LOG_INFO("dsi", "OptiCraft - DSi\n");
	MC_LOG_INFO("dsi", "video ok: top=3D (256x192) bottom=black\n");
}

#endif // DSI_PLATFORM
