// Resource_dsi.cpp — DSi implementation of Resource::getResource().
// Copied from src/ps2/java/Resource_ps2.cpp: same reasoning applies verbatim
// (GameResources::open() already resolves under PlatformResources, which
// Resources_DSI.cpp implements over the SD card -- see that file).
#ifdef DSI_PLATFORM

#include "java/Resource.h"
#include "java/String.h"
#include "net/minecraft/src/GameResources.h"

#include <stdexcept>
#include <string>

namespace Resource
{

std::istream* getResource(const jstring& name)
{
	auto input = GameResources::open(static_cast<const std::string&>(name));
	if (input)
		return input.release();

	throw std::runtime_error("Failed to open resource " + static_cast<const std::string&>(name));
}

} // namespace Resource

#endif // DSI_PLATFORM
