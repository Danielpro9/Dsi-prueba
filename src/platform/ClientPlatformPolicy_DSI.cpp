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
	return 2; // Same reduced grid PS2 uses: less texture memory for the menu background.
}

void reportCrash(const std::string& description)
{
	MC_LOG_ERROR("crash", "%s\n", description.c_str());
}
}

#endif // DSI_PLATFORM
