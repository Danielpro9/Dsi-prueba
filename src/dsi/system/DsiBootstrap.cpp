#ifdef DSI_PLATFORM
#include "platform/Log.h"
#include "dsi/system/DsiBootstrap.h"

#include "lwjgl/Display.h"
#include "dsi/DsiEarlyInit.h"

#include <nds.h>

// Unlike WiiBootstrap::initialize(), this does not check for a staged data/
// tree yet: there is no asset packaging story decided for DSi (NitroFS vs.
// the SD card -- see the comment in Resources_DSI.cpp), so there is nothing
// concrete to check for and no UI screen to report it on if the check failed
// -- the real game's bottom screen is meant to stay flat black (see
// DsiEarlyVideo.cpp), which rules out the kind of text screen
// WiiBootstrap.cpp's showMissingAssetsScreen() draws. Revisit once that
// packaging exists.
namespace DsiBootstrap
{

bool initialize()
{
	MC_LOG_INFO("dsi", "main() entered\n");

	// DSi-exclusive: doubles the ARM9 clock from the DS-compatible 67.03MHz
	// to 134.06MHz (setCpuClock()'s own doc comment, nds/system.h) -- pure
	// upside for everything CPU-bound in this port (world/entity logic,
	// Tessellator vertex prep, the whole game loop other than the GPU's own
	// fixed clock), and left unclaimed until now even though the *other*
	// DSi-only bonus, the extended 12MB heap, was already being used (see
	// DsiEarlyMemory.cpp). Guarded by isDSiMode() the same way that heap
	// budget is: only DSi-enhanced (TWL) mode has this clock to unlock, and
	// whether that's actually how the .nds got launched depends on the
	// loader, not just how it was compiled (see DsiEarlyMemory.cpp's own
	// comment on this). On a plain NDS-compatible launch this is a no-op at
	// the DS-native 67MHz, same as every other DSi-only feature in this port.
	if (isDSiMode())
		setCpuClock(true);

	if (!dsiEnsureStorage())
	{
		MC_LOG_ERROR("dsi", "*** no SD card -- cannot continue ***\n");
		return false;
	}

	MC_LOG_INFO("dsi", "save dir: %s\n", dsiGetSaveDir());
	MC_LOG_INFO("dsi", "starting display/render...\n");
	lwjgl::Display::create();
	return true;
}

void shutdown()
{
}

}
#endif
