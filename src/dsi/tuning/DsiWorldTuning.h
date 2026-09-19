#pragma once


// Central DSi-only chunk streaming/cache knobs.
//
// Included from platform/PlatformTuning.h AFTER the desktop baseline table, and
// only when PLATFORM_DSI is set. This is an override file, not a third copy of
// the ~60-knob table: it only touches the subset that decides how many chunks
// stay resident and how aggressively they get written out to the SD card and
// freed. The much larger group of render/mesh-timing knobs (PLATFORM_MESH_BUDGET,
// PLATFORM_CHUNK_BUILD_BUDGET_MS, PLATFORM_RENDERER_UPDATE_CANDIDATES_PER_FRAME,
// ...) is deliberately left on the desktop defaults for now -- tuning those needs
// real frame-time measurements on device/in an emulator, which nothing in this
// repo can produce yet. Revisit once src/dsi/Makefile actually links (see the
// bring-up notes in DsiEarlyMemory.cpp/DsiBringup.cpp).
//
// The budget these numbers are sized against, and why they are tighter than PS2
// ------------------------------------------------------------------------------
// DsiEarlyMemory.cpp enforces an 8 MB heap ceiling total (code, textures, GL
// state, entities, the Java-side object heap -- everything, not just chunks).
// PS2's own comment in ps2/tuning/Ps2CoreTuning.h records a MEASURED number for
// its chunk cache: radius 2 with 3 vertical sections (25 columns x 3 = 75
// renderer slots) cost ~8 MB BY ITSELF, out of a 32 MB total budget. That is
// PS2's entire chunk allowance before touching anything else, and DSi's ENTIRE
// budget is smaller than that one number. Copying PS2's radius as-is would
// alone exceed what DSi has for the whole game.
//
// So this file does not reuse PS2's radius/vertical-count values, only its
// eviction-rate ones (which are already the tightest in the table and are not a
// function of how much RAM the console has, just how much I/O stall per tick is
// tolerable). The radius/vertical-count numbers below are a first estimate, NOT
// a measurement: scaled from PS2's one real data point by slot count --
// (1 chunk radius, 3x3=9 columns) x (2 vertical sections) = 18 slots, against
// PS2's 75 -- i.e. roughly a quarter of PS2's ~8 MB, call it ~2 MB, leaving
// headroom in the 8 MB budget for everything else. Treat every number here as
// "unverified, needs a real run" until DsiBringup.cpp (or a chunk-loaded version
// of it) has actually printed a measured heap number back. See the corresponding
// comment in DsiEarlyMemory.cpp.

#if PLATFORM_DSI
#define PLATFORM_BOUNDED_WORLD 1

// 3x3 columns rendered/cached around the player, 2 vertical sections centred on
// them. This is a genuinely short view distance -- shorter than PS2's already
// short 5x5 -- and is the direct, visible cost of an 8 MB total budget rather
// than PS2's 32 MB. If a real run shows headroom, raising this to PS2's radius 2
// is the first knob to try, one step at a time, watching the enforced heap
// ceiling in DsiEarlyMemory.cpp as the fitness function.
#undef  PLATFORM_VISIBLE_CHUNK_RADIUS
#define PLATFORM_VISIBLE_CHUNK_RADIUS            1
#undef  PLATFORM_VERTICAL_CHUNK_COUNT
#define PLATFORM_VERTICAL_CHUNK_COUNT            2
#undef  PLATFORM_CENTER_VERTICAL_RENDERERS
#define PLATFORM_CENTER_VERTICAL_RENDERERS       1

// Streaming window equal to the visible radius (PS2 settled on the same choice
// for the same reason -- see the "Back at radius 2 for memory" note in
// Ps2CoreTuning.h): a bigger cache radius buys lead time for prefetching, but
// every extra ring is columns resident in RAM, and RAM is what is missing here.
// Unload radius keeps PS2's one-larger hysteresis margin so crossing a chunk
// border does not immediately delete/reload the column just left.
#undef  PLATFORM_CHUNK_CACHE_RADIUS
#define PLATFORM_CHUNK_CACHE_RADIUS              1
#undef  PLATFORM_CHUNK_UNLOAD_RADIUS
#define PLATFORM_CHUNK_UNLOAD_RADIUS             2
#undef  PLATFORM_CHUNK_MAP_RESERVE
#define PLATFORM_CHUNK_MAP_RESERVE               16

