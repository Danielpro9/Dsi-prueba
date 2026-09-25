#include "dsi/render/DsiBlockRenderInfo.h"

#ifdef DSI_PLATFORM

#include <array>
#include <cmath>

#include "net/minecraft/src/Block.h"
#include "net/minecraft/src/Material.h"

namespace
{
    std::array<DsiBlockRenderInfo, Block::BLOCK_REGISTRY_SIZE> s_renderInfo{};
    bool s_renderInfoReady = false;

    bool isUnitBounds(const Block *block)
    {
        if (block == nullptr)
            return false;

        constexpr float epsilon = 0.000001f;
        return std::fabs(static_cast<float>(block->minX)) <= epsilon &&
            std::fabs(static_cast<float>(block->minY)) <= epsilon &&
            std::fabs(static_cast<float>(block->minZ)) <= epsilon &&
            std::fabs(static_cast<float>(block->maxX) - 1.0f) <= epsilon &&
            std::fabs(static_cast<float>(block->maxY) - 1.0f) <= epsilon &&
            std::fabs(static_cast<float>(block->maxZ) - 1.0f) <= epsilon;
    }

    void buildRenderInfo()
    {
        if (s_renderInfoReady)
            return;

        for (int_t id = 0; id < Block::BLOCK_REGISTRY_SIZE; ++id)
        {
            DsiBlockRenderInfo info;
            Block *block = Block::blocksList[id];
            if (block != nullptr)
            {
                const int_t renderType = block->getRenderType();
                const int_t renderPass = block->getRenderBlockPass();
                const bool opaqueCube = Block::opaqueCubeLookup[id];
                const bool normalCube = block->blockMaterial != nullptr &&
                    block->blockMaterial->getIsSolid() && block->renderAsNormalBlock();
                info.renderType = static_cast<std::int8_t>(renderType);
                info.renderPass = static_cast<std::uint8_t>(renderPass > 0 ? 1 : 0);
                info.simpleOpaqueCube = dsiSimpleOpaqueCubeEligible(renderType, renderPass,
                    opaqueCube, normalCube, Block::usesDefaultFaceCullingLookup[id], isUnitBounds(block));
                info.defaultWhiteColorMultiplier = info.simpleOpaqueCube && block->usesDefaultColorMultiplier();
                if (info.simpleOpaqueCube && block->usesDefaultWorldTextureLookup())
                {
                    bool metadataInvariant = true;
                    for (int_t side = 0; side < 6; ++side)
                    {
                        const int_t texture = block->getBlockTextureFromSideAndMetadata(side, 0);
                        info.textureBySide[static_cast<std::size_t>(side)] = static_cast<std::int16_t>(texture);
                        for (int_t metadata = 1; metadata < 16; ++metadata)
                        {
                            if (block->getBlockTextureFromSideAndMetadata(side, metadata) != texture)
                            {
                                metadataInvariant = false;
                                break;
                            }
                        }
                        if (!metadataInvariant)
                            break;
                    }
                    info.staticTextureBySide = metadataInvariant;
                }
            }
            s_renderInfo[static_cast<std::size_t>(id)] = info;
        }

        s_renderInfoReady = true;
    }
}

const DsiBlockRenderInfo &dsiGetBlockRenderInfo(int_t blockId)
{
    static const DsiBlockRenderInfo emptyInfo{};
    if (blockId < 0 || blockId >= Block::BLOCK_REGISTRY_SIZE)
        return emptyInfo;

    buildRenderInfo();
    return s_renderInfo[static_cast<std::size_t>(blockId)];
}

#else

const DsiBlockRenderInfo &dsiGetBlockRenderInfo(int_t)
{
    static const DsiBlockRenderInfo emptyInfo{};
    return emptyInfo;
}

#endif
