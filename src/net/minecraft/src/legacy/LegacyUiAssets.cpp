#include "LegacyUiAssets.h"

#include "LegacyUiPolicy.h"
#include "net/minecraft/src/Minecraft.h"
#include "net/minecraft/src/RenderEngine.h"
#include "net/minecraft/src/Tessellator.h"
#include "platform/RenderAPI.h"

namespace
{
struct LegacyTitleTextureCache
{
    RenderEngine *engine = nullptr;
    bool resourceChecked = false;
    bool resourceAvailable = false;
};

LegacyTitleTextureCache g_titleTextureCache;

// Real-hardware evidence (reported: the OptiCraft logo gets replaced by
// Steve's skin PNG after entering and then leaving a world): this function
// used to cache the resolved GL texture NAME (an int) here, separately from
// RenderEngine's own textureMap. ClientPlatformPolicy_DSI.cpp's
// releaseWorldEntryAssets() calls renderEngine->releaseTexture(title.png)
// right before a world loads, to free VRAM -- that correctly erases title.png
// from textureMap AND deletes its GL texture name, but this separate cache
// was never told, so it kept holding the now-deleted name. renderTextureIsValid()
// only checks whether that NAME is currently allocated to *something*, not
// whether it still holds title.png's data -- and libnds's glGenTextures()
// hands out freed names again, so the very next texture allocated after the
// release (in practice: the player's skin, downloaded once gameplay starts)
// was highly likely to receive that exact freed name. Back at the menu, this
// function's own valid-looking-but-stale cache then bound that name and drew
// whatever RenderEngine::getTexture("/legacy/title.png") the reload path
// still tracks. Removed here in favor of that path.
int_t resolveLegacyTitleTexture(RenderEngine *engine, const char *path)
{
    if (engine == nullptr || path == nullptr)
        return -1;

    if (g_titleTextureCache.engine != engine)
    {
        g_titleTextureCache.engine = engine;
        g_titleTextureCache.resourceChecked = false;
        g_titleTextureCache.resourceAvailable = false;
    }

    if (!g_titleTextureCache.resourceChecked)
    {
        g_titleTextureCache.resourceAvailable = engine->hasResource(path);
        g_titleTextureCache.resourceChecked = true;
    }
    if (!g_titleTextureCache.resourceAvailable)
        return -1;

    // RenderEngine::getTexture() already caches by resource path in its own
    // textureMap, correctly invalidated by releaseTexture() -- this is the
    // same lookup every other caller in the engine relies on (FontRenderer::
    // refresh(), GuiMainMenu, ...), so a second cache here can only go stale.
    const int_t texture = engine->getTexture(path);
    if (!renderTextureIsValid(texture))
        return -1;
    return texture;
}
}

bool legacyDrawTitleTexture(Minecraft *mc, const LegacyMainMenuLayout &layout, int_t screenWidth,
    float_t zLevel, LegacyUiRect *outRect)
{
    if (outRect != nullptr)
        *outRect = LegacyUiRect{};
    if (mc == nullptr || mc->renderEngine == nullptr)
        return false;

    const char *path = legacyUiTitleResourcePath();
    const int_t texture = resolveLegacyTitleTexture(mc->renderEngine, path);
    if (texture < 0)
        return false;
    int_t textureWidth = 0;
    int_t textureHeight = 0;
    if (!mc->renderEngine->getTextureDimensions(texture, &textureWidth, &textureHeight) ||
        textureWidth <= 0 || textureHeight <= 0)
        return false;

    const LegacyUiRect rect = legacyFitTitleRect(screenWidth, layout.titleY, layout.titleMaxWidth,
        layout.titleMaxHeight, textureWidth, textureHeight);
    if (rect.width <= 0 || rect.height <= 0)
        return false;

    renderBindTexture(texture);
    renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    renderEnable(RenderCapability::Blend);
    renderBlendFunc(RenderBlendFactor::SrcAlpha, RenderBlendFactor::OneMinusSrcAlpha);

    Tessellator *tess = &Tessellator::instance;
    tess->startDrawingQuads();
    tess->setColorOpaque_I(0xffffff);
    tess->addVertexWithUV(rect.x, rect.y + rect.height, zLevel, 0.0, 1.0);
    tess->addVertexWithUV(rect.x + rect.width, rect.y + rect.height, zLevel, 1.0, 1.0);
    tess->addVertexWithUV(rect.x + rect.width, rect.y, zLevel, 1.0, 0.0);
    tess->addVertexWithUV(rect.x, rect.y, zLevel, 0.0, 0.0);
    tess->draw();
    renderDisable(RenderCapability::Blend);

    if (outRect != nullptr)
        *outRect = rect;
    return true;
}
