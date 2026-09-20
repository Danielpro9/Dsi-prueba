#pragma once

// Central DSi-only GUI-scale knobs.
//
// Included from platform/PlatformTuning.h (via DsiTuning.h) AFTER the desktop
// baseline table, and only when PLATFORM_DSI is set. PlatformGameTuning.h
// groups DSi with PS2 for most of its ~60-knob table (memory/CPU-shaped
// choices that genuinely do apply to both), but a handful of those PS2
// values are PS2-*screen*-shaped instead, and PS2's screen (640x448) has
// nothing in common with the DSi's (256x192) -- this file is where those
// get their own DSi-appropriate values instead of silently inheriting PS2's.

#if PLATFORM_DSI

// Real-hardware symptom this fixes: the legacy console-style main menu
// (GameSettings::legacyUI) rendered its five button rows as huge, barely
// readable bars covering almost the entire screen. Root cause, traced from
// a real-hardware photo: ScaledResolution.cpp only takes PLATFORM_LEGACY_GUI_
// SCALE if it is > 0.0 (see its ctor); PlatformGameTuning.h's shared PS2/DSi
// branch (never overridden for DSi before this file existed) set it to
// PS2_LEGACY_GUI_SCALE = 2.0 -- tuned so PS2's 640x448 framebuffer maps to a
// 320x224 *virtual* canvas (see Ps2PresentationTuning.h's own comment). On
// the DSi's real 256x192 screen the same /2 divide collapses the legacy
// menu's logical canvas to 128x96, and LegacyMainMenuLayout.cpp's fixed
// pixel floors (a 120px minimum button width, a 16px minimum row height --
// both authored against a much bigger virtual canvas) then dominate that
// shrunken space: a 120-of-128px button is already 94% of the screen width
// before anything else happens. Minecraft.cpp's renderOrtho then stretches
// that already-oversized 128x96 logical geometry across the full 256x192
// physical screen -- a second 2x on top of the first -- which is the
// "bars filling almost the whole screen" in the photo.
//
// 1.0: the legacy canvas stays at the DSi's native 256x192, matching the
// physical screen 1:1 (renderOrtho maps logical:physical at exactly 1:1, no
// stretch), so the existing pixel floors in LegacyMainMenuLayout.cpp land at
// the proportions they were actually designed around (a 120px button out of
// 256 is a reasonable 47%, not 94%).
#undef  PLATFORM_LEGACY_GUI_SCALE
#define DSI_LEGACY_GUI_SCALE                     1.0
#define PLATFORM_LEGACY_GUI_SCALE                DSI_LEGACY_GUI_SCALE

// Same PS2-screen-shaped inheritance, for the *non*-legacy GUI scale this
// time (regular HUD/inventory screens, via ScaledResolution.cpp's
// PLATFORM_CONSOLE_LOW && PLATFORM_FORCE_GUI_SCALE > 0 branch -- DSi
// inherits PLATFORM_CONSOLE_LOW = 1 from the same PS2 grouping, see
// PlatformTuning.h's own comment on that). PS2_FORCE_GUI_SCALE = 2 halves
// PS2's 640x448 down to a 320x224 canvas for the same reason as the legacy
// value above; halving the DSi's 256x192 the same way would hit the exact
// same oversized-HUD bug the moment a world loads, just not observed on
// hardware yet since testing has not gotten past the main menu. Fixed here
// pre-emptively rather than waiting for a second matching bug report: 1
// keeps the non-legacy GUI at native 256x192 too, for the same reason.
#undef  PLATFORM_FORCE_GUI_SCALE
#define DSI_FORCE_GUI_SCALE                      1
#define PLATFORM_FORCE_GUI_SCALE                 DSI_FORCE_GUI_SCALE

#endif // PLATFORM_DSI
