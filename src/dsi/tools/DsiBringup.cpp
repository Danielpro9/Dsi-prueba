// DsiBringup.cpp — Nintendo DSi toolchain/diagnostic bring-up.
//
// This is NOT the game entry point. It proves BlocksDS/libnds link and boot
// (see src/dsi/Makefile -- this needs BlocksDS, not devkitARM/devkitPro), and
// prints the numbers every later DSi decision in this port depends on:
//
//   1. Whether the ROM is actually running DSi-enhanced (isDSiMode()) and, if
//      so, the enforced malloc heap ceiling. Building against dsi_arm9.specs
//      gets a retail DSi's real 16 MB of main RAM (confirmed against
//      BlocksDS's own docs -- see DsiEarlyMemory.cpp), and
//      DsiEarlyMemory.cpp caps it at the project brief's 8-9 MB budget with
//      reduceHeapSize(), leaving the rest untouched as crash margin. This is
//      the first place that confirms that actually happened on real
//      hardware/an emulator instead of the game silently assuming it.
//   2. Byte order and type sizes. The ARM9 is little-endian, like the PS2's EE
//      and unlike the Wii's Broadway, so NBT/region files should need no new
//      branch -- but this is the cheapest place to confirm it rather than
//      assume it.
//   3. Whether the SD card mounts at all, which is where chunk streaming (see
//      the project brief) will read and write.
//
// Uses its own console-friendly video setup (sub screen as a text console)
// instead of dsiEnsureEarlyVideo(), which blanks the bottom screen for the real
// game. Build this file instead of the game's own main to run the smoke test.

#include <nds.h>
#include <fat.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "dsi/DsiEarlyInit.h"

namespace
{

void initVideoAndConsole()
{
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleDemoInit(); // text console on the sub (bottom) screen only
}

void reportDsiMode()
{
	std::printf("MODE    %s\n", isDSiMode() ? "DSi-enhanced (TWL)" : "NDS-compatible");
}

// dsiGetHeapCeiling() already applies DsiEarlyMemory's reduceHeapSize() cap, so
// in DSi mode this should read ~8192 KB (the enforced budget), not the raw
// 16 MB the console actually has -- that's the point of printing it here.
void reportMemory()
{
	const u32 ceilingKb = dsiGetHeapCeiling() / 1024u;
	const u32 committedKb = dsiGetHeapCommitted() / 1024u;
	std::printf("HEAP    %u KB ceiling (enforced), %u KB committed\n", ceilingKb, committedKb);
	std::printf("BUDGET  8192 KB target; PS2 used ~14336 of 32768\n");
}

void reportByteOrder()
{
	const unsigned int probe = 0x01020304u;
	unsigned char bytes[4];
	std::memcpy(bytes, &probe, sizeof(bytes));
	const bool littleEndianAtRuntime = (bytes[0] == 0x04);

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
	const bool littleEndianAtCompileTime = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
#else
	const bool littleEndianAtCompileTime = true;
#endif

	std::printf("ENDIAN  runtime=%s compile=%s %s\n",
	            littleEndianAtRuntime ? "LE" : "BE",
	            littleEndianAtCompileTime ? "LE" : "BE",
	            (littleEndianAtRuntime == littleEndianAtCompileTime) ? "OK" : "*** MISMATCH ***");

	std::printf("TYPES   float=%u double=%u long=%u long long=%u ptr=%u\n",
	            (unsigned)sizeof(float), (unsigned)sizeof(double),
	            (unsigned)sizeof(long), (unsigned)sizeof(long long),
	            (unsigned)sizeof(void*));
}

void reportStorage()
{
	if (!fatInitDefault())
	{
		std::printf("STORAGE *** fatInitDefault() FAILED -- no SD card ***\n");
		return;
	}

	char cwd[128];
	if (getcwd(cwd, sizeof(cwd)))
		std::printf("STORAGE launched from %s\n", cwd);
	else
		std::printf("STORAGE mounted, no launch cwd\n");

	// Write probe: chunk streaming (project brief) lives or dies on the card
	// actually being writable, not just present.
	FILE* probe = std::fopen("write_probe.tmp", "wb");
	if (probe)
	{
		std::fputs("ok", probe);
		std::fclose(probe);
		std::remove("write_probe.tmp");
		std::printf("STORAGE write probe OK\n");
	}
	else
	{
		std::printf("STORAGE *** write probe FAILED (read-only or full) ***\n");
	}
}

} // namespace

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	initVideoAndConsole();

	std::printf("OptiCraft - DSi bring-up\n");
	std::printf("---------------------------\n\n");

	reportDsiMode();
	reportMemory();
	reportByteOrder();
	reportStorage();

	std::printf("\nSTART to exit.\n");

	while (true)
	{
		swiWaitForVBlank();
		scanKeys();
		if (keysDown() & KEY_START)
			break;
	}

	return 0;
}
