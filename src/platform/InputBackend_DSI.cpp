#include "platform/Input.h"

#ifdef DSI_PLATFORM

// Real input (buttons, touch screen as a pointer) is explicitly deferred --
// see the project brief and DsiBringup.cpp's demos, which read libnds keys
// directly rather than through this interface. Everything here reports "no
// controller, nothing pressed" rather than guessing at a control scheme
// before one has been designed.

PlatformTextInputSnapshot platformTextInputSnapshot(int)
{
	return PlatformTextInputSnapshot{};
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
