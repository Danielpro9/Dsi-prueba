#ifdef DSI_PLATFORM
#include "platform/Log.h"
#include "dsi/system/DsiBootstrap.h"

#include "lwjgl/Display.h"
#include "dsi/DsiEarlyInit.h"

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
