#include "platform/Input.h"

#ifdef DSI_PLATFORM

#include <cstdint>
#include <nds.h>

// Menu-navigation input: the D-pad and A/B now drive the console-style menu
// list (GuiMainMenu.cpp's updateScreen() PLATFORM_PS2 || PLATFORM_WII branch,
// now also PLATFORM_DSI), the same mechanism PS2/Wii already use --
// InputBackend_PS2.cpp is the reference this mirrors. Deliberately NOT a
// full controller mapping yet (no analog stick on this hardware, no
// in-world movement/action bindings): platformGamepadSnapshot and friends
// below still report nothing, since only menu navigation has been asked
// for and verified so far.
//
// Does not call scanKeys() here: Display_dsi.cpp's processMessages() already
// calls it exactly once per frame (its isCloseRequested()/KEY_START check
// depends on that). Calling it again here would reset libnds's
// press/release edge-detection state mid-frame and make keysDown() miss
// presses -- this only ever reads back the state that scan already captured.
namespace
{
std::uint32_t mapTextButtons(std::uint32_t bits)
{
	std::uint32_t value = 0;
	if (bits & KEY_LEFT)  value |= PLATFORM_TEXT_LEFT;
	if (bits & KEY_RIGHT) value |= PLATFORM_TEXT_RIGHT;
	if (bits & KEY_UP)    value |= PLATFORM_TEXT_UP;
	if (bits & KEY_DOWN)  value |= PLATFORM_TEXT_DOWN;
	if (bits & KEY_A)     value |= PLATFORM_TEXT_TYPE;
	// B is this hardware's one "cancel/go back" button, so it stands in for
	// both flags other platforms split across two buttons (PS2: Square for
	// BACK -- see LegacyOptionsScreen.cpp/GuiIngameMenu.cpp's separate BACK
	// checks -- Circle for CLOSE, the "leave this screen" check most legacy
	// screens gate on, e.g. LegacyOptionsScreen.cpp's CLOSE|SHIFT check).
	if (bits & KEY_B)     value |= PLATFORM_TEXT_BACK | PLATFORM_TEXT_CLOSE;
	return value;
}
}

PlatformTextInputSnapshot platformTextInputSnapshot(int)
{
	PlatformTextInputSnapshot out;
	out.connected = true; // The D-pad/buttons are built into the hardware, always "there".
	out.held = mapTextButtons(keysHeld());
	out.pressed = mapTextButtons(keysDown());
	return out;
}

PlatformGamepadSnapshot platformGamepadSnapshot(int)
{
	return PlatformGamepadSnapshot{};
}

PlatformGamepadSnapshot platformRawGamepadSnapshot(int)
{
	return PlatformGamepadSnapshot{};
}

int platformMenuPad()
{
	return 0;
}

bool platformMenuPointerActive()
{
	return false;
}

bool platformMenuCursorVisible()
{
	return false;
}

void platformSetMenuCursor(int, int)
{
}

const PlatformKeyboardHints& platformKeyboardHints()
{
	static const PlatformKeyboardHints hints{};
	return hints;
}

const char* platformInputDebugLine()
{
	return "";
}

#endif // DSI_PLATFORM
