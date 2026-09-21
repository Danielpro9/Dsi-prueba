#pragma once

// Included directly (not via the DsiTuning.h umbrella, which is pulled in
// later than this file -- see PlatformTuning.h's include order) so the DSI_
// DIRECT_* constants below are already defined by the time the PLATFORM_DSI
// branch references them. Same reasoning as Ps2Tuning.h being pulled in
// early enough for the PLATFORM_PS2 branch's PS2_DIRECT_* references.
#if PLATFORM_DSI
#  include "dsi/tuning/DsiInputTuning.h"
#endif

// -----------------------------------------------------------------------------
// Input policy aliases
// -----------------------------------------------------------------------------
#if PLATFORM_PS2
#  define PLATFORM_ANALOG_MOVE_DEADZONE       PS2_DIRECT_MOVE_DEADZONE
#  define PLATFORM_ANALOG_MOVE_SCALE          PS2_DIRECT_MOVE_SCALE
#  define PLATFORM_DIRECT_CAMERA_ENABLED      PS2_DIRECT_PAD_CAMERA
#  define PLATFORM_DIRECT_CAMERA_DEADZONE     PS2_DIRECT_CAMERA_DEADZONE
#  define PLATFORM_DIRECT_CAMERA_SCALE        PS2_DIRECT_CAMERA_SCALE
#  define PLATFORM_DIRECT_CAMERA_INVERT_X     PS2_DIRECT_CAMERA_INVERT_X
#  define PLATFORM_DIRECT_CAMERA_INVERT_Y     PS2_DIRECT_CAMERA_INVERT_Y
#  define PLATFORM_DIRECT_CAMERA_REFERENCE_FPS PS2_DIRECT_CAMERA_REFERENCE_FPS
#  define PLATFORM_DIRECT_CAMERA_MAX_DT       PS2_DIRECT_CAMERA_MAX_DT
#elif PLATFORM_DSI
// D-pad -> movement, touch screen -> camera. See DsiInputTuning.h for the
// constants and InputBackend_DSI.cpp's platformGamepadSnapshot() for how the
// D-pad/touch state is turned into the stick-shaped values this scheme
// expects (MovementInputFromOptions.cpp / EntityRenderer.cpp), the same
// PLATFORM_DIRECT_ANALOG_MOVEMENT / PLATFORM_DIRECT_CAMERA_ENABLED path PS2
// already uses for its analog stick.
#  define PLATFORM_ANALOG_MOVE_DEADZONE       0.0f
#  define PLATFORM_ANALOG_MOVE_SCALE          DSI_DIRECT_MOVE_SCALE
#  define PLATFORM_DIRECT_CAMERA_ENABLED      1
#  define PLATFORM_DIRECT_CAMERA_DEADZONE     0.0f
#  define PLATFORM_DIRECT_CAMERA_SCALE        DSI_DIRECT_CAMERA_SCALE
#  define PLATFORM_DIRECT_CAMERA_INVERT_X     DSI_DIRECT_CAMERA_INVERT_X
#  define PLATFORM_DIRECT_CAMERA_INVERT_Y     DSI_DIRECT_CAMERA_INVERT_Y
#  define PLATFORM_DIRECT_CAMERA_REFERENCE_FPS DSI_DIRECT_CAMERA_REFERENCE_FPS
#  define PLATFORM_DIRECT_CAMERA_MAX_DT       DSI_DIRECT_CAMERA_MAX_DT
#else
#  define PLATFORM_ANALOG_MOVE_DEADZONE       0.20f
#  define PLATFORM_ANALOG_MOVE_SCALE          1.0f
#  define PLATFORM_DIRECT_CAMERA_ENABLED      0
#  define PLATFORM_DIRECT_CAMERA_DEADZONE     0.18f
#  define PLATFORM_DIRECT_CAMERA_SCALE        96.0f
#  define PLATFORM_DIRECT_CAMERA_INVERT_X     0
#  define PLATFORM_DIRECT_CAMERA_INVERT_Y     0
#  define PLATFORM_DIRECT_CAMERA_REFERENCE_FPS 60.0f
#  define PLATFORM_DIRECT_CAMERA_MAX_DT       0.10f
#endif
