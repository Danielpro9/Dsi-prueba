// DsiBringup.cpp — Nintendo DSi toolchain/diagnostic bring-up.
//
// This is NOT the game entry point. It proves BlocksDS/libnds link and boot
// (see src/dsi/Makefile -- this needs BlocksDS, not devkitARM/devkitPro), and
// prints the numbers every later DSi decision in this port depends on:
//
//   1. Whether the ROM is actually running DSi-enhanced (isDSiMode()) and, if
//      so, the enforced malloc heap ceiling. Building against dsi_arm9.specs
//      gets a retail DSi's real 16 MB of main RAM (confirmed against
//      BlocksDS's own docs -- see DsiEarlyMemory.cpp), and DsiEarlyMemory.cpp
//      caps it at a 12 MB budget with reduceHeapSize(), leaving 4 MB as an
//      unprofiled-code safety margin rather than half the console. This is
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
// in DSi mode this should read ~12288 KB (the enforced budget), not the raw
// 16 MB the console actually has -- that's the point of printing it here.
void reportMemory()
{
	const unsigned ceilingKb = (unsigned)(dsiGetHeapCeiling() / 1024u);
	const unsigned committedKb = (unsigned)(dsiGetHeapCommitted() / 1024u);
	std::printf("HEAP    %u KB ceiling (enforced), %u KB committed\n", ceilingKb, committedKb);
	std::printf("BUDGET  12288 KB target (3/4 of 16 MB); PS2 used ~14336 of 32768\n");
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

// Switches from the diagnostic text console into the actual layout the real
// game will boot into (dsiEnsureEarlyVideo(): top screen MODE_0_3D, bottom
// screen left blank so it reads flat black -- see DsiEarlyVideo.cpp) and keeps
// it live so it can actually be looked at, instead of just trusting the
// DsiEarlyVideo.cpp source. The clear color cycles through hue so a frozen/
// black top screen (GL never initialized, wrong VRAM bank, ...) is
// immediately obvious rather than looking identical to "it's just black on
// purpose" -- which is deliberately what the BOTTOM screen looks like right
// next to it, so the contrast is the point.
//
// Deliberately not a rotating 3D shape: that needs gluPerspectivef32/
// gluLookAtf32 fixed-point parameters this port has no way to check against
// real hardware yet (see the compile-only verification note in the DSi
// storage/tuning commits), and a wrong FOV or camera would make a working GL
// setup look broken. A clear-color cycle only exercises glClearColor()+
// glFlush(), which is already proven by this file having reached this point
// at all.
// Triangle wave over a 62-frame period, clamped to glClearColor's 0-31 range:
// 0 -> 31 over the first half, back down to 0 over the second. Three copies of
// this, phase-shifted, stand in for a real HSV-to-RGB conversion -- all that
// matters here is "visibly and continuously changing colour".
int triangleWave31(int phase)
{
	phase = phase % 62;
	return phase < 31 ? phase : 62 - phase;
}

void runVideoDemo()
{
	dsiEnsureEarlyVideo();

	int hue = 0;
	while (true)
	{
		glClearColor((uint8_t)triangleWave31(hue),
		             (uint8_t)triangleWave31(hue + 20),
		             (uint8_t)triangleWave31(hue + 40),
		             31);
		++hue;

		glFlush(0); // waits for vblank and swaps, same as swiWaitForVBlank()

		scanKeys();
		if (keysDown() & KEY_START)
			break;
	}
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

	std::printf("\nA: show top=3D/bottom=black video demo\nSTART: exit\n");

	bool showVideoDemo = false;
	while (true)
	{
		swiWaitForVBlank();
		scanKeys();
		const int down = keysDown();
		if (down & KEY_START)
			break;
		if (down & KEY_A)
		{
			showVideoDemo = true;
			break;
		}
	}

	if (showVideoDemo)
		runVideoDemo(); // returns on START

	return 0;
}
