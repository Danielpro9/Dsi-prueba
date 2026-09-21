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
#define DSI_DIRECT_CAMERA_INVERT_Y 0
#define DSI_DIRECT_CAMERA_REFERENCE_FPS 60.0f
#define DSI_DIRECT_CAMERA_MAX_DT 0.10f
