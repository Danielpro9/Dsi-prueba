// Keyboard_dsi.cpp — DSi implementation of lwjgl::Keyboard.
//
// Real input (buttons, touch screen) is explicitly deferred (see
// InputBackend_DSI.cpp and the project brief) -- no pad/keyboard poll exists
// yet to call detail::pushKey()/pushChar() declared in lwjgl/Keyboard.h, so
// this reports "nothing ever pressed" rather than leaving the many GUI call
// sites (GuiChat, GuiCreateWorld, GameSettings key-name lookup, ...) as
// unresolved link errors. Once real DSi input is designed, that poll
// implementation is the only thing that needs to feed this queue.
#ifdef DSI_PLATFORM

#include "lwjgl/Keyboard.h"
#include "lwjgl/KeyNames.h"

namespace lwjgl
{
namespace Keyboard
{

jstring getKeyName(int_t key)
{
	if (const char *name = lwjglKeyDisplayName(key))
		return name;
	return "KEY " + std::to_string(key);
}

static bool s_repeatEvents = false;

bool next() { return false; }

void enableRepeatEvents(bool repeat) { s_repeatEvents = repeat; }
bool areRepeatEventsEnabled()        { return s_repeatEvents; }

char_t getEventCharacter() { return 0; }
int_t  getEventKey()       { return KEY_NONE; }
bool   getEventKeyState()  { return false; }

void poll() {}

bool isKeyDown(int_t) { return false; }

} // namespace Keyboard
} // namespace lwjgl

#endif // DSI_PLATFORM
