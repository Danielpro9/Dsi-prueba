#include "platform/Profiler.h"

#ifdef DSI_PLATFORM

#include "platform/PlatformCompat.h"
#include "platform/Log.h"
#include <cstring>
#include <cstdio>

// Minimal for now: only render-phase timing and named tick phases are wired
// up. The rest are no-ops. Wii's/PS2's equivalents accumulate all of this
// into an on-screen/logged performance window (see
// ClientProfilerBackend_WII.cpp), which is real engineering work worth doing
// once there is a running frame loop to profile and a reason to believe any
// particular number here matters -- not before.
std::uint32_t platformProfileRenderPhaseBegin()
{
	return static_cast<std::uint32_t>(PlatformCompat::getMonotonicMicros());
}

void platformProfileRenderPhaseEnd(std::uint32_t, PlatformRenderPhase)
{
}

namespace
{
// Real-hardware data (GuiMainMenu.cpp's own [dsi.perf] section timing, added
// to chase the render-side cost) showed the title-texture reupload fix
// dropped "render" from ~2s/frame to a few hundred ms -- and the very next
// test showed "tick" had become the new dominant cost instead, at up to
// several seconds a frame, with no breakdown of what inside Minecraft::
// runTick() is spending it. Minecraft.cpp already calls
// ClientProfiler::tickPhase(name, ns) once per named phase every tick
// ("stats", "mouseOver", "dynTex", ...) -- this was just discarding it. Same
// window-and-report shape as ClientProfilerBackend_DSI.cpp's frame/tick/
// render line and GuiMainMenu.cpp's menu-section line: accumulate per name,
// log one averaged line every 20 ticks (using "stats" as the once-per-tick
// marker -- it is the first call runTick() makes, unconditionally, every
// single tick).
constexpr int kTickPhaseSlots = 16;
constexpr int kTickPhaseNameChars = 16;
struct DsiTickPhase
{
	char name[kTickPhaseNameChars] = {};
	long long sumNs = 0;
	long long maxNs = 0;
};
DsiTickPhase g_tickPhases[kTickPhaseSlots];
int g_tickPhaseCount = 0;
int g_tickCount = 0;

void recordTickPhaseSample(const char* name, long long ns)
{
	for (int i = 0; i < g_tickPhaseCount; ++i)
	{
		DsiTickPhase& slot = g_tickPhases[i];
		if (std::strncmp(slot.name, name, kTickPhaseNameChars - 1) == 0)
		{
			slot.sumNs += ns;
			if (ns > slot.maxNs) slot.maxNs = ns;
			return;
		}
	}
	if (g_tickPhaseCount >= kTickPhaseSlots)
		return;
	DsiTickPhase& slot = g_tickPhases[g_tickPhaseCount++];
	std::strncpy(slot.name, name, kTickPhaseNameChars - 1);
	slot.name[kTickPhaseNameChars - 1] = '\0';
	slot.sumNs = ns;
	slot.maxNs = ns;
}

void reportTickPhasesIfDue()
{
	if (g_tickCount < 20)
		return;

	char line[512];
	int len = 0;
	for (int i = 0; i < g_tickPhaseCount && len < (int)sizeof(line) - 40; ++i)
	{
		const DsiTickPhase& slot = g_tickPhases[i];
		len += std::snprintf(line + len, sizeof(line) - len, " %s=%ld/%ldms",
			slot.name, (long)(slot.sumNs / g_tickCount / 1000000LL), (long)(slot.maxNs / 1000000LL));
	}
	MC_LOG_INFO("dsi.perf", "tickphase%s\n", line);

	for (int i = 0; i < g_tickPhaseCount; ++i)
		g_tickPhases[i] = DsiTickPhase{};
	g_tickPhaseCount = 0;
	g_tickCount = 0;
}
}

void platformProfileTickPhase(const char* name, long long ns)
{
	if (name == nullptr)
		return;
	if (ns < 0)
		ns = 0;
	if (std::strcmp(name, "stats") == 0)
		++g_tickCount;
	recordTickPhaseSample(name, ns);
	reportTickPhasesIfDue();
}
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
