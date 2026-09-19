#include "platform/Resources.h"

#ifdef DSI_PLATFORM

#include "dsi/DsiEarlyInit.h"

#include <cstdlib>
#include <fstream>

// Resolves under sd:/OptiCraft (DsiEarlyStorage.cpp) for now, matching where
// world saves already live. NitroFS ("nitro:/", read-only, bundled inside the
// .nds itself -- see docs/guides/filesystem.md in the BlocksDS repo) is the
// natural home for shipped assets once src/dsi/Makefile actually stages a
// NITROFSDIR; nothing does that yet (see the Makefile's own note on this),
// so there is nothing to prefer it over the SD card for yet.
std::string PlatformResources::baseDir()
{
	return dsiGetSaveDir();
}

std::string PlatformResources::assetsDir()
{
	return baseDir() + "/data/assets";
}

std::string PlatformResources::audioDir()
{
	return baseDir() + "/data/resources";
}

std::string PlatformResources::resolveExisting(const std::string& path)
{
	std::string resolved;
	if (path.rfind("assets/", 0) == 0)
		resolved = assetsDir() + "/" + path.substr(7);
	else
		resolved = baseDir() + "/" + path;

	std::ifstream file(resolved, std::ios::binary);
	return file.good() ? resolved : std::string();
}

std::string PlatformResources::resolveAsset(const std::string& input)
{
	std::string path = input;
	if (!path.empty() && path[0] == '/')
		path.erase(path.begin());
	return resolveExisting("assets/" + path);
}

long PlatformResources::fileSize(const std::string& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	return file ? static_cast<long>(file.tellg()) : -1L;
}

unsigned char* PlatformResources::loadFile(const std::string& path, unsigned int* outSize)
{
	if (outSize)
		*outSize = 0;
	const long size = fileSize(path);
	if (size <= 0)
		return nullptr;

	unsigned char* data = static_cast<unsigned char*>(std::malloc(static_cast<std::size_t>(size)));
	if (!data)
		return nullptr;

	std::ifstream file(path, std::ios::binary);
	if (!file.read(reinterpret_cast<char*>(data), size))
	{
		std::free(data);
		return nullptr;
	}
	if (outSize)
		*outSize = static_cast<unsigned int>(size);
	return data;
}

#endif // DSI_PLATFORM
