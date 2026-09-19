#include "platform/ClientProfilerBackend.h"

#ifdef DSI_PLATFORM

#include "platform/Log.h"

// Minimal for now: logs one line every ~10 seconds with the frame/tick/render
// average and worst times, the same window length ClientProfilerBackend_WII.cpp
// uses. Everything that file also tracks (render-phase breakdown, populate-
// phase breakdown, present-slip detection, ...) is real engineering work worth
// doing once there is a running frame loop on real hardware to actually
// profile -- see Profiler_DSI.cpp's comment for the same reasoning.
namespace
{
struct DsiWindow
{
	long long startedMs = 0;
	long long frameNs = 0, maxFrameNs = 0;
	long long tickNs = 0, maxTickNs = 0;
	long long renderNs = 0, maxRenderNs = 0;
	int frames = 0;
} g_dsi;
}

namespace ClientProfilerBackend
{
void frameBegin() {}
void ticks(long long, int) {}
void lighting(long long) {}
void displayUpdate(long long) {}
void render(long long) {}

void frameEnd(long long frameNs, long long tickNs, long long renderNs,
              int, int, World*, RenderGlobal*)
{
	g_dsi.frameNs += frameNs;
	g_dsi.tickNs += tickNs;
	g_dsi.renderNs += renderNs;
	if (frameNs > g_dsi.maxFrameNs) g_dsi.maxFrameNs = frameNs;
	if (tickNs > g_dsi.maxTickNs) g_dsi.maxTickNs = tickNs;
	if (renderNs > g_dsi.maxRenderNs) g_dsi.maxRenderNs = renderNs;
	++g_dsi.frames;

	// currentTimeMillis()-style windowing lives in java/System.h, which pulls
	// in more of the shared engine than this minimal placeholder needs; a
	// frame count is a fine enough proxy for "log roughly every 10 seconds"
	// at any frame rate this port will plausibly hit.
	if (g_dsi.frames < 300)
		return;

	MC_LOG_INFO("dsi.perf", "frames=%d frame=%ld/%ldms tick=%ld/%ldms render=%ld/%ldms\n",
	            g_dsi.frames,
	            (long)(g_dsi.frameNs / g_dsi.frames / 1000000LL), (long)(g_dsi.maxFrameNs / 1000000LL),
	            (long)(g_dsi.tickNs / g_dsi.frames / 1000000LL), (long)(g_dsi.maxTickNs / 1000000LL),
	            (long)(g_dsi.renderNs / g_dsi.frames / 1000000LL), (long)(g_dsi.maxRenderNs / 1000000LL));

	g_dsi = DsiWindow{};
}
}

#endif // DSI_PLATFORM
