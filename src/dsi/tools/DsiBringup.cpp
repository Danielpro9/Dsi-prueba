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
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "dsi/DsiEarlyInit.h"
#include "platform/RenderAPI.h"

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

// One RGBA8-coloured vertex, laid out exactly how RenderInterleavedMesh
// expects (position first, then whatever optional attributes are flagged --
// see RenderAPI.h). No padding: 3 floats (12 bytes) + 4 bytes is already a
// multiple of 4, so sizeof(CubeVertex) == stride with no gaps to account for.
struct CubeVertex
{
	float x, y, z;
	std::uint8_t r, g, b, a;
};

// A unit cube, one solid colour per face, so a wrongly-transformed or
// wrongly-wound face is immediately obvious as "wrong colour where I didn't
// expect it" rather than needing a texture to notice anything is off.
const CubeVertex kCubeVertices[24] = {
	// Front (+Z) red
	{-0.5f, -0.5f,  0.5f, 255, 0, 0, 255}, { 0.5f, -0.5f,  0.5f, 255, 0, 0, 255},
	{ 0.5f,  0.5f,  0.5f, 255, 0, 0, 255}, {-0.5f,  0.5f,  0.5f, 255, 0, 0, 255},
	// Back (-Z) green
	{ 0.5f, -0.5f, -0.5f, 0, 255, 0, 255}, {-0.5f, -0.5f, -0.5f, 0, 255, 0, 255},
	{-0.5f,  0.5f, -0.5f, 0, 255, 0, 255}, { 0.5f,  0.5f, -0.5f, 0, 255, 0, 255},
	// Right (+X) blue
	{ 0.5f, -0.5f,  0.5f, 0, 0, 255, 255}, { 0.5f, -0.5f, -0.5f, 0, 0, 255, 255},
	{ 0.5f,  0.5f, -0.5f, 0, 0, 255, 255}, { 0.5f,  0.5f,  0.5f, 0, 0, 255, 255},
	// Left (-X) yellow
	{-0.5f, -0.5f, -0.5f, 255, 255, 0, 255}, {-0.5f, -0.5f,  0.5f, 255, 255, 0, 255},
	{-0.5f,  0.5f,  0.5f, 255, 255, 0, 255}, {-0.5f,  0.5f, -0.5f, 255, 255, 0, 255},
	// Top (+Y) magenta
	{-0.5f,  0.5f,  0.5f, 255, 0, 255, 255}, { 0.5f,  0.5f,  0.5f, 255, 0, 255, 255},
	{ 0.5f,  0.5f, -0.5f, 255, 0, 255, 255}, {-0.5f,  0.5f, -0.5f, 255, 0, 255, 255},
	// Bottom (-Y) cyan
	{-0.5f, -0.5f, -0.5f, 0, 255, 255, 255}, { 0.5f, -0.5f, -0.5f, 0, 255, 255, 255},
	{ 0.5f, -0.5f,  0.5f, 0, 255, 255, 255}, {-0.5f, -0.5f,  0.5f, 0, 255, 255, 255},
};

// This is the actual point of this file existing: DsiEarlyVideo.cpp only
// proves the video/GL setup itself; this proves platform/RenderAPI_DSI.cpp
// -- the ~900-line GL-to-libnds translation layer the real Minecraft renderer
// will call into -- actually turns a Minecraft-shaped RenderInterleavedMesh
// into a correct, transformed, coloured shape on screen. Orthographic instead
// of a real perspective frustum: it needs no FOV/aspect math this port has no
// way to check yet (see the "APPROXIMATED"/"ASSUMED" notes in
// RenderAPI_DSI.cpp), and a rotating cube still fully exercises the matrix
// stack, per-vertex colour and depth test either way.
void runCubeDemo()
{
	dsiEnsureEarlyVideo();

	RenderInterleavedMesh mesh;
	mesh.data = kCubeVertices;
	mesh.stride = sizeof(CubeVertex);
	mesh.count = 24;
	mesh.primitive = RenderPrimitive::Quads;
	mesh.hasColor = true;
	mesh.colorOffset = offsetof(CubeVertex, r);

	renderMatrixMode(RenderMatrixMode::Projection);
	renderLoadIdentity();
	renderOrtho(-2.0, 2.0, -1.5, 1.5, 0.1, 10.0);
	renderEnable(RenderCapability::DepthTest);

	int angle = 0;
	while (true)
	{
		renderMatrixMode(RenderMatrixMode::ModelView);
		renderLoadIdentity();
		renderTranslate(0.0f, 0.0f, -3.0f);
		renderRotate((float)angle, 0.4f, 1.0f, 0.2f);
		angle = (angle + 2) % 360;

		renderDrawInterleaved(mesh);
		glFlush(0); // waits for vblank and swaps

		scanKeys();
		if (keysDown() & KEY_START)
			break;
	}
}

