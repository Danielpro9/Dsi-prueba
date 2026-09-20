#include "RenderLightingProfile.h"

#ifdef DSI_PLATFORM
// Same values PS2 uses -- this is a generic ambient/diffuse split for held-item
// lighting, not something tied to specific 3D hardware, and nothing about the
// DS 3D engine's lighting (see RenderAPI_DSI.cpp's renderLightfv() comment)
// suggests a different split is needed here.
RenderLightingProfile renderGetStandardItemLightingProfile() { return {0.4f, 0.6f}; }
#endif // DSI_PLATFORM
