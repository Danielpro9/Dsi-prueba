#include "platform/Input.h"

#ifdef DSI_PLATFORM

#include <cstdint>
#include <nds.h>

#include "dsi/DsiEarlyInit.h"

// Menu-navigation input: the D-pad and A/B drive the console-style menu list
// (GuiMainMenu.cpp's updateScreen() PLATFORM_PS2 || PLATFORM_WII branch, now
// also PLATFORM_DSI), the same mechanism PS2/Wii already use --
// InputBackend_PS2.cpp is the reference this mirrors.
//
// In-game controls, added directly on request: the D-pad drives movement
// (platformGamepadSnapshot()'s leftX/leftY feed MovementInputFromOptions.cpp
// the same way PS2's analog stick does, via PLATFORM_DIRECT_ANALOG_MOVEMENT --
// see DsiInputTuning.h) and the touch screen drives the camera (rightX/rightY
// feed EntityRenderer.cpp's PLATFORM_DIRECT_CAMERA_ENABLED path the same way
// PS2's right stick does). Both reuse PS2's already-working formulas as-is;
// nothing new was invented for how a "stick" value turns into movement/camera
// motion, only how this hardware's D-pad/touchscreen produce that value.
//
// Does not call scanKeys() here: Display_dsi.cpp's processMessages() already
// calls it exactly once per frame (its isCloseRequested()/KEY_START check
// depends on that). Calling it again here would reset libnds's
// press/release edge-detection state mid-frame and make keysDown() miss
// presses -- this only ever reads back the state that scan already captured.
namespace
{
// Touch-drag camera state. Updated once a frame by dsiUpdateTouchCameraDelta()
// (called from Display_dsi.cpp's processMessages(), see that function's own
// comment on why this can only happen once a frame), read as often as needed
// by platformGamepadSnapshot() afterwards without disturbing it.
bool g_touchWasDown = false;
int g_prevTouchX = 0;
int g_prevTouchY = 0;
float g_touchDeltaX = 0.0f;
float g_touchDeltaY = 0.0f;

// A full-speed drag across this many pixels in one frame reads as a fully
// deflected stick (matches the [-1, 1] range Ps2AnalogFilter::apply()
// produces for PS2's real stick, which PLATFORM_DIRECT_CAMERA_SCALE and
// friends were tuned against -- see DsiInputTuning.h). Picked as a fraction
// of the 256px-wide touch screen that leaves room for a controlled, less-
// than-full-screen drag to still reach full turn speed; unverified against
// real hardware feel, same caveat as DsiInputTuning.h's own.
constexpr float kTouchDragPixelsForFullDeflection = 24.0f;

float normalizeDrag(float deltaPixels)
{
	float value = deltaPixels / kTouchDragPixelsForFullDeflection;
	if (value < -1.0f) value = -1.0f;
	if (value > 1.0f) value = 1.0f;
	return value;
}

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

void dsiUpdateTouchCameraDelta()
{
	touchPosition touch;
	touchRead(&touch);
	const bool touching = (keysHeld() & KEY_TOUCH) != 0;

	// Only a real drag (touching now, was touching last frame too) produces a
	// delta. A fresh touch-down has no previous position on this drag to
	// diff against -- reporting one would be the jump from wherever the last
	// drag ended to this new, unrelated touch-down point.
	if (touching && g_touchWasDown)
	{
		g_touchDeltaX = static_cast<float>(static_cast<int>(touch.px) - g_prevTouchX);
		g_touchDeltaY = static_cast<float>(static_cast<int>(touch.py) - g_prevTouchY);
	}
	else
	{
		g_touchDeltaX = 0.0f;
		g_touchDeltaY = 0.0f;
	}

	if (touching)
	{
		g_prevTouchX = touch.px;
		g_prevTouchY = touch.py;
	}
	g_touchWasDown = touching;
}

PlatformGamepadSnapshot platformGamepadSnapshot(int)
{
	PlatformGamepadSnapshot out;
	out.connected = true; // Built into the hardware, always "there".

	const std::uint32_t held = keysHeld();
	// Digital D-pad: each axis is -1, 0 or +1, no deadzone to apply.
	// MovementInputFromOptions.cpp computes moveForward += -leftY and
	// moveStrafe += -leftX, so up/left need to be the negative direction for
	// forward/strafe-left to come out positive the way that file expects.
	out.leftX = (held & KEY_LEFT) ? -1.0f : (held & KEY_RIGHT) ? 1.0f : 0.0f;
	out.leftY = (held & KEY_UP)   ? -1.0f : (held & KEY_DOWN)  ? 1.0f : 0.0f;

	// Touch-drag delta, normalized to the same [-1, 1] "stick deflection"
	// range EntityRenderer.cpp's direct-camera path expects. See
	// dsiUpdateTouchCameraDelta() for where this is actually computed --
	// exactly once a frame, not here, so calling this more than once in the
	// same frame (movement and camera each call it separately) reads a
	// stable value instead of consuming it.
	out.rightX = normalizeDrag(g_touchDeltaX);
	out.rightY = normalizeDrag(g_touchDeltaY);

	return out;
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
