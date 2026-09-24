#pragma once

// D-pad drives movement directly (MovementInputFromOptions.cpp), the same
// scheme PS2 already uses for its analog stick -- see PLATFORM_DIRECT_ANALOG_
// MOVEMENT in PlatformConfig.h. The D-pad is fully digital (each axis is
// -1, 0 or +1, from InputBackend_DSI.cpp's platformGamepadSnapshot()), so
// there is no deadzone to tune and no scale beyond 1:1.
#define DSI_DIRECT_MOVE_SCALE 1.0f

// The touch screen drives the camera (EntityRenderer.cpp's PLATFORM_DIRECT_
// CAMERA_ENABLED path), reusing the same "stick deflection * sensitivity *
// elapsed time" formula PS2's right stick already uses -- InputBackend_DSI.cpp
// normalizes the raw per-frame touch-drag pixel delta into roughly the same
// [-1, 1] range a real analog stick reports before handing it to
// platformGamepadSnapshot(), so the same scale this formula was tuned for on
// PS2 is the right starting point here too.
//
// UNTESTED on real hardware: this sandbox cannot run the game to feel out
// drag sensitivity, only compile it (see DsiEarlyMemory.cpp's comment on the
// same limitation for other DSi work this port has done). Expect this to
// need a real-hardware pass to tune how far a drag has to travel before it
// reads as a "full" turn.
#define DSI_DIRECT_CAMERA_SCALE 96.0f
#define DSI_DIRECT_CAMERA_INVERT_X 0
// Real-hardware evidence (reported: dragging down on the touch screen makes
// the camera look UP, and vice versa -- inverted from every other input
// path). InputBackend_DSI.cpp's dsiUpdateTouchCameraDelta() computes
// g_touchDeltaY as touch.py - g_prevTouchY, i.e. the DS touch panel's own
// raster convention (py increases DOWN the screen), so a downward drag is a
// POSITIVE raw delta. Entity::turnEntity(yaw, pitch) does
// `rotationPitch -= pitch * 0.15f`, the same convention the PC mouse path
// (mouseHelper->deltaY, LWJGL-style: positive = moved UP) and PS2's own
// analog stick both already satisfy -- PS2_DIRECT_CAMERA_INVERT_Y is 1 for
// exactly this reason (Ps2InputTuning.h). This file's own INVERT_Y was set
// to 0 independently instead of matching PS2's already-correct value, so a
// downward (positive) touch-drag delta reached turnEntity un-negated and
// DECREASED pitch -- looking up on a downward drag. 1 matches PS2's value
// and the sign every other input path already relies on.
#define DSI_DIRECT_CAMERA_INVERT_Y 1
#define DSI_DIRECT_CAMERA_REFERENCE_FPS 60.0f
#define DSI_DIRECT_CAMERA_MAX_DT 0.10f