// Eviction rate: reused from PS2 as-is. These bound how many chunks unload
// (write to SD + free) per tick, not how much RAM the cache holds, so they do
// not scale with the smaller radius above -- they exist to keep a save-to-SD
// burst out of a single frame regardless of cache size. PS2_MIN_UNUSED_TICKS_
// BEFORE_UNLOAD is halved from PS2's 60: DSi's cache is already a third the
// size, so a stale column needs to leave sooner to keep the resident set small,
// at the cost of writing to the SD card somewhat more often. Unverified without
// a measured SD write latency -- raise it if that turns out to matter.
#undef  PLATFORM_MAX_CHUNK_UNLOADS_PER_TICK
#define PLATFORM_MAX_CHUNK_UNLOADS_PER_TICK      1
#undef  PLATFORM_EMERGENCY_CHUNK_UNLOADS_PER_TICK
#define PLATFORM_EMERGENCY_CHUNK_UNLOADS_PER_TICK 2
#undef  PLATFORM_MIN_UNUSED_TICKS_BEFORE_UNLOAD
#define PLATFORM_MIN_UNUSED_TICKS_BEFORE_UNLOAD  30

// Read-only worlds (prebuilt maps) never need to keep a dirty chunk resident
// for a later save, so they can use a faster unload throttle than a normal
// world -- same values PS2 uses, since this is again about I/O-stall shape, not
// the radius.
#undef  PLATFORM_READ_ONLY_CHUNK_CACHE_RADIUS
#define PLATFORM_READ_ONLY_CHUNK_CACHE_RADIUS              PLATFORM_CHUNK_CACHE_RADIUS
#undef  PLATFORM_READ_ONLY_CHUNK_UNLOAD_RADIUS
#define PLATFORM_READ_ONLY_CHUNK_UNLOAD_RADIUS             PLATFORM_CHUNK_UNLOAD_RADIUS
#undef  PLATFORM_READ_ONLY_MAX_CHUNK_UNLOADS_PER_TICK
#define PLATFORM_READ_ONLY_MAX_CHUNK_UNLOADS_PER_TICK      4
#undef  PLATFORM_READ_ONLY_MIN_UNUSED_TICKS_BEFORE_UNLOAD
#define PLATFORM_READ_ONLY_MIN_UNUSED_TICKS_BEFORE_UNLOAD  10

// New-world preload: one chunk each way (16 blocks), half of PS2's 32 -- the
// preload window should not be bigger than the cache radius it is filling.
#undef  PLATFORM_PRELOAD_RADIUS_BLOCKS
#define PLATFORM_PRELOAD_RADIUS_BLOCKS           16

// Vanilla will not tick an entity unless a 32-block area around it is loaded.
// With a 3x3 chunk cache that guard can starve the local player's own ticking
// (see the identical PS2 comment in Ps2CoreTuning.h -- same cause, same fix):
// only require the entity's own chunk to be loaded.
#undef  PLATFORM_PLAYER_UPDATE_CHUNK_RANGE_BLOCKS
#define PLATFORM_PLAYER_UPDATE_CHUNK_RANGE_BLOCKS 0

// SD card write latency through BlocksDS's FatFs has not been measured yet on
// this port. Default to PS2's conservative answer (no background autosave,
// explicit save only) rather than guessing that a microSD card is "obviously
// fast enough" -- if a measured run shows the incremental save path is cheap
// here, this is the knob to flip back on.
#undef  PLATFORM_DISABLE_RUNTIME_AUTOSAVE
#define PLATFORM_DISABLE_RUNTIME_AUTOSAVE        1
#undef  PLATFORM_SKIP_NEW_WORLD_FULL_SAVE
#define PLATFORM_SKIP_NEW_WORLD_FULL_SAVE        1
#undef  PLATFORM_INCREMENTAL_CHUNK_SAVE_LIMIT
#define PLATFORM_INCREMENTAL_CHUNK_SAVE_LIMIT    2

#endif // PLATFORM_DSI
