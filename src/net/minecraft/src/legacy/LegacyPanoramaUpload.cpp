#include "LegacyPanoramaUpload.h"

#include <algorithm>
#include <utility>

#include "java/BufferedImage.h"
#include "java/Type.h"
#include "platform/Log.h"
#include "LegacyUiPolicy.h"

constexpr int_t LEGACY_PANORAMA_PS2_MAX_WIDTH = 512;
constexpr int_t LEGACY_PANORAMA_WII_MAX_WIDTH = 1024;

namespace
{

// Shared bilinear resample, factored out of the old single PS2/WII-only path
// below so the DSi power-of-two path (which needs a different width AND
// height, not just a width clamp) can reuse it instead of duplicating the
// loop. 16.16 fixed point keeps this one-time console path cheap and avoids
// introducing another image-resize dependency. Releases the source pixels
// as soon as the resized image is returned, before setupTexture() allocates
// its own upload staging buffer.
std::unique_ptr<BufferedImage> resamplePanoramaBilinear(
	const BufferedImage &image, int_t targetWidth, int_t targetHeight)
{
	const int_t sourceWidth = image.getWidth();
	const int_t sourceHeight = image.getHeight();
	const unsigned char *src = image.getRawPixels();
	std::unique_ptr<unsigned char[]> dst(
		new unsigned char[BufferedImage::checkedRgbaByteCount(targetWidth, targetHeight)]);

	for (int_t y = 0; y < targetHeight; ++y)
	{
		const unsigned int yFixed = targetHeight > 1
			? static_cast<unsigned int>((static_cast<unsigned long long>(y) *
				static_cast<unsigned long long>(sourceHeight - 1) << 16) /
				static_cast<unsigned int>(targetHeight - 1))
			: 0u;
		const int_t y0 = static_cast<int_t>(yFixed >> 16);
		const int_t y1 = std::min<int_t>(y0 + 1, sourceHeight - 1);
		const unsigned int fy = yFixed & 0xffffu;

		for (int_t x = 0; x < targetWidth; ++x)
		{
			const unsigned int xFixed = targetWidth > 1
				? static_cast<unsigned int>((static_cast<unsigned long long>(x) *
					static_cast<unsigned long long>(sourceWidth - 1) << 16) /
					static_cast<unsigned int>(targetWidth - 1))
				: 0u;
			const int_t x0 = static_cast<int_t>(xFixed >> 16);
			const int_t x1 = std::min<int_t>(x0 + 1, sourceWidth - 1);
			const unsigned int fx = xFixed & 0xffffu;

			const std::size_t p00 = (static_cast<std::size_t>(y0) * sourceWidth + x0) * 4u;
			const std::size_t p10 = (static_cast<std::size_t>(y0) * sourceWidth + x1) * 4u;
			const std::size_t p01 = (static_cast<std::size_t>(y1) * sourceWidth + x0) * 4u;
			const std::size_t p11 = (static_cast<std::size_t>(y1) * sourceWidth + x1) * 4u;
			const std::size_t out = (static_cast<std::size_t>(y) * targetWidth + x) * 4u;

			for (int_t channel = 0; channel < 4; ++channel)
			{
				const unsigned int top =
					(static_cast<unsigned int>(src[p00 + channel]) * (0x10000u - fx) +
					 static_cast<unsigned int>(src[p10 + channel]) * fx + 0x8000u) >> 16;
				const unsigned int bottom =
					(static_cast<unsigned int>(src[p01 + channel]) * (0x10000u - fx) +
					 static_cast<unsigned int>(src[p11 + channel]) * fx + 0x8000u) >> 16;
				dst[out + channel] = static_cast<unsigned char>(
					(top * (0x10000u - fy) + bottom * fy + 0x8000u) >> 16);
			}
		}
	}

	return std::unique_ptr<BufferedImage>(
		new BufferedImage(targetWidth, targetHeight, std::move(dst)));
}

#if defined(DSI_PLATFORM)
// Largest power of two <= value, clamped to the DS 3D texture unit's valid
// 8-1024 range (nds/arm9/videoGL.h's glTexImageNtr2D()). Rounds DOWN rather
// than padding up to the next power of two: this is a decorative background
// image, a little softness from downscaling is invisible next to the
// legacy blur this same panorama used to (and, on DSi, no longer does --
// see LegacyPanorama.cpp) apply anyway, and padding would need the extra
// border pixels filled with *something* (transparent stretches the UVs,
// edge-clamped needs its own copy loop) for zero practical benefit.
int_t roundDownToPowerOfTwo(int_t value)
{
	if (value <= 8)
		return 8;
	int_t p = 8;
	while (p * 2 <= value && p < 1024)
		p *= 2;
	return p;
}
#endif

}