// One textured vertex: position + UV, no per-vertex colour (drawn white so
// the texture shows its real colours -- see runTerrainDemo()). Same
// no-padding reasoning as CubeVertex: 3 floats + 2 floats is a clean 20-byte
// stride.
struct TexVertex
{
	float x, y, z;
	float u, v;
};

// A flat 16x16 checkerboard, 4x4-pixel tiles, greenish/brownish -- there is no
// real Minecraft texture asset pipeline for DSi yet (see Resources_DSI.cpp:
// it looks for files under sd:/OptiCraft/data/assets, which this bring-up
// build never stages), so this is generated in code purely to have *some*
// non-solid-colour image to push through renderTextureImageRgba(). The cube
// demo above never called that function at all: it only ever set per-vertex
// colour, so the RGBA8->DS-native texture conversion in RenderAPI_DSI.cpp
// (and the wrap/tiling texture parameters) are otherwise completely untested
// until this runs.
void buildCheckerTexture(std::uint8_t* rgba)
{
	constexpr int kSize = 16;
	constexpr int kTile = 4;
	for (int y = 0; y < kSize; ++y)
	{
		for (int x = 0; x < kSize; ++x)
		{
			const bool green = ((x / kTile) + (y / kTile)) % 2 == 0;
			std::uint8_t* pixel = rgba + (std::size_t)(y * kSize + x) * 4;
			pixel[0] = green ? 60  : 120; // R
			pixel[1] = green ? 140 : 85;  // G
			pixel[2] = green ? 60  : 45;  // B
			pixel[3] = 255;
		}
	}
}

