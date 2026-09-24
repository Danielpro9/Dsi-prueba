#pragma once
#ifdef DSI_PLATFORM

struct RenderCapturedMesh;

// Converts a finished captured mesh's position field from float3 to the GPU's
// native v16 fixed-point format, in place, once. Defined in
// platform/RenderAPI_DSI.cpp (same precedent as dsi/DsiEarlyInit.h's
// dsiTotalTextureVramBytes()/dsiTextureVramBytes(): declared under dsi/,
// implemented where the RenderAPI_DSI-internal state it needs already lives).
// See that definition's own comment for the full why and why only there --
// WorldRendererDsi.cpp is meant to be the only caller, right after a
// section's mesh finishes compiling, never on an in-progress build step.
//
// terrainTextureId: the GL texture name this mesh is always drawn against
// (WorldRendererDsi.cpp's drawCapturedTerrain() never rebinds a texture
// itself -- see renderExtraTerrainMeshes()'s own "state assumes terrain.png
// is still bound" comment for why that invariant holds -- so the caller
// already knows this, typically ConnectedTextures::getTerrainTextureId()).
// Used to also pre-convert texcoords the same way, IF that texture is
// already resident (its width/height known); see RenderCapturedMesh::
// texCoordIsT16's own comment for why this is only safe when the mesh's
// draw-time texture is known fixed in advance, which callers other than the
// main terrain section mesh should NOT assume for themselves.
void dsiRepackCapturedMeshFast(RenderCapturedMesh& mesh, int terrainTextureId);

#endif // DSI_PLATFORM
