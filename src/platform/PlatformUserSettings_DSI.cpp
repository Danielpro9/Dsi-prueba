#include "platform/PlatformUserSettings.h"

#ifdef DSI_PLATFORM

namespace PlatformUserSettings
{
// Controls (including a deadzone/alternative-scheme story) are explicitly
// deferred for DSi -- see the DSi input backend, not yet written. No-ops
// until that exists rather than guessed at here.
void setControllerDeadzone(float)
{
}

void setAlternativeControls(bool)
{
}

// No deflicker filter on this hardware: the DS/DSi LCD is progressive, not an
// interlaced TV output, so there is no EFB->XFB-style flicker to filter (see
// the identical reasoning for WII_DEFLICKER in cmake/wii.cmake).
void setDisplayDeflicker(bool)
{
}
}

#endif // DSI_PLATFORM
