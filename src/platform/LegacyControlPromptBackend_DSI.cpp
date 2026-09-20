#include "platform/LegacyControlPromptBackend.h"

#ifdef DSI_PLATFORM

// Control prompts ("press X to open inventory") need a real key binding to
// name, and DSi input is deferred (see InputBackend_DSI.cpp) -- an empty
// label is the honest answer until there is a scheme to describe.
std::string legacyControlPromptLabel(const GameSettings&, LegacyControlAction)
{
	return std::string();
}

#endif // DSI_PLATFORM
