#ifdef DSI_PLATFORM

#include "platform/RenderTerrainAPI.h"
#include "platform/RenderAPI.h"
#include "dsi/render/DsiGreedyMesh.h"

// Same shape as RenderTerrainAPI_PC.cpp/RenderTerrainAPI_GL.cpp: PLATFORM_
// NATIVE_TERRAIN_PIPELINE is Wii-only (see PlatformConfig.h), so DSi -- like
// PC and PS2 -- never compiles the native chunk-handle/face-group-sort path
// this header also declares (renderTerrainCacheBeginOpaqueBuild() and
// friends), only the plain per-quad path RenderAPI_DSI.cpp's
// renderDrawInterleaved()/renderCaptureInterleaved() already implement.
// Everything below is either genuinely unused on that path (returns a safe
// "did nothing" value) or a thin forward to RenderAPI_DSI.cpp.

namespace { int s_nextTerrainHandle = 1; }

int renderTerrainCreateChunkHandle() { return s_nextTerrainHandle++; }
void renderTerrainDestroyChunkHandle(int) {}
void renderTerrainClearChunkHandle(int) {}
void renderTerrainSwapChunkHandles(int, int) {}
bool renderTerrainBeginChunkBatch(int) { return false; }
bool renderTerrainAppendChunk(int) { return false; }
void renderTerrainEndChunkBatch() {}
void renderTerrainCaptureCamera() {}
void renderTerrainSetViewerPosition(double, double, double) {}
void renderTerrainSetFog(RenderFogMode, float, float, float, float, float, float, float) {}
void renderTerrainSetEarlyDepth(bool) {}
bool renderTerrainSortOpaqueFaces(const std::vector<int_t>&, std::vector<int_t>&, int, int) { return false; }
bool renderTerrainBeginPass(int texture, RenderTerrainPass) { renderBindTexture(texture); return true; }
void renderTerrainEndPass(RenderTerrainPass) {}
std::size_t renderTerrainLiveBytes() { return 0; }
std::size_t renderTerrainStagingBytes() { return 0; }

bool renderTerrainCaptureFrame(RenderTerrainFrame& out) { out = RenderTerrainFrame{}; return false; }
RenderTerrainDrawResult renderTerrainDrawSection(const RenderTerrainFrame&, const RenderTerrainSectionView&, const RenderTerrainFallbackDraw&) { return {}; }

void renderTerrainCacheInit(RenderTerrainBackendCache& cache) { cache.initialized = true; }
void renderTerrainCacheDestroy(RenderTerrainBackendCache& cache) { cache.initialized = false; }
void renderTerrainCacheReset(RenderTerrainBackendCache&) {}
void renderTerrainCacheRelease(RenderTerrainBackendCache&) {}
std::size_t renderTerrainCacheRamBytes(const RenderTerrainBackendCache&) { return 0; }
void renderTerrainCacheRamBreakdown(const RenderTerrainBackendCache&, RenderTerrainCacheRamBreakdown&) {}
bool renderTerrainCacheSortFaces(RenderTerrainBackendCache&, const int_t*, int_t*, int_t) { return false; }
bool renderTerrainCacheBuildOpaque(RenderTerrainBackendCache&, const int_t*, std::size_t, int_t, int_t, bool, bool, bool) { return false; }
bool renderTerrainCacheOpaqueValid(const RenderTerrainBackendCache&) { return false; }
int_t renderTerrainCacheOpaqueVertexCount(const RenderTerrainBackendCache&) { return 0; }
const void* renderTerrainCacheFaceGroups(const RenderTerrainBackendCache&) { return nullptr; }
const void* renderTerrainCacheOpaqueMesh(const RenderTerrainBackendCache&) { return nullptr; }

// Forwards to DsiGreedyMesh.cpp -- the actual port of PS2's proven greedy
// mesher (see that file's header comment for the one deliberate deviation,
// merged-quad UV not scaling with width/height, and why). WorldRendererDsi.cpp
// calls these through this same generic RenderTerrainAPI.h pair PS2's own
// RenderTerrainAPI_GS_PS2.cpp forwards through, rather than calling
// DsiGreedyMesh.cpp directly, so both platforms' terrain builders share one
// call shape.
bool renderTerrainIsGreedyCube(Block* block) { return dsi_is_greedy_cube(block); }
bool renderTerrainGreedyMeshFace(ChunkCache& cache, int face, int x0, int y0, int z0, int x1, int y1, int z1)
{
	return dsi_greedy_mesh_face(cache, face, x0, y0, z0, x1, y1, z1);
}

#endif // DSI_PLATFORM
