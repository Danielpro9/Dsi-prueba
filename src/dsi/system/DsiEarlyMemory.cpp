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
// The 8-9 MB budget, enforced
// -----------------------------------------------------------------------------
// Confirmed against BlocksDS's own docs (docs/internal/memory_map.md section 4,
// docs/guides/usage_notes.md "ARM9 options"): a retail DSi's main RAM is 16 MB
// once the ARM9 binary is built against dsi_arm9.specs (see src/dsi/Makefile)
// and the ROM is actually launched DSi-enhanced -- no SCFG_EXT register poking
// needed from game code, the crt0 that ships with that specs file does it.
// isDSiMode() below confirms it actually happened at boot, since that also
// depends on how the .nds is launched (DSi Menu / a loader that respects the
// DSi-enhanced header), not just on how it was compiled.
//
// The brief wants roughly HALF of that (8-9 MB) actually used, with the rest
// held back as a crash margin -- mirroring the PS2 port using ~14 of its 32 MB.
// reduceHeapSize() is libnds's tool for that: called here before anything has
// allocated, it shrinks the malloc ceiling by a fixed number of bytes, so the
// upper half of main RAM is never handed out no matter what the game does
// later. This makes the budget an enforced ceiling instead of a number game
// code has to remember to respect.
//
// On a plain NDS-compatible build/launch (~3.5-4 MB natural ceiling) this is a
// no-op: the natural ceiling is already below the target, so there is nothing
// to trim.
constexpr u32 DSI_HEAP_BUDGET_TARGET_KB = 8 * 1024; // ~half of 16 MB

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
	       g_heapCeiling / 1024u, DSI_HEAP_BUDGET_TARGET_KB);
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
