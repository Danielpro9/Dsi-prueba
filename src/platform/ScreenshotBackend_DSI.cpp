#include "ScreenshotBackend.h"

#ifdef DSI_PLATFORM

namespace ScreenshotBackend
{
// PLATFORM_FRAMEBUFFER_READBACK is 0 for DSi (see PlatformConfig.h), so
// renderReadPixelsRgb() does not exist on this backend to call -- the DS 3D
// engine's render target is not simply memory-mappable the way a GX/GL
// framebuffer copy is, and nothing has implemented the capture path an
// F2 screenshot needs yet. Fails honestly instead of returning a blank image.
std::string save(const std::string&, int_t, int_t)
{
	return "Screenshots are not supported on DSi yet";
}
}

#endif // DSI_PLATFORM
