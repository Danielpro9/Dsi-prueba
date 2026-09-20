// RenderList for DSi -- see src/pc/minecraft/RenderList.cpp (OpenGL display
// lists) and src/wii/minecraft/RenderList.cpp (native GX chunk batches) for
// the two existing shapes this takes. Neither applies here: the DS 3D engine
// has no display lists and no native terrain pipeline (see RenderAPI_DSI.cpp
// and WorldRendererDsi.cpp), so a section's terrain is a captured-in-RAM
// RenderStaticMesh replayed with plain immediate-mode calls every frame
// (platform/RenderStaticMesh.cpp -- the same mechanism RenderGlobal already
// uses for the sky/star meshes). This file is therefore the simplest of the
// three: it just remembers which (WorldRenderer*, pass) pairs were added this
// frame and asks each one to draw its own already-cached mesh.
#include "net/minecraft/src/RenderList.h"

#include "net/minecraft/src/WorldRenderer.h"
#include "platform/RenderAPI.h"

RenderList::RenderList()
{
    originX = 0;
    originY = 0;
    originZ = 0;
    viewerX = 0.0;
    viewerY = 0.0;
    viewerZ = 0.0;
    initialized = false;
}

void RenderList::setup(int i, int j, int k, double d, double d1, double d2)
{
    initialized = true;
    terrainEntries.clear();
    originX = i;
    originY = j;
    originZ = k;
    viewerX = d;
    viewerY = d1;
    viewerZ = d2;
}

bool RenderList::matchesPos(int i, int j, int k)
{
    return initialized && i == originX && j == originY && k == originZ;
}

void RenderList::addTerrainRenderer(WorldRenderer *renderer, int_t pass)
{
    if (renderer == nullptr || pass < 0 || pass > 1)
        return;
    terrainEntries.push_back({renderer, pass});
}

void RenderList::render()
{
    if (!initialized || terrainEntries.empty())
        return;

    // Same coarse origin-bucket-to-viewer translate PC and Wii apply around
    // their whole batch; WorldRenderer::drawCapturedTerrain() then applies the
    // finer posXClip/Y/Z chunk-local translate per section, exactly as PC's
    // display list and Wii's renderExtraTerrainMeshes() do.
    const float translateX = static_cast<float>(static_cast<double>(originX) - viewerX);
    const float translateY = static_cast<float>(static_cast<double>(originY) - viewerY);
    const float translateZ = static_cast<float>(static_cast<double>(originZ) - viewerZ);
    renderPushMatrix();
    renderTranslate(translateX, translateY, translateZ);

    for (const TerrainRenderEntry &entry : terrainEntries)
    {
        if (entry.renderer != nullptr)
            entry.renderer->drawCapturedTerrain(entry.pass);
    }

    // CTM (OptiFine connected-texture) atlases are captured separately from
    // the main terrain mesh and must replay after it so their texture binds
    // don't land in the middle of /terrain.png geometry -- same ordering PC
    // and Wii use for the identical reason.
    for (const TerrainRenderEntry &entry : terrainEntries)
    {
        if (entry.renderer != nullptr)
            entry.renderer->renderExtraTerrainMeshes(entry.pass);
    }

    renderPopMatrix();
}

void RenderList::reset()
{
    initialized = false;
    terrainEntries.clear();
}
