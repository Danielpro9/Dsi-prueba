#include "platform/Profiler.h"

#ifdef DSI_PLATFORM

#include "platform/PlatformCompat.h"

// Minimal for now: only render-phase timing is wired up (cheap -- one
// getMonotonicMicros() call at each end of a begin/end pair, and dropped on
// the floor rather than accumulated into a report). The rest are no-ops.
// Wii's/PS2's equivalents accumulate all of this into an on-screen/logged
// performance window (see ClientProfilerBackend_WII.cpp), which is real
// engineering work worth doing once there is a running frame loop to profile
// and a reason to believe any particular number here matters -- not before.
std::uint32_t platformProfileRenderPhaseBegin()
{
	return static_cast<std::uint32_t>(PlatformCompat::getMonotonicMicros());
}

void platformProfileRenderPhaseEnd(std::uint32_t, PlatformRenderPhase)
{
}

void platformProfileTickPhase(const char*, long long) {}
void platformProfileChunkBuild(long long, int) {}
void platformProfileChunkMeshPass(int, long long, int) {}
void platformProfileSnowColumn(bool, bool, int) {}
void platformProfilePopulatePhase(PlatformPopulatePhase, long long) {}
void platformProfileChunkLoad(long long) {}
void platformProfilePopulate(long long) {}
void platformProfileGenerate(long long) {}
void platformProfileMesh(long long) {}
void platformProfileUnloadSave(long long) {}
void platformProfileTickUpdates(long long) {}
void platformProfileTickQueue(long long) {}
void platformProfileMobSpawn(long long) {}
void platformProfileSaveWorldInfo(long long) {}
void platformProfileMapStorage(long long) {}
void platformProfileChunkEvict(long long) {}

#endif // DSI_PLATFORM
