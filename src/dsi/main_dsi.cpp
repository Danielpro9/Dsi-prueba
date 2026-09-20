// DSi full-game entry point. Hardware bring-up lives in DsiBootstrap; game
// code only starts after it is ready. See src/dsi/tools/DsiBringup.cpp for
// the toolchain/hardware smoke test this is NOT -- that one is built
// instead of this file, never alongside it.
#ifdef DSI_PLATFORM

#include "platform/Log.h"
#include "client/Minecraft.h"
#include "java/String.h"
#include "dsi/system/DsiBootstrap.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	if (!DsiBootstrap::initialize())
		return 1;

	MC_LOG_INFO("dsi", "handing off to Minecraft::start()\n");
	jstring username = "Player";
	jstring auth = "-";
	Minecraft::start(&username, &auth);
	MC_LOG_INFO("dsi", "Minecraft::start returned; exiting to loader\n");

	DsiBootstrap::shutdown();
	return 0;
}

#endif
