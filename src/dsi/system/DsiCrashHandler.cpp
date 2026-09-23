// DsiCrashHandler.cpp -- real-hardware crash triage.
//
// Motivating evidence: a real-hardware debug.log ended mid-session with no
// warning of any kind -- the last lines were an ordinary world load
// finishing ("worldload end"), then nothing, not even the next ~1s
// heartbeat. MC_LOG_SYNC_WRITES commits every INFO/WARN/ERROR line
// immediately (see Log.h), so that is not a buffering artifact: nothing
// else ran that logged anything before the console stopped responding.
// That shape -- total silence, not a slow decline -- points at a hardware
// exception (an invalid memory access, most likely) rather than the
// gradual heap/VRAM growth memtrend already tracks.
//
// EXCEPTION_VECTOR (nds/exceptions.h) is 0 by default -- "no handler,
// mirror" -- so with nothing installed, a real exception falls through to
// whatever the BIOS does with that, which is exactly this: silent death,
// no trace. libnds already ships the pieces to do better (setExceptionHandler,
// exceptionRegisters[]; see nds/exceptions.h and its own
// defaultExceptionHandler()/guruMeditationDump(), which print the same
// information to a text console). This reuses that same captured state but
// writes it to debug.log instead of the screen: the bottom screen is meant
// to stay flat black in the real game (see DsiBootstrap.cpp), so a console
// dump would never reach anyone, while the log file is the one artifact
// that actually gets sent back after a crash.
#ifdef DSI_PLATFORM
#include "dsi/system/DsiCrashHandler.h"

#include "platform/Log.h"
#include "dsi/DsiEarlyInit.h"

#include <nds.h>
#include <nds/cpu_asm.h>

#include <cstdint>

namespace
{

// Runs in the context enterException (exceptionHandler.s) sets up: its own
// stack (exceptionStack, top at a fixed address in main RAM), CPU switched
// back to a mode that can run ordinary C/C++, exceptionRegisters[0..15]
// holding r0-r15 as they were at the moment of the fault. Same register
// layout guruMeditationDump() (libnds's own handler) decodes -- pc needs the
// same mode-dependent offset subtracted to point at the faulting
// instruction rather than one or two instructions past it.
void logCrashAndHalt()
{
	const std::uint32_t currentMode = getCPSR() & CPSR_MODE_MASK;
	const std::uint32_t thumbState = *(EXCEPTION_STACK_TOP - 3) & CPSR_FLAG_T;
	const int offset = thumbState ? 2 : 4;
	const std::uint32_t faultPc = static_cast<std::uint32_t>(exceptionRegisters[15]) - offset;

	const char* description = "Unknown error";
	if (currentMode == CPSR_MODE_ABORT)
		description = "Data abort";
	else if (currentMode == CPSR_MODE_UNDEFINED)
		description = "Undefined instruction";

	MC_LOG_ERROR("dsi", "*** CRASH: %s at pc=0x%08lx (thumb=%d) ***\n",
		description, (unsigned long)faultPc, thumbState ? 1 : 0);
	MC_LOG_ERROR("dsi", "  r0=%08lx r1=%08lx r2=%08lx r3=%08lx\n",
		(unsigned long)exceptionRegisters[0], (unsigned long)exceptionRegisters[1],
		(unsigned long)exceptionRegisters[2], (unsigned long)exceptionRegisters[3]);
	MC_LOG_ERROR("dsi", "  r4=%08lx r5=%08lx r6=%08lx r7=%08lx\n",
		(unsigned long)exceptionRegisters[4], (unsigned long)exceptionRegisters[5],
		(unsigned long)exceptionRegisters[6], (unsigned long)exceptionRegisters[7]);
	MC_LOG_ERROR("dsi", "  r8=%08lx r9=%08lx r10=%08lx r11=%08lx\n",
		(unsigned long)exceptionRegisters[8], (unsigned long)exceptionRegisters[9],
		(unsigned long)exceptionRegisters[10], (unsigned long)exceptionRegisters[11]);
	MC_LOG_ERROR("dsi", "  r12=%08lx sp=%08lx lr=%08lx pc(raw)=%08lx\n",
		(unsigned long)exceptionRegisters[12], (unsigned long)exceptionRegisters[13],
		(unsigned long)exceptionRegisters[14], (unsigned long)exceptionRegisters[15]);
	MC_LOG_ERROR("dsi", "  at-crash heap=%u/%uKB vram=%u/512KB\n",
		(unsigned)(dsiGetHeapCommitted() / 1024u), (unsigned)(dsiGetHeapCeiling() / 1024u),
		(unsigned)(dsiTotalTextureVramBytes() / 1024u));

	// Same reasoning as libnds's own guruMeditationDump(): nothing after an
	// exception can be assumed safe to run (a corrupted return address, a
	// blown stack, ...), so stop here instead of trying to continue or exit
	// cleanly.
	while (true)
	{
	}
}

} // namespace

void dsiInstallCrashHandler()
{
	setExceptionHandler(logCrashAndHalt);
}

#endif // DSI_PLATFORM