// A patch of "terrain": one large tiled floor quad plus three textured cubes
// standing on it at different positions, all sharing the single checker
// texture above -- as close as this bring-up tool gets to what
// Tessellator/RenderGlobal will actually ask RenderAPI_DSI.cpp to do (many
// textured quads, some depth-sorted against each other) without pulling in
// any real Minecraft world/chunk code. Camera is tilted down onto the floor
// with a plain X-axis renderRotate(), still ortho for the reasons in
// runCubeDemo()'s comment.
void runTerrainDemo()
{
	dsiEnsureEarlyVideo();

	std::uint8_t texturePixels[16 * 16 * 4];
	buildCheckerTexture(texturePixels);

	int texture = 0;
	renderGenerateTextures(1, &texture);
	renderTextureBeginUpload(texture, 16, 16, 0, /*blur=*/false, /*clamp=*/false);
	renderTextureImageRgba(0, 16, 16, texturePixels);

	// The floor: one quad from (-2,-0.5,-2) to (2,-0.5,2), UVs 0..4 so
	// GL_TEXTURE_WRAP_S/T (set by renderTextureBeginUpload's clamp=false
	// above) tiles the 16x16 checker 4x4 times across it.
	const TexVertex floorVertices[4] = {
		{-2.0f, -0.5f, -2.0f, 0.0f, 0.0f},
		{ 2.0f, -0.5f, -2.0f, 4.0f, 0.0f},
		{ 2.0f, -0.5f,  2.0f, 4.0f, 4.0f},
		{-2.0f, -0.5f,  2.0f, 0.0f, 4.0f},
	};

	// One textured unit cube (all six faces reuse the same 0..1 UV square;
	// there is no atlas to pick a different sub-rectangle per face yet).
	const TexVertex cubeVertices[24] = {
		{-0.5f, -0.5f,  0.5f, 0, 0}, { 0.5f, -0.5f,  0.5f, 1, 0}, { 0.5f,  0.5f,  0.5f, 1, 1}, {-0.5f,  0.5f,  0.5f, 0, 1},
		{ 0.5f, -0.5f, -0.5f, 0, 0}, {-0.5f, -0.5f, -0.5f, 1, 0}, {-0.5f,  0.5f, -0.5f, 1, 1}, { 0.5f,  0.5f, -0.5f, 0, 1},
		{ 0.5f, -0.5f,  0.5f, 0, 0}, { 0.5f, -0.5f, -0.5f, 1, 0}, { 0.5f,  0.5f, -0.5f, 1, 1}, { 0.5f,  0.5f,  0.5f, 0, 1},
		{-0.5f, -0.5f, -0.5f, 0, 0}, {-0.5f, -0.5f,  0.5f, 1, 0}, {-0.5f,  0.5f,  0.5f, 1, 1}, {-0.5f,  0.5f, -0.5f, 0, 1},
		{-0.5f,  0.5f,  0.5f, 0, 0}, { 0.5f,  0.5f,  0.5f, 1, 0}, { 0.5f,  0.5f, -0.5f, 1, 1}, {-0.5f,  0.5f, -0.5f, 0, 1},
		{-0.5f, -0.5f, -0.5f, 0, 0}, { 0.5f, -0.5f, -0.5f, 1, 0}, { 0.5f, -0.5f,  0.5f, 1, 1}, {-0.5f, -0.5f,  0.5f, 0, 1},
	};
	// Three "blocks" at different floor positions, so overlapping/occluding
	// geometry actually gets depth-tested against both the floor and each
	// other, not just against itself like the single cube demo.
	const float cubePositions[3][3] = {
		{-1.0f, 0.0f, -0.5f},
		{ 0.3f, 0.0f,  0.5f},
		{ 1.2f, 0.5f, -1.0f}, // taller stack: y=0.5 sits one cube-height above the first two
	};

	RenderInterleavedMesh floorMesh;
	floorMesh.data = floorVertices;
	floorMesh.stride = sizeof(TexVertex);
	floorMesh.count = 4;
	floorMesh.primitive = RenderPrimitive::Quads;
	floorMesh.hasTexture = true;
	floorMesh.texCoordOffset = offsetof(TexVertex, u);

	RenderInterleavedMesh cubeMesh;
	cubeMesh.data = cubeVertices;
	cubeMesh.stride = sizeof(TexVertex);
	cubeMesh.count = 24;
	cubeMesh.primitive = RenderPrimitive::Quads;
	cubeMesh.hasTexture = true;
	cubeMesh.texCoordOffset = offsetof(TexVertex, u);

	renderMatrixMode(RenderMatrixMode::Projection);
	renderLoadIdentity();
	renderOrtho(-3.0, 3.0, -2.25, 2.25, 0.1, 20.0);
	renderEnable(RenderCapability::DepthTest);
	renderEnable(RenderCapability::Texture2D);
	renderBindTexture(texture);
	renderColor3f(1.0f, 1.0f, 1.0f); // white: let the texture show its own colour

	int angle = 0;
	while (true)
	{
		renderMatrixMode(RenderMatrixMode::ModelView);
		renderLoadIdentity();
		renderTranslate(0.0f, 0.0f, -4.0f);
		renderRotate(35.0f, 1.0f, 0.0f, 0.0f); // tilt down onto the floor
		renderRotate((float)angle, 0.0f, 1.0f, 0.0f); // slow orbit around it
		angle = (angle + 1) % 360;

		renderDrawInterleaved(floorMesh);
		for (const float (&pos)[3] : cubePositions)
		{
			renderPushMatrix();
			renderTranslate(pos[0], pos[1], pos[2]);
			renderDrawInterleaved(cubeMesh);
			renderPopMatrix();
		}

		glFlush(0); // waits for vblank and swaps

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

	std::printf("\nA: top=3D/bottom=black colour-cycle demo\n");
	std::printf("X: spinning cube via RenderAPI_DSI (the real renderer)\n");
	std::printf("Y: textured floor + blocks (tests texture upload, untested by X)\n");
	std::printf("START: exit\n");

	int choice = 0; // 0 = exit, 1 = colour cycle, 2 = cube, 3 = textured terrain
	while (true)
	{
		swiWaitForVBlank();
		scanKeys();
		const int down = keysDown();
		if (down & KEY_START)
			break;
		if (down & KEY_A)
		{
			choice = 1;
			break;
		}
		if (down & KEY_X)
		{
			choice = 2;
			break;
		}
		if (down & KEY_Y)
		{
			choice = 3;
			break;
		}
	}

	if (choice == 1)
		runVideoDemo(); // returns on START
	else if (choice == 2)
		runCubeDemo(); // returns on START
	else if (choice == 3)
		runTerrainDemo(); // returns on START

	return 0;
}
