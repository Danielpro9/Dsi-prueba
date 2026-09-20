// Runtime_dsi.cpp — DSi implementation of java/Runtime.h.
//
// Unlike the PS2 version (src/ps2/java/Runtime_ps2.cpp), which has to probe
// sbrk()/mallinfo() and the live stack pointer to approximate its heap
// bounds, DSi already tracks this precisely: DsiEarlyMemory.cpp calls
// reduceHeapSize() at boot to cap the real 16 MB down to a deliberate budget
// (see that file for why), and DsiEarlyInit.h exposes the result directly.
#ifdef DSI_PLATFORM

#include "java/Runtime.h"
#include "dsi/DsiEarlyInit.h"

Runtime Runtime::instance;

Runtime& Runtime::getRuntime()
{
	return instance;
}

long_t Runtime::maxMemory()
{
	return static_cast<long_t>(dsiGetHeapCeiling());
}

long_t Runtime::totalMemory()
{
	return static_cast<long_t>(dsiGetHeapCeiling());
}

long_t Runtime::freeMemory()
{
	const long_t ceiling = static_cast<long_t>(dsiGetHeapCeiling());
	const long_t committed = static_cast<long_t>(dsiGetHeapCommitted());
	return ceiling > committed ? ceiling - committed : 0;
}

#endif // DSI_PLATFORM
