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
void dsiRepackCapturedMeshFast(RenderCapturedMesh& mesh);

#endif // DSI_PLATFORM
