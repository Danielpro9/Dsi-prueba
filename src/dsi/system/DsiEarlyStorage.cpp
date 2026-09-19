#ifdef DSI_PLATFORM

#include "platform/Log.h"
#include "platform/storage/PosixFileSystem.h"
#include "dsi/DsiEarlyInit.h"

#include <cstdio>
#include <fat.h>

namespace
{
bool g_storageTried = false;
bool g_storageReady = false;
// "sd:/" is the DSi's own SD slot (docs/guides/filesystem.md section 4 in the
// BlocksDS repo), which is what the project brief means by "tarjeta MicroSD" --
// not "fat:/", which is a Slot-1/Slot-2 flashcart's DLDI-patched card. If this
// ever needs to run from a flashcart too, this is the constant to make a
// runtime choice instead of a fixed prefix.
const char* const kSaveDir = "sd:/OptiCraft";
}

bool dsiEnsureStorage()
{
	if (g_storageTried)
		return g_storageReady;
	g_storageTried = true;

	MC_LOG_INFO("dsi", "mounting sd card...\n");
	g_storageReady = fatInitDefault();
	if (!g_storageReady)
	{
		MC_LOG_WARN("dsi", "*** fatInitDefault() FAILED -- no SD card ***\n");
		return g_storageReady;
	}

	if (!PlatformStorage::makeDirectories(kSaveDir))
	{
		MC_LOG_WARN("dsi", "could not create %s\n", kSaveDir);
		g_storageReady = false;
		return g_storageReady;
	}

	if (!McLog::openSessionFile(kSaveDir))
		MC_LOG_WARN("dsi", "could not create debug.log in %s\n", kSaveDir);
	MC_LOG_INFO("dsi", "sd card mounted, save dir %s\n", kSaveDir);
	return g_storageReady;
}

const char* dsiGetSaveDir()
{
	dsiEnsureStorage();
	return kSaveDir;
}

#endif // DSI_PLATFORM
