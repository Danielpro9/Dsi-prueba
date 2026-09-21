#include "System.h"

#include <chrono>
#if defined(DSI_PLATFORM)
#include "platform/PlatformCompat.h"
#elif !defined(PS2_PLATFORM) && !defined(WII_PLATFORM)
#include <SDL.h>
#endif

namespace System
{

long_t currentTimeMillis()
{
#if defined(DSI_PLATFORM)
	// std::chrono::system_clock on this toolchain rides newlib's gettimeofday,
	// which platform/PlatformCompat.h and platform/ConsoleInputClock.h already
	// document as unreliable here -- the same disease recorded for PS2's BIOS
	// timer in those same files, just not yet worked around for this specific
	// function. Timer::updateTimer() calls currentTimeMillis()/nanoTime() every
	// single frame to decide how many ticks have elapsed; a clock that is stuck
	// for a stretch of real time and then jumps reads as "no time passed" for
	// several frames followed by a catch-up burst -- consistent with the
	// reported "game pauses for a moment" stalls, and with every duration in
	// every [dsi.perf] log line this session being suspiciously an exact
	// multiple of ~50ms (all of them timed via this function). Routed through
	// the same hardware tick counter PlatformCompat::getTicks() already trusts
	// instead. DSi has no reliable RTC/epoch wired up regardless, and no DSi
	// call site of this function (delta timing, animation cycles, cache
	// timestamps) needs a true calendar date, only a consistent, live,
	// monotonically increasing clock.
	return static_cast<long_t>(PlatformCompat::getTicks());
#else
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
#endif
}

long_t nanoTime()
{
#if defined(DSI_PLATFORM)
	// See currentTimeMillis() above for why std::chrono is avoided here. Reuses
	// PlatformCompat::getMonotonicMicros() (already hardware-backed and already
	// exercised by other DSi budget code) rather than duplicating its tick-to-
	// time conversion a second time; nanoTime()'s contract is nanoseconds, so
	// this is just that value scaled up by 1000.
	return static_cast<long_t>(PlatformCompat::getMonotonicMicros()) * 1000LL;
#else
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

bool openURL(const std::string &url)
{
#if defined(PS2_PLATFORM) || defined(WII_PLATFORM) || defined(DSI_PLATFORM)
	// No browser to hand the URL to on any of the three consoles.
	(void)url;
	return false;
#else
	return SDL_OpenURL(url.c_str()) == 0;
#endif
}

}
