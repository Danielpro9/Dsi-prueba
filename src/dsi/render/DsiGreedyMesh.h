#pragma once

#ifdef DSI_PLATFORM

#include "java/Type.h"

// Greedy mesher for the DSi terrain build path -- a port of the proven, already
// shipping PS2 implementation (src/ps2/render/Ps2GreedyMesh.h/.cpp), reachable
// on DSi through the very hooks src/platform/RenderTerrainAPI_DSI.cpp already
// declared but never called (renderTerrainIsGreedyCube/renderTerrainGreedyMeshFace).
//
// Merging adjacent, identically-shaded faces of full opaque cubes into one
// larger quad cuts the vertex count DSi's captured-mesh replay
// (RenderAPI_DSI.cpp's drawCapturedMeshFast/drawInterleavedMesh) has to push
// through immediate-mode GL calls every frame -- see WorldRendererDsi.cpp's own
// file banner on why that per-vertex replay cost is this platform's real
// bottleneck, same shape of problem PS2's greedy mesher was built to reduce.
//
// One deliberate deviation from PS2's version: PS2's emitQuad() scales a merged
// quad's UV span by its width/height and relies on the GS's hardware
// REGION_REPEAT wrap mode to tile the source 16x16 texture across the larger
// area. The DS 3D engine has no region-repeat equivalent (confirmed absent from
// libnds' texture-parameter API), so DsiGreedyMesh keeps every merged quad's UV
// span identical to a single unmerged tile's -- the source texture is stretched
// across the merged geometry instead of tiled. PS2_GREEDY_MAX_MERGE (and its
// DSi counterpart, DSI_GREEDY_MAX_MERGE) caps this at 2x2, so the visible cost
// is a mildly stretched texture on merged runs, never a wrong or bled-in tile.
//
// Also simplified versus PS2: no row-bitmask per-section cache
// (Ps2MeshSectionCache/ps2_prepare_greedy_section_cache) and no raw
// six-slot-layout emitter -- DSi's per-step slice budget (DSI_GREEDY_SLICES_PER_
// STEP) already bounds a single call's cost the same way PS2's own slicing
// does (see Ps2GreedyMesh.h's own comment on why slicing alone fixed PS2's
// original "whole section in one frame" spike), and DSi's build loop already
// captures through the standard Tessellator API (WorldRendererDsi.cpp's
// dsiBuildRendererStep(), not a raw slot writer), so only the Tessellator-based
// emit path is needed. Revisit the section cache if a real hardware profile
// shows the per-cell FaceKey probes themselves are the bottleneck, not just the
// vertex count they used to produce.

class Block;
class ChunkCache;

// True when this block is handled by the greedy pass and must therefore be
// skipped by the per-block RenderBlocks loop (opaque pass only).
bool dsi_is_greedy_cube(Block* block);

// Greedy-mesh ONE face direction (0..5, matching RenderBlocks: 0=Y- 1=Y+ 2=Z-
// 3=Z+ 4=X- 5=X+) of the opaque full cubes inside [x0,x1) x [y0,y1) x [z0,z1)
// into Tessellator::instance (the caller must have started a quad batch).
// Coordinates are world block coordinates; vertices are emitted in world space
// (the section translation is applied by the caller, exactly like the per-block
// loop it replaces). Returns true if anything was emitted.
bool dsi_greedy_mesh_face(ChunkCache& cc, int face,
                          int x0, int y0, int z0,
                          int x1, int y1, int z1);

// Number of face directions dsi_greedy_mesh_face accepts.
enum { DSI_GREEDY_FACE_COUNT = 6 };

#endif // DSI_PLATFORM
