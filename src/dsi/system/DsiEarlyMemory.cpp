#ifdef DSI_PLATFORM

#include "platform/Log.h"
#include "dsi/system/DsiEarlyMemory.h"
#include "dsi/DsiEarlyInit.h"

#include <nds.h>

namespace
{
u32 g_heapCeiling = 0;
u8* g_heapStart = nullptr;
}

// -----------------------------------------------------------------------------
// The budget, enforced
// -----------------------------------------------------------------------------
// Confirmed against BlocksDS's own docs (docs/internal/memory_map.md section 4,
// docs/guides/usage_notes.md "ARM9 options"): a retail DSi's main RAM is a real,
// usable 16 MB once the ARM9 binary is built against dsi_arm9.specs (see
// src/dsi/Makefile) and the ROM is actually launched DSi-enhanced -- no
// SCFG_EXT register poking needed from game code, the crt0 that ships with that
// specs file does it. isDSiMode() below confirms it actually happened at boot,
// since that also depends on how the .nds is launched (DSi Menu / a loader
// that respects the DSi-enhanced header), not just on how it was compiled. So
// the full 16 MB genuinely is available -- this is a deliberate choice to use
// less of it, not a hardware/toolchain limit.
//
// Raised from an initial 8 MB (half of 16, mirroring the PS2 port's ~14-of-32)
// to 12 MB (three quarters) after discussion: a DSi-enhanced title fully owns
// the console the way most PS2 games do too -- there is no background menu it
// has to coexist with, a soft reset is the way out either way -- so there is
// less reason to hold back half of it than the original PS2-ratio guess
// assumed.
//
// Raised again, 12 -> 15 MB, once real hardware finally reported actual
// numbers: a debug.log heartbeat trace covering normal single-player
// exploration (chunk loads, a death/respawn cycle, several minutes of play)
// peaked at ~8059/12288 KB -- comfortably under the old ceiling the whole
// time, let alone the full 16 MB. That is exactly the "once a real run
// reports actual resident/committed numbers, move this" trigger the previous
// version of this comment was waiting for. 15 MB keeps a deliberate 1 MB
// margin rather than claiming the last byte: still a cushion against our own
// bugs (leaks, fragmentation, an unexpectedly large allocation), just a
// smaller one now that real usage data says the game is nowhere near the
// ceiling. Revisit downward if a future run shows instability near the new
// ceiling instead.
//
// reduceHeapSize() is libnds's tool for enforcing this: called here before
// anything has allocated, it shrinks the malloc ceiling by a fixed number of
// bytes, so whatever is held back is never handed out no matter what the game
// does later -- an enforced ceiling, not a number game code has to remember to
// respect.
//
// On a plain NDS-compatible build/launch (~3.5-4 MB natural ceiling) this is a
// no-op: the natural ceiling is already below the target, so there is nothing
// to trim.
constexpr u32 DSI_HEAP_BUDGET_TARGET_KB = 15 * 1024; // 15 of 16 MB, 1 MB safety margin

namespace DsiEarlyMemory
{

void captureInitialState()
{
	if (g_heapCeiling != 0)
		return;

	g_heapStart = getHeapStart();
	u32 ceilingBytes = static_cast<u32>(getHeapLimit() - g_heapStart);

	const u32 targetBytes = DSI_HEAP_BUDGET_TARGET_KB * 1024u;
	if (ceilingBytes > targetBytes)
	{
		reduceHeapSize(ceilingBytes - targetBytes);
		ceilingBytes = static_cast<u32>(getHeapLimit() - g_heapStart);
	}
	g_heapCeiling = ceilingBytes;

	MC_LOG_INFO("dsi", "dsi mode (TWL): %s\n", isDSiMode() ? "yes" : "no");
	MC_LOG_INFO("dsi", "heap ceiling %u KB (budget %u KB)\n",
	       (unsigned)(g_heapCeiling / 1024u), (unsigned)DSI_HEAP_BUDGET_TARGET_KB);
}

} // namespace DsiEarlyMemory

u32 dsiGetHeapCeiling()
{
	DsiEarlyMemory::captureInitialState();
	return g_heapCeiling;
}

u32 dsiGetHeapCommitted()
{
	DsiEarlyMemory::captureInitialState();
	return static_cast<u32>(getHeapEnd() - g_heapStart);
}

#endif // DSI_PLATFORM
