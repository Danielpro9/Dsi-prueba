#include "platform/ClientPlatformPolicy.h"

#ifdef DSI_PLATFORM

#include "platform/Log.h"
#include "net/minecraft/src/GameSettings.h"
#include "dsi/DsiEarlyInit.h"

namespace ClientPlatformPolicy
{
int initialWidth()
{
	return 256; // DS/DSi screen resolution -- see DsiEarlyVideo.cpp's glViewport().
}

int initialHeight()
{
	return 192;
}

std::string minecraftDirectory()
{
	return dsiGetSaveDir();
}

bool saveConverterUsesSavesSubdirectory()
{
	return true;
}

// Same simplifications PS2 applies for the same reason: an equally
// memory/GPU-constrained fixed-function 3D engine gains nothing from
// animating water/lava/fire/portal/redstone/explosion/flame/smoke textures,
// and neither "advanced" GL features nor fancy occlusion exist on this
// hardware to enable.
void applyGameSettingsDefaults(GameSettings* settings)
{
	if (settings == nullptr)
		return;

	settings->ofAnimatedWater = 2;
	settings->ofAnimatedLava = 2;
	settings->ofAnimatedFire = false;
	settings->ofAnimatedPortal = false;
	settings->ofAnimatedRedstone = false;
	settings->ofAnimatedExplosion = false;
	settings->ofAnimatedFlame = false;
	settings->ofAnimatedSmoke = false;
	settings->advancedOpengl = false;
	settings->ofOcclusionFancy = false;
}

// Left as a no-op for now rather than guessed at: preloading/releasing
// specific textures is a real-hardware memory-pressure optimisation (see the
// PS2/Wii comments on their equivalents), and nothing has been measured yet
// on this backend to say it is needed -- see DsiEarlyMemory.cpp for the
// running theme of "measure before tuning" on this port.
void preloadStartupTextures(RenderEngine*)
{
}

void releaseWorldEntryAssets(RenderEngine*)
{
}

int panoramaSampleGrid()
{
	// 1: a single, unblurred draw per cube face (6 quads/frame total), not
	// PS2's already-reduced 2x2 grid (24 quads/frame). Requested directly
	// after a real-hardware report of severe main-menu lag (menu barely
	// responding to input) -- the sampleGrid loop in GuiMainMenu.cpp's
	// drawPanorama() is a deliberate motion-blur-style effect: N*N samples
	// per face, each a separate alpha-blended draw, stacked to fake a soft
	// trail as the cube rotates. Translucent-polygon rendering is one of
	// the more expensive, easier-to-misconfigure paths on the DS's fixed-
	// function GPU (see RenderAPI_DSI.cpp's own POLY_ALPHA/blending notes),
	// so 24 blended draws a frame just for the background -- before the
	// menu buttons, text, or anything else -- is a real, plausible cost on
	// this hardware, not just PS2's GS. Dropping to a single opaque draw
	// per face removes both the blend stacking AND 18 of the 24 draws.
	return 1;
}

void reportCrash(const std::string& description)
{
	MC_LOG_ERROR("crash", "%s\n", description.c_str());
}
}

#endif // DSI_PLATFORM
