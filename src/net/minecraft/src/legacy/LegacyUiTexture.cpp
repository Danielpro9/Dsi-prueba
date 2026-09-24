#include "LegacyUiTexture.h"

#include "net/minecraft/src/RenderEngine.h"
#include "net/minecraft/src/Tessellator.h"
#include "platform/RenderAPI.h"

#include <algorithm>

LegacyUiTexture::LegacyUiTexture(const char *resourcePath)
    : path(resourcePath), boundEngine(nullptr), texture(-1), resourceChecked(false),
      resourceAvailable(false)
{
}

int_t LegacyUiTexture::resolve(RenderEngine *engine)
{
    if (engine == nullptr || path == nullptr)
        return -1;

    // A texture pack switch hands out a different RenderEngine. Drop the cached id
    // instead of binding a handle that belongs to the previous pack.
    if (boundEngine != engine)
    {
        boundEngine = engine;
        texture = -1;
        resourceChecked = false;
        resourceAvailable = false;
    }

    // hasResource() touches the pack index, so the miss is cached too: a missing
    // asset must not cost a lookup on every widget of every frame.
    if (!resourceChecked)
    {
        resourceAvailable = engine->hasResource(path);
        resourceChecked = true;
    }
    if (!resourceAvailable)
        return -1;

    // Real-hardware evidence (same bug traced through LegacyUiAssets.cpp's
    // title-texture cache, which this class's caching predates): reusing a
    // once-resolved GL texture NAME across calls goes stale the moment
    // anything releases that resource's texture (ClientPlatformPolicy_DSI.cpp's
    // releaseWorldEntryAssets() does exactly this for every path this class is
    // ever constructed with -- scroll_down.png/tick.png/tickbox.png/
    // tickbox_hovered.png -- right before a world loads, to free VRAM).
    // renderTextureIsValid() only reports whether that NAME is currently
    // allocated to *something*, not whether it still holds this resource's
    // data; libnds hands freed names back out to the next texture allocated
    // (in practice: the player's skin, downloaded once gameplay starts), so a
    // cached name surviving a release can end up silently bound to unrelated
    // image data once the menu is shown again. RenderEngine::getTexture()
    // already caches by resource path in its own textureMap, correctly
    // invalidated by releaseTexture() -- deferring to it here every call
    // removes the second, unsafe cache instead of trying to keep both in sync.
    const int_t resolved = engine->getTexture(path);
    if (!renderTextureIsValid(resolved))
        return -1;

    texture = resolved;
    return texture;
}

void legacyDrawUiTexture(int_t texture, int_t x, int_t y, int_t width, int_t height, float_t zLevel)
{
    if (texture < 0 || width <= 0 || height <= 0)
        return;

    renderBindTexture(texture);
    renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    renderEnable(RenderCapability::Blend);
    renderBlendFunc(RenderBlendFactor::SrcAlpha, RenderBlendFactor::OneMinusSrcAlpha);

    Tessellator *tess = &Tessellator::instance;
    tess->startDrawingQuads();
    tess->setColorOpaque_I(0xffffff);
    tess->addVertexWithUV(x, y + height, zLevel, 0.0, 1.0);
    tess->addVertexWithUV(x + width, y + height, zLevel, 1.0, 1.0);
    tess->addVertexWithUV(x + width, y, zLevel, 1.0, 0.0);
    tess->addVertexWithUV(x, y, zLevel, 0.0, 0.0);
    tess->draw();

    renderDisable(RenderCapability::Blend);
}

void legacyDrawUiTextureNineSlice(int_t texture, int_t x, int_t y, int_t width, int_t height,
    int_t spriteWidth, int_t spriteHeight, int_t border, float_t zLevel)
{
    if (texture < 0 || width <= 0 || height <= 0 || spriteWidth <= 0 || spriteHeight <= 0)
        return;
    if (border * 2 > width || border * 2 > height)
        border = std::min<int_t>(width, height) / 2;

    // Column / row edges in destination pixels and in texture space (0..1).
    const int_t dx[4] = { x, x + border, x + width - border, x + width };
    const int_t dy[4] = { y, y + border, y + height - border, y + height };
    const double ux[4] = { 0.0, (double)border / spriteWidth, 1.0 - (double)border / spriteWidth, 1.0 };
    const double uy[4] = { 0.0, (double)border / spriteHeight, 1.0 - (double)border / spriteHeight, 1.0 };

    renderBindTexture(texture);
    renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    renderEnable(RenderCapability::Blend);
    renderBlendFunc(RenderBlendFactor::SrcAlpha, RenderBlendFactor::OneMinusSrcAlpha);

    Tessellator *tess = &Tessellator::instance;
    tess->startDrawingQuads();
    tess->setColorOpaque_I(0xffffff);
    for (int_t row = 0; row < 3; ++row)
    {
        if (dy[row + 1] <= dy[row])
            continue;
        for (int_t column = 0; column < 3; ++column)
        {
            if (dx[column + 1] <= dx[column])
                continue;
            tess->addVertexWithUV(dx[column],     dy[row + 1], zLevel, ux[column],     uy[row + 1]);
            tess->addVertexWithUV(dx[column + 1], dy[row + 1], zLevel, ux[column + 1], uy[row + 1]);
            tess->addVertexWithUV(dx[column + 1], dy[row],     zLevel, ux[column + 1], uy[row]);
            tess->addVertexWithUV(dx[column],     dy[row],     zLevel, ux[column],     uy[row]);
        }
    }
    tess->draw();

    renderDisable(RenderCapability::Blend);
}
