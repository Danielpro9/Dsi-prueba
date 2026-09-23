#pragma once
#ifdef DSI_PLATFORM

// Installs a hardware exception handler (ARM9 data abort / undefined
// instruction) that logs CPU register state to debug.log before halting.
// See DsiCrashHandler.cpp for why: without this, a real-hardware crash
// leaves nothing behind except "the log stopped here".
void dsiInstallCrashHandler();

#endif
