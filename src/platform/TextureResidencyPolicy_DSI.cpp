#include "platform/TextureResidencyPolicy.h"

#ifdef DSI_PLATFORM

namespace TextureResidencyPolicy
{
// RenderAPI_DSI.cpp's DsiTexture already keeps its own RGBA8 CPU mirror for
// every texture regardless of dynamicTexture, since glTexImageNtr2D() has no
// partial-upload path and a sub-image write needs the full previous image to
// patch (see the struct comment there). So there is nothing static textures
// could discard here that dynamic ones can't -- this is a no-op on this
// backend, not an oversight.
void afterNamedTextureUpload(int, bool)
{
}
}

#endif // DSI_PLATFORM
