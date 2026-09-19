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
// Target vs. what this build actually gets
// -----------------------------------------------------------------------------
// The project brief targets an 8-9 MB footprint out of the DSi's 16 MB, mirroring
// the PS2 port's ~14 MB-of-32 usage. That extra RAM only exists on the DSi past
// the original NDS's 4 MB, and reaching it needs the ROM to run DSi-enhanced
// (TWL header) *and* have the loader grant it access to SCFG_EXT -- which a
// title launched normally from the DSi Menu gets, but which most homebrew
// loading paths (flashcards, nds-bootstrap without Unlaunch) do not. Mainline
// devkitPro/libnds does not expose a turnkey "give me the extra 12 MB" call for
// this; BlocksDS is further along here but this bring-up still targets the
// classic devkitARM/libnds toolchain the rest of this file assumes.
//
// So: this file reports the REAL ceiling (getHeapLimit(), the plain newlib sbrk
// top) every boot, rather than asserting the aspirational number. Until the
// DSi-enhanced build path is confirmed working, budget the game to fit standard
// NDS-compatible RAM (DSI_HEAP_BUDGET_FALLBACK_KB below) and treat
// DSI_HEAP_BUDGET_TARGET_KB as the number the chunk-streaming work is sized
// against once extended RAM is actually confirmed live -- see reportExtendedMemoryStatus()
// in DsiBringup.cpp, which is the first thing to run to find out.
constexpr u32 DSI_HEAP_BUDGET_TARGET_KB = 8 * 1024;    // 8 MB, the brief's floor
constexpr u32 DSI_HEAP_BUDGET_FALLBACK_KB = 3 * 1024;  // ~3 MB, safe in NDS-compat mode

namespace DsiEarlyMemory
{

void captureInitialState()
{
	if (g_heapCeiling != 0)
		return;

	g_heapStart = getHeapStart();
	g_heapCeiling = static_cast<u32>(getHeapLimit() - g_heapStart);

	MC_LOG_INFO("dsi", "dsi mode (TWL): %s\n", isDSiMode() ? "yes" : "no");
	MC_LOG_INFO("dsi", "heap ceiling %u KB (target %u KB, fallback budget %u KB)\n",
	       g_heapCeiling / 1024u, DSI_HEAP_BUDGET_TARGET_KB, DSI_HEAP_BUDGET_FALLBACK_KB);
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