std::unique_ptr<BufferedImage> legacyPreparePanoramaForUpload(
	const std::string &name, std::unique_ptr<BufferedImage> image)
{
	// Real-hardware data (GuiMainMenu.cpp's per-section [dsi.perf] timing,
	// added specifically to find this): the menu's "title" section alone was
	// costing ~1-2 SECONDS on every single frame, indefinitely -- not once at
	// boot, every frame, for as long as the menu stayed on screen. That shape
	// (constant cost, no periodic pattern, no plateau after the first frame)
	// matches this exact bug already fixed once for panorama.png below: a
	// texture whose glTexImage2D() upload fails DS's exact-power-of-two check
	// is never inserted into RenderEngine's textureMap (see that file's
	// getTexture(), "Do not cache a broken texture id; retry the whole load
	// on the next bind") -- so every call to legacyDrawTitleTexture() (once
	// per menu frame) re-opens legacyUiTitleResourcePath() from the SD card,
	// re-decodes the PNG, and re-attempts (and re-fails) the upload from
	// scratch, forever. This function already existed to hand exactly this
	// hardware limitation a texture it can accept for panorama.png; the
	// legacy title banner is the same kind of real-photo-sourced, non-power-
	// of-two, purely decorative image (explicitly flagged as such --
	// legacyUiTitleResourcePath()'s own "hardcoded badd" comment), so it
	// needs the identical treatment, not a second copy of it.
	const bool isDsiLegacyDecorativeTexture =
#if defined(DSI_PLATFORM)
		name == "/legacy/panorama.png" || name == legacyUiTitleResourcePath();
#else
		name == "/legacy/panorama.png";
#endif
	if (!image || !isDsiLegacyDecorativeTexture)
		return image;

	const int_t sourceWidth = image->getWidth();
	const int_t sourceHeight = image->getHeight();
	if (sourceWidth <= 0 || sourceHeight <= 0)
		return image;

	// Defaults mean "no resize needed"; only a platform branch below that
	// actually wants a resize overwrites them. Declaring these unconditionally
	// (rather than inside each #if arm) keeps the shared resample call below
	// building on every platform, including ones that take neither branch.
	int_t targetWidth = sourceWidth;
	int_t targetHeight = sourceHeight;

#if defined(PS2_PLATFORM) || defined(WII_PLATFORM)
	const int_t maxWidth =
#if defined(PS2_PLATFORM)
		LEGACY_PANORAMA_PS2_MAX_WIDTH;
#else
		LEGACY_PANORAMA_WII_MAX_WIDTH;
#endif
	if (sourceWidth > maxWidth)
	{
		targetWidth = maxWidth;
		const long long scaledHeight =
			static_cast<long long>(sourceHeight) * static_cast<long long>(targetWidth);
		targetHeight = std::max<int_t>(1, static_cast<int_t>(
			(scaledHeight + sourceWidth / 2) / sourceWidth));
	}
#elif defined(DSI_PLATFORM)
	// Unlike PS2's GS or Wii's GX, the DS 3D texture unit requires an EXACT
	// power of two per dimension -- not just "small enough". A real photo-
	// sourced panorama.png (and, since real hardware just proved it, the
	// legacy title banner too) is essentially never power-of-two, so without
	// this it silently failed to upload at all (RenderAPI_DSI.cpp's
	// uploadTexture() now reports that failure instead of hiding it, but
	// the actual fix is to hand it a texture the hardware can accept in the
	// first place). For panorama.png that showed up as a blank white
	// background; for the title banner it showed up as a ~1-2s-per-frame
	// stall, since RenderEngine.cpp never caches a texture that fails this
	// validity check, so the full SD decode+upload was retried from scratch
	// on every single frame instead of once. 256 is also this console's own
	// screen width, so there is no benefit to keeping either image bigger
	// than that resident.
	targetWidth = roundDownToPowerOfTwo(std::min<int_t>(sourceWidth, 256));
	targetHeight = roundDownToPowerOfTwo(std::min<int_t>(sourceHeight, 256));
#endif

	if (targetWidth == sourceWidth && targetHeight == sourceHeight)
		return image;

	std::unique_ptr<BufferedImage> resized =
		resamplePanoramaBilinear(*image, targetWidth, targetHeight);
	MC_LOG_DEBUG("render", "legacy panorama upload resize %dx%d -> %dx%d\n",
		(int)sourceWidth, (int)sourceHeight, (int)targetWidth, (int)targetHeight);
	return resized;
}
