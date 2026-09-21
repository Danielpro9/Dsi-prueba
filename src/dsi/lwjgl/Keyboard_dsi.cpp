// Keyboard_dsi.cpp — DSi implementation of lwjgl::Keyboard.
//
// Was a full stub (queue always empty) while real input was deferred. Now
// fed the same way PS2's Keyboard_ps2.cpp already is: InputBackend_DSI.cpp's
// per-frame button poll calls detail::pushKey() for the physical buttons
// mapped to keyboard-bound actions (jump, inventory, chat -- see
// InputBackend_DSI.cpp's own comment for the full button scheme), and
// Minecraft.cpp's existing `while (lwjgl::Keyboard::next())` loop consumes
// them exactly as if they were real key presses.
#ifdef DSI_PLATFORM

#include "lwjgl/Keyboard.h"
#include "lwjgl/KeyNames.h"

#include <queue>

namespace lwjgl
{
namespace Keyboard
{

namespace detail
{

struct Event
{
	int key;
	int character;
	bool down;
};

static Event s_current = {};
static std::queue<Event> s_queue;

// Simple bitfield: isKeyDown per LWJGL key code (max 256), same as PS2's.
static bool s_keyState[256] = {};

void pushKey(int lwjglKey, bool down)
{
	if (lwjglKey >= 0 && lwjglKey < 256)
		s_keyState[lwjglKey] = down;
	s_queue.push({lwjglKey, 0, down});
}

void pushChar(int character)
{
	s_queue.push({KEY_NONE, character, true});
}

} // namespace detail

jstring getKeyName(int_t key)
{
	if (const char *name = lwjglKeyDisplayName(key))
		return name;
	return "KEY " + std::to_string(key);
}

static bool s_repeatEvents = false;

bool next()
{
	if (detail::s_queue.empty())
		return false;
	detail::s_current = detail::s_queue.front();
	detail::s_queue.pop();
	return true;
}

void enableRepeatEvents(bool repeat) { s_repeatEvents = repeat; }
bool areRepeatEventsEnabled()        { return s_repeatEvents; }

char_t getEventCharacter() { return (char_t)detail::s_current.character; }
int_t  getEventKey()       { return detail::s_current.key; }
bool   getEventKeyState()  { return detail::s_current.down; }

void poll() {}

bool isKeyDown(int_t key)
{
	if (key < 0 || key >= 256)
		return false;
	return detail::s_keyState[key];
}

} // namespace Keyboard
} // namespace lwjgl

#endif // DSI_PLATFORM
