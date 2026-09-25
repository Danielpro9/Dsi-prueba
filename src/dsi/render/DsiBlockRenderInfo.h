#pragma once

#include <array>
#include <cstdint>

#include "java/Type.h"

// Per-block classification for DSi's greedy mesher (DsiGreedyMesh.cpp), mirroring
// src/ps2/render/Ps2BlockRenderInfo.h exactly -- this logic (render type/pass,
// opaque-cube lookup, default-texture/colour-multiplier invariance) is plain
// Block-API code, not PS2-specific, so the eligibility rule stays identical to
// the one already proven on PS2 rather than a second hand-picked version.
struct DsiBlockRenderInfo
{
    std::int8_t renderType = -1;
    std::uint8_t renderPass = 0;
    bool simpleOpaqueCube = false;
    bool staticTextureBySide = false;
    bool defaultWhiteColorMultiplier = false;
    std::array<std::int16_t, 6> textureBySide{};
};

constexpr bool dsiSimpleOpaqueCubeEligible(int renderType, int renderPass,
    bool opaqueCube, bool normalCube, bool defaultFaceCulling, bool unitBounds)
{
    return renderType == 0 && renderPass == 0 && opaqueCube && normalCube &&
        defaultFaceCulling && unitBounds;
}

const DsiBlockRenderInfo &dsiGetBlockRenderInfo(int_t blockId);
