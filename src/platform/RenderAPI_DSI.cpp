#ifdef DSI_PLATFORM

#include "platform/RenderAPI.h"

#include <nds.h>
#include <cstring>
#include <vector>

// -----------------------------------------------------------------------------
// DSi RenderAPI backend -- maps the shared GL-shaped platform/RenderAPI.h
// surface onto libnds's videoGL wrapper around the DS 3D engine.
// -----------------------------------------------------------------------------
// Confidence varies a lot across this file, and the comment on each section
// says which of these it is:
//
//   VERIFIED    -- matches a libnds function/macro read directly from
//                   blocksds/libnds source (nds/arm9/videoGL.h, nds/arm9/
//                   video.h). High confidence, but still UNTESTED: nothing in
//                   this port has run on real hardware/an emulator yet (see
//                   DsiEarlyMemory.cpp/DsiBringup.cpp for why -- this sandbox
//                   cannot reach the BlocksDS toolchain servers, only GitHub
//                   Actions can, and this file hasn't been through that build
//                   yet at the time it was written).
//   APPROXIMATED -- the DS 3D engine genuinely cannot do what desktop
//                   OpenGL/GX/GS do here (no generalised glBlendFunc, no
//                   multitexturing, a table-based fog model instead of a
//                   parametric one, a very different lighting model), so this
//                   is a deliberate, commented substitute, not an oversight.
//   ASSUMED      -- a specific behavior (like the lightmap's UV-to-index
//                   convention) inferred from how the shared engine calls
//                   this API rather than confirmed against its source line by
//                   line. Flagged so it's the first thing to check against a
//                   real screenshot once this can run.
//
// Scope cut relative to RenderAPI.h: the display-list/occlusion-query group
// (PLATFORM_PC only), the persistent-mesh group (PLATFORM_PERSISTENT_RENDER_
// MESH || PLATFORM_MODEL_PERSISTENT_MESH, both 0 for DSi -- see PlatformConfig.h),
// the native-terrain-pipeline function (PLATFORM_NATIVE_TERRAIN_PIPELINE,
// Wii-only) and renderApplyTextureQuality/renderReadPixelsRgb (both gated off
// for DSi too) are all compiled out by RenderAPI.h's own #if guards, so none
// of them are implemented here -- DSi uses the same captured/immediate mesh
// replay path PS2 uses instead of native persistent geometry.

namespace
{

// -----------------------------------------------------------------------------
// Poly format shadow state -- VERIFIED mechanism, APPROXIMATED mapping
// -----------------------------------------------------------------------------
// Unlike desktop GL/GX, the DS 3D engine has no per-feature enable toggles for
// culling, lighting or shading: they are all bits of one word set together by
// glPolyFmt(). Every setter below that touches one of these fields just
// updates the shadow and marks it dirty; applyPolyFormat() re-issues glPolyFmt
// lazily, right before the next draw call, so a burst of state changes costs
// one glPolyFmt() instead of one per call.
struct PolyFormatState
{
	RenderFace cullFace = RenderFace::Back;
	bool cullEnabled = false; // DS default: nothing is culled.
	bool light0 = false;
	bool light1 = false;
	RenderShadeModel shade = RenderShadeModel::Smooth;
	std::uint8_t alpha31 = 31; // POLY_ALPHA is 5 bits (0-31); 31 = opaque.
	bool fogEnabled = false;
	// APPROXIMATED: DS opaque polygons always write depth; there is no
	// general glDepthMask() equivalent. POLY_TRANS_SET_DEPTH only affects
	// TRANSLUCENT polygons (POLY_ALPHA < 31) writing depth, which is the
	// closest available control and matches Minecraft's actual use of
	// depth-mask-off (translucent water/glass/GUI overlays).
	bool depthMaskEnabled = true;
	bool dirty = true;
};

PolyFormatState g_poly;

void applyPolyFormatIfDirty()
{
	if (!g_poly.dirty)
		return;
	g_poly.dirty = false;

	u32 bits = POLY_ALPHA(g_poly.alpha31);
	if (g_poly.shade == RenderShadeModel::Flat)
		bits |= POLY_DECAL; // Closest DS shading mode to "no interpolation".
	if (g_poly.light0)
		bits |= POLY_FORMAT_LIGHT0;
	if (g_poly.light1)
		bits |= POLY_FORMAT_LIGHT1;
	if (g_poly.fogEnabled)
		bits |= POLY_FOG;

	if (g_poly.cullEnabled)
		bits |= (g_poly.cullFace == RenderFace::Front) ? POLY_CULL_FRONT : POLY_CULL_BACK;
	else
		bits |= POLY_CULL_NONE;

	if (g_poly.depthMaskEnabled)
		bits |= POLY_TRANS_SET_DEPTH;

	glPolyFmt(bits);
}

void markPolyDirty() { g_poly.dirty = true; }

// -----------------------------------------------------------------------------
// Texture registry -- VERIFIED upload mechanism (glTexImageNtr2D/GL_RGBA),
// APPROXIMATED sub-image support (see renderTextureSubImageRgba below).
// -----------------------------------------------------------------------------
// glTexImageNtr2D() always replaces a texture's data wholesale; there is no
// partial/sub-rectangle upload on this hardware. Minecraft's atlas
// stitching and animated/procedural textures (lava, fire, the colormap-tinted
// leaves/water icons, ItemRenderer's dynamic icons) rely on
// renderTextureSubImageRgba() patching a region of an already-uploaded
// texture, so each texture keeps its own RGBA8 CPU-side copy here and a
// sub-image write patches that copy and re-uploads the whole thing. This
// costs real RAM (width*height*4 bytes per texture, held for the life of the
// texture) -- worth watching in DsiEarlyMemory.cpp's committed-heap number
// once real textures are loading.
struct DsiTexture
{
	int width = 0;
	int height = 0;
	bool blur = false;
	bool clamp = false;
	bool allocated = false;
	bool paletted = false; // Set by uploadTexture(): which VRAM format `allocated` actually used.
	std::vector<std::uint8_t> rgba; // width*height*4, tightly packed RGBA8
};

// How many bytes of the four 128 KB texture-image banks (DsiEarlyVideo.cpp)
// this slot's current upload actually holds: 0 if nothing is allocated, else
// 1 byte/pixel for the GL_RGB256 paletted path or 2 for GL_RGBA. Exists so a
// texture-upload failure (RenderEngine.cpp's DSi-only warning) can report how
// much of the 512 KB budget was already spoken for at that moment instead of
// just the one texture's own size -- real hardware already showed a valid
// power-of-two texture (gui/items.png, 256x256) failing this way, which only
// happens when the banks are full, and guessing what filled them wastes a
// round-trip to real hardware that a number in the log line does not.
std::size_t textureVramBytes(const DsiTexture& tex)
{
	if (!tex.allocated)
		return 0;
	return static_cast<std::size_t>(tex.width) * static_cast<std::size_t>(tex.height) * (tex.paletted ? 1u : 2u);
}

std::vector<DsiTexture> g_textures; // index 0 unused (0 means "no texture" in GL)
int g_boundTexture = 0;

DsiTexture* textureSlot(int name)
{
	if (name <= 0)
		return nullptr;
	if (static_cast<std::size_t>(name) >= g_textures.size())
		g_textures.resize(name + 1);
	return &g_textures[name];
}

// RGBA8 -> DS GL_RGBA (15-bit direct colour, 1-bit alpha). Bit layout VERIFIED
// against nds/arm9/video.h's ARGB16() macro: bit15=alpha, bits0-4=R, 5-9=G,
// 10-14=B. The alpha channel only has one bit on this hardware, so anything at
// or above the halfway point becomes fully opaque and anything below becomes
// fully transparent -- there is no partial-coverage alpha in this texture
// format (translucency for a whole polygon is a separate mechanism, see
// renderColor4f/POLY_ALPHA below).
void convertRgba8ToDs(const std::uint8_t* src, std::uint16_t* dst, int pixelCount)
{
	for (int i = 0; i < pixelCount; ++i)
	{
		const std::uint8_t r = src[i * 4 + 0];
		const std::uint8_t g = src[i * 4 + 1];
		const std::uint8_t b = src[i * 4 + 2];
		const std::uint8_t a = src[i * 4 + 3];
		dst[i] = static_cast<std::uint16_t>(
			((a >= 128) ? 0x8000 : 0) |
			(r >> 3) |
			((g >> 3) << 5) |
			((b >> 3) << 10));
	}
}

// Paletted (GL_RGB256) upload -- HALF the VRAM cost of GL_RGBA (1 byte per
// pixel instead of 2), tried before it below. Real-hardware data this
// session traced the remaining main-menu lag and the complete loss of 3D
// world rendering to the same root cause: the DS 3D engine's texture image
// VRAM is a hard 512 KB ceiling (DsiEarlyVideo.cpp's four 128 KB banks,
// already the hardware maximum for this data), and terrain.png/gui/items.png/
// gui/icons.png -- all confirmed exactly 256x256, a valid power-of-two size,
// so the earlier power-of-two fixes do not apply here -- are 128 KB each in
// GL_RGBA. Those three alone are 384 KB, and once font/gui.png and whatever
// else a real scene needs are also resident, gameplay's own essential
// textures were failing to fit at all, silently falling back to the same
// "never cached, retried from scratch on every bind" path already fixed for
// the menu's decorative textures -- except every failed bind here is a real
// SD decode+upload attempt in the middle of drawing a frame, not once at
// menu idle.
//
// GL_RGB256 stores one 8-bit palette index per pixel instead of a 16-bit
// colour: half the memory for any texture with 255 or fewer distinct opaque
// colours, which describes most block/item/icon pixel art (a handful of
// shades per material) even if it does not describe a photo-sourced
// panorama. Index 0 is reserved for GL_TEXTURE_COLOR0_TRANSPARENT ("this
// index is fully transparent"), the same 1-bit alpha convertRgba8ToDs()
// already uses for GL_RGBA -- no loss of transparency capability, just a
// different place the one alpha bit lives. If the source image has more
// than 255 distinct opaque colours (fails partway through the scan below),
// this returns false and the caller falls through to the existing GL_RGBA
// path unchanged -- trying this first can only help, never break a texture
// that does not fit it.
//
// O(pixel count) time: one pass building a 32768-entry (all possible RGB555
// values) direct-lookup table of "which palette index is this colour",
// rather than a linear search of the growing palette per pixel. Runs once
// per texture load (and once per animated sub-image update, same as
// convertRgba8ToDs() already does for every upload today -- not a new cost
// pattern, the same one this file already accepts), not per frame.
bool tryUploadPaletted(int name, const DsiTexture& tex, int param)
{
	const std::size_t pixelCount = static_cast<std::size_t>(tex.width) * tex.height;

	std::vector<std::int16_t> colorToIndex(32768, -1);
	std::vector<std::uint16_t> palette;
	palette.reserve(256);
	palette.push_back(0); // Index 0's actual colour is irrelevant -- GL_TEXTURE_COLOR0_TRANSPARENT hides it.
	std::vector<std::uint8_t> indices(pixelCount);

	const std::uint8_t* src = tex.rgba.data();
	for (std::size_t i = 0; i < pixelCount; ++i)
	{
		const std::uint8_t a = src[i * 4 + 3];
		if (a < 128)
		{
			indices[i] = 0;
			continue;
		}

		const std::uint8_t r = src[i * 4 + 0];
		const std::uint8_t g = src[i * 4 + 1];
		const std::uint8_t b = src[i * 4 + 2];
		const std::uint16_t color15 = static_cast<std::uint16_t>(
			(r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));

		std::int16_t index = colorToIndex[color15];
		if (index < 0)
		{
			if (palette.size() >= 256)
				return false; // More than 255 distinct opaque colours: doesn't fit this format.
			index = static_cast<std::int16_t>(palette.size());
			colorToIndex[color15] = index;
			palette.push_back(color15);
		}
		indices[i] = static_cast<std::uint8_t>(index);
	}

	glBindTexture(0, name);
	const int uploaded = glTexImage2D(0, 0, GL_RGB256, tex.width, tex.height, 0,
		param | GL_TEXTURE_COLOR0_TRANSPARENT, indices.data());
	if (!uploaded)
		return false;

	glColorTableNtr(palette.size(), palette.data());
	return true;
}

// Returns whether the DS GPU actually accepted this texture. glTexImage2D()
// (really glTexImageNtr2D(), see nds/arm9/videoGL.h) requires each dimension
// to be an EXACT power of two from 8 to 1024 and returns 0 -- uploading
// nothing -- for anything else (also on VRAM exhaustion). That return value
// used to be discarded here, so a real-hardware asset with, say, a
// non-power-of-two panorama.png silently uploaded nothing while every CPU-
// side book-keeping struct (DsiTexture::allocated, renderTextureIsValid())
// still reported success: the polygon still drew, just textureless, at
// whatever flat vertex colour the caller set -- solid white for
// LegacyPanorama.cpp's renderColor4f(1,1,1,1). Checking it here lets
// renderTextureImageRgba() report the failure truthfully, so RenderEngine.cpp's
// existing missing-texture fallback (the black/white checkerboard,
// createMissingTexture()) actually engages the way it already does for a
// texture that fails to *decode* -- a recognisable placeholder instead of an
// invisible, silent failure.
bool uploadTexture(int name, DsiTexture& tex)
{
	if (tex.width <= 0 || tex.height <= 0 || tex.rgba.empty())
		return false;

	int param = 0;
	if (!tex.clamp)
		param |= GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T;

	if (tryUploadPaletted(name, tex, param))
	{
		glTexParameter(0, param); // wrap bits already set above; kept for parity with callers that only touch params later
		tex.paletted = true;
		return true;
	}

	std::vector<std::uint16_t> converted(static_cast<std::size_t>(tex.width) * tex.height);
	convertRgba8ToDs(tex.rgba.data(), converted.data(), tex.width * tex.height);

	glBindTexture(0, name);
	const int uploaded = glTexImage2D(0, 0, GL_RGBA, tex.width, tex.height, 0, param, converted.data());
	glTexParameter(0, param); // wrap bits already set above; kept for parity with callers that only touch params later
	tex.paletted = false;
	return uploaded != 0;
}

// -----------------------------------------------------------------------------
// Lightmap-as-vertex-colour -- ASSUMED UV convention.
// -----------------------------------------------------------------------------
// The DS 3D engine has exactly one texture unit -- no multitexture combiner
// stage -- unlike the Wii's GX (see RenderAPI_GX_WII.cpp, which uses real
// hardware multitexturing for this same lightmap). PS2's GS does have one but
// this backend takes the other approach PS2 could have taken too: bake the
// lightmap into the per-vertex colour instead. Minecraft's standard shading
// mode is POLY_MODULATION (texture x vertex colour, the libnds default), so
// setting the vertex colour to the looked-up lightmap colour right before each
// vertex achieves the same on-screen result as sampling a second texture and
// multiplying, at the cost of losing hardware bilinear filtering across
// lightmap texel boundaries (a soft lighting gradient becomes bands instead) --
// a real, visible quality loss worth revisiting once this can be seen running.
//
// OpenGlHelper::setLightmapTextureCoords() forwards normalized OpenGL texture
// coordinates (u, v in [0, 1]) exactly as the caller computed them for a real
// GL texture sampler; renderSetLightmapColors() is handed that same texture's
// full pixel data (count entries). This assumes the standard row-major square
// layout a 2D texture lookup implies -- index = round(v * (side-1)) * side +
// round(u * (side-1)), side = sqrt(count) -- rather than tracing the exact
// lightmap texture's dimensions through Minecraft.cpp/EntityRenderer.cpp,
// which is the first thing to check if lighting looks wrong (banded/blocky is
// expected and is the filtering loss above; discontinuous/scrambled would mean
// this indexing assumption is wrong).
std::vector<std::uint32_t> g_lightmapColors; // RGBA8, one std::uint32_t per texel
int g_lightmapUnit = -1; // OpenGL's GL_TEXTURE1_ARB-style enum for the lightmap unit, once seen

void applyLightmapColorAt(float u, float v)
{
	if (g_lightmapColors.empty())
		return;

	const int side = static_cast<int>(g_lightmapColors.size());
	// Only exact square counts are handled; anything else falls back to doing
	// nothing rather than an out-of-bounds guess.
	int sqrtSide = 0;
	while ((sqrtSide + 1) * (sqrtSide + 1) <= side)
		++sqrtSide;
	if (sqrtSide * sqrtSide != side || sqrtSide <= 0)
		return;

	auto clampIndex = [](float value, int maxIndex) -> int
	{
		if (value < 0.0f) value = 0.0f;
		if (value > 1.0f) value = 1.0f;
		const int result = static_cast<int>(value * (maxIndex) + 0.5f);
		return result < 0 ? 0 : (result > maxIndex ? maxIndex : result);
	};

	const int col = clampIndex(u, sqrtSide - 1);
	const int row = clampIndex(v, sqrtSide - 1);
	const std::uint32_t packed = g_lightmapColors[static_cast<std::size_t>(row) * sqrtSide + col];

	const float r = ((packed >> 0) & 0xFF) / 255.0f;
	const float g = ((packed >> 8) & 0xFF) / 255.0f;
	const float b = ((packed >> 16) & 0xFF) / 255.0f;
	glColor3f(r, g, b);
}

// -----------------------------------------------------------------------------
// Fog -- APPROXIMATED. The DS fog is a 32-entry density lookup table indexed
// by (depth >> shift) - offset, not GL's parametric density/start/end/mode.
// Rebuilding the table from GL-style parameters on every renderFogf() call
// would be the faithful approach; for now this only forwards the colour
// (renderFogColor -> glFogColor) and leaves the table at libnds's default
// (set once in dsiEnsureEarlyVideo()-equivalent init below), which is a
// reasonable fixed falloff but does not track Minecraft's actual fog
// start/end distance options yet. Flagged rather than silently wrong: fog
// will render as *some* distance fog, just not necessarily at the requested
// distance.
float g_fogDensity = 1.0f;
float g_fogStart = 0.0f;
float g_fogEnd = 1.0f;

} // namespace

// Sum of textureVramBytes() across every currently-allocated texture: how much
// of the 512 KB texture-image budget (DsiEarlyVideo.cpp's four banks) is
// actually spoken for right now. See dsi/DsiEarlyInit.h's declaration for who
// calls this and why.
std::size_t dsiTotalTextureVramBytes()
{
	std::size_t total = 0;
	for (const DsiTexture& tex : g_textures)
		total += textureVramBytes(tex);
	return total;
}

// -----------------------------------------------------------------------------
// Static / captured mesh replay -- VERIFIED per-call mapping (glBegin/
// glVertex3f/glTexCoord2f/glColor3b), APPROXIMATED overall performance: this
// re-issues one GL call per vertex per frame instead of compiling to a native
// display list, matching PS2's non-native-terrain-pipeline path (see
// PLATFORM_NATIVE_TERRAIN_PIPELINE in PlatformConfig.h, left off for DSi).
// -----------------------------------------------------------------------------
namespace
{

// A Minecraft interleaved vertex is always float3 position, float2 texcoord,
// RGBA8 colour, signed-byte3 normal, in whatever subset the mesh's hasTexture/
// hasColor/hasNormals/hasBrightness flags declare -- see RenderInterleavedMesh
// in RenderAPI.h. renderDrawInterleaved()'s job is to walk that buffer and
// issue one glVertex3f() (plus whatever glTexCoord2f()/glColor3b()/glNormal3f()
// precede it) per vertex.
bool drawInterleavedMesh(const RenderInterleavedMesh& mesh)
{
	if (!mesh.data || mesh.count <= 0 || mesh.stride <= 0)
		return false;

	const std::uint8_t* base = static_cast<const std::uint8_t*>(mesh.data) + (std::size_t)mesh.first * mesh.stride;

	// The DS GPU has one alpha value per polygon batch (POLY_ALPHA, see
	// renderColor4f above), not per vertex -- there is no generalised
	// per-fragment blend the way a per-vertex alpha byte would imply. A
	// translucent Tessellator draw (GuiMainMenu's drawGradientRect vignette,
	// drawPanorama's sample cross-fade) carries its alpha in mesh.hasColor's
	// rgba[3], which the per-vertex glColor3b() call below drops on the
	// floor -- glColor3b has no alpha parameter, and nothing else in this
	// function ever reads rgba[3]. Net effect on real hardware: every such
	// draw came out fully opaque regardless of what alpha the caller asked
	// for. Average every vertex's alpha and latch that for the whole batch
	// before applyPolyFormatIfDirty() below picks it up: an approximation
	// for an actual gradient (the fade across the quad is lost, every vertex
	// renders at one flat alpha), but it turns "always opaque" into
	// "actually translucent", which is what every caller here expects and
	// previously never got. Averaging rather than sampling one vertex
	// matters concretely for drawGradientRect: its two calls for the menu
	// vignette are top-transparent/bottom-opaque and top-opaque/bottom-
	// transparent, so reading only the first vertex would render one of
	// them at alpha 0 -- effectively deleting it -- instead of the
	// in-between value the average gives both.
	//
	// Sampling first+last (not a full per-vertex scan) is enough: every
	// caller of this path is either uniform alpha across the whole mesh
	// (glyph batches -- a full menu screen's worth of text lands in one
	// draw call here, easily hundreds of vertices, all sharing FontRenderer's
	// one currentA -- and most other UI/terrain draws), where first == last
	// gives the exact value for free, or a straight linear gradient like
	// drawGradientRect's two quads, where the two endpoints alone already
	// average to the exact in-between value a full scan would compute (the
	// two middle vertices share one or the other endpoint's alpha, so they
	// add nothing a full scan wouldn't already cancel out). A full O(n) scan
	// here previously ran on every hundred-plus-vertex text batch every
	// frame on a CPU with no hardware float unit (arm946e-s+nofp) -- real,
	// measurable overhead for zero difference in the result for anything
	// this engine actually draws.
	if (mesh.hasColor)
	{
		std::uint8_t alphaFirst;
		std::memcpy(&alphaFirst, base + mesh.colorOffset + 3, sizeof(alphaFirst));
		std::uint8_t alphaLast = alphaFirst;
		if (mesh.count > 1)
			std::memcpy(&alphaLast, base + (std::size_t)(mesh.count - 1) * mesh.stride + mesh.colorOffset + 3, sizeof(alphaLast));
		const unsigned int alphaSum = static_cast<unsigned int>(alphaFirst) + static_cast<unsigned int>(alphaLast);
		const float averageAlpha = static_cast<float>(alphaSum) / (255.0f * 2.0f);
		g_poly.alpha31 = static_cast<std::uint8_t>(averageAlpha * 31.0f + 0.5f);
		markPolyDirty();
	}

	applyPolyFormatIfDirty();

	GL_GLBEGIN_ENUM glPrimitive = GL_TRIANGLES;
	switch (mesh.primitive)
	{
		case RenderPrimitive::Triangles:
		case RenderPrimitive::TriangleStrip: // No native strip primitive is used
		case RenderPrimitive::TriangleFan:   // here; each vertex still gets emitted,
			glPrimitive = GL_TRIANGLES;       // just not connected as a strip/fan --
			break;                            // acceptable for Quads/Triangles, the
		case RenderPrimitive::Quads:          // only two primitives Minecraft's
			glPrimitive = GL_QUADS;           // Tessellator actually emits.
			break;
		default:
			return false;
	}

	// libnds's glVertex3f() converts straight to the DS vertex hardware's
	// native format: v16, a 16-bit signed 4.12 fixed-point value (see
	// floattov16() in nds/arm9/videoGL.h) that only represents roughly -8.0
	// to +7.9998 -- and the conversion happens on the raw float passed in,
	// BEFORE any modelview/projection matrix multiply (those run in much
	// wider 20.12 fixed point, in hardware, after this). Every caller of
	// this function submits vertices far outside +-8: GuiMainMenu's 2D
	// draws use screen-pixel coordinates directly (0 to 256/192, the logo
	// geometry out to 265), and nothing in the shared Tessellator/GuiScreen
	// code has ever had to think about this, because no other backend
	// (PC/PS2/Wii) has anything like it. Passed through unscaled, a
	// coordinate like this either silently wraps -- 256.0 and 192.0 are
	// both exact multiples of the v16 step (4096 units/px), so they wrap to
	// exactly 0.0 -- or lands somewhere else nonsensical. This is the real
	// hardware bug behind the DSi main menu photo: full-screen quads (the
	// darkening vignette) painting nothing, and the logo/splash text
	// collapsing into a garbled blob near the coordinate origin instead of
	// spanning the screen.
	//
	// Fix: scale every vertex down by kVertexScale before glVertex3f() sees
	// it, and push a matching glScalef(kVertexScale, ...) first, so the
	// GPU's own matrix multiply -- done in that wider fixed point, not v16
	// -- puts the geometry back where the caller meant it. kVertexScale is
	// 8x the screen's own longer dimension (256): comfortably covers every
	// 2D coordinate this engine draws (menus go out to a few hundred px at
	// most) and every chunk-local 3D vertex offset (well under 256/8 = 32
	// units) with a wide safety margin, while still leaving a v16 step of
	// 256/4096 = 1/16 unit -- far finer than this console's 256x192 screen
	// can show. Assumes GL_MODELVIEW is the active matrix mode, true for
	// every vertex-submitting draw in this engine (GL_PROJECTION is only
	// touched briefly for camera/ortho setup, never held across a
	// Tessellator draw).
	constexpr float kVertexScale = 256.0f;
	// Multiply by the reciprocal instead of dividing by kVertexScale below:
	// same result (kVertexScale is an exact power of two, so this reciprocal
	// is exact too, no precision lost), but division is one of the slower
	// operations a *hardware* FPU has, and this ARM9 (arm946e-s+nofp) has no
	// FPU at all -- every one of these was a soft-float library call, three
	// per vertex, every vertex, every draw call in the whole engine. Real
	// cost on a menu screen's text batch alone (a few hundred vertices).
	constexpr float kInvVertexScale = 1.0f / kVertexScale;
	glPushMatrix();
	glScalef(kVertexScale, kVertexScale, kVertexScale);

	glBegin(glPrimitive);
	for (int i = 0; i < mesh.count; ++i)
	{
		const std::uint8_t* vertex = base + (std::size_t)i * mesh.stride;

		if (mesh.hasBrightness)
		{
			// Brightness carries the lightmap (u, v) pair as two floats, the
			// same convention EntityRenderer/Tessellator use when there is no
			// real second texture unit to send it to -- see the lightmap
			// comment block above.
			float uv[2];
			std::memcpy(uv, vertex + mesh.brightnessOffset, sizeof(uv));
			applyLightmapColorAt(uv[0], uv[1]);
		}
		if (mesh.hasColor)
		{
			std::uint8_t rgba[4];
			std::memcpy(rgba, vertex + mesh.colorOffset, sizeof(rgba));
			glColor3b(rgba[0], rgba[1], rgba[2]);
		}
		if (mesh.hasNormals)
		{
			constexpr float kInvNormalScale = 1.0f / 127.0f;
			std::int8_t normal[3];
			std::memcpy(normal, vertex + mesh.normalOffset, sizeof(normal));
			glNormal3f(normal[0] * kInvNormalScale, normal[1] * kInvNormalScale, normal[2] * kInvNormalScale);
		}
		if (mesh.hasTexture)
		{
			float uv[2];
			std::memcpy(uv, vertex + mesh.texCoordOffset, sizeof(uv));
			glTexCoord2f(uv[0], uv[1]);
		}

		float position[3];
		std::memcpy(position, vertex, sizeof(position));
		if (mesh.positionShort)
		{
			// positionShort means the source buffer stores the position as a
			// compact GL_SHORT triple instead of 3 floats (see RenderAPI.h --
			// a PC vertex-buffer bandwidth optimisation, unrelated to the v16
			// hardware format discussed above). Nothing DSi-specific ever
			// creates such a mesh today (only the PC/PS2 backends set this
			// flag), so this is dead code on this backend; the memcpy above
			// always reads 3 floats. Left in so a future DSi caller that does
			// set it fails loudly (garbage values, not silently) instead of
			// this comment going stale.
		}
		glVertex3f(position[0] * kInvVertexScale, position[1] * kInvVertexScale, position[2] * kInvVertexScale);
	}
	glEnd();
	glPopMatrix(1);

	return true;
}

} // namespace

// renderStaticMeshCreate/Destroy/Compile/Draw are NOT defined here: they are
// the generic platform/RenderStaticMesh.cpp implementation (shared by every
// backend that doesn't set PLATFORM_PERSISTENT_RENDER_MESH -- DSi doesn't;
// see PlatformConfig.h). platform/RenderStaticMesh.cpp is compiled
// unconditionally by Makefile.game's SOURCEDIRS (it has no per-backend
// filename suffix, so the RenderAPI_(PC|GL|GX_WII|GS_PS2).cpp exclusion
// grep never touches it). Defining them again here previously duplicated
// those four symbols and would fail at link time with a multiple-definition
// error against RenderStaticMesh.cpp's copies -- removed rather than kept as
// a second, coincidentally-identical implementation.

bool renderDrawInterleaved(const RenderInterleavedMesh& mesh)
{
	return drawInterleavedMesh(mesh);
}

bool renderCaptureInterleaved(const RenderInterleavedMesh& mesh, RenderCapturedMesh& out, bool append)
{
	if (!mesh.data || mesh.count <= 0 || mesh.stride <= 0)
		return false;

	if (!append)
		out.clear();

	const std::size_t bytes = (std::size_t)mesh.count * mesh.stride;
	const std::size_t words = (bytes + sizeof(std::int32_t) - 1) / sizeof(std::int32_t);
	const std::size_t oldSize = out.raw.size();
	out.raw.resize(oldSize + words);
	std::memcpy(out.raw.data() + oldSize,
	            static_cast<const std::uint8_t*>(mesh.data) + (std::size_t)mesh.first * mesh.stride,
	            bytes);

	out.vertexCount += mesh.count;
	out.stride = mesh.stride;
	out.primitive = mesh.primitive;
	out.positionShort = mesh.positionShort;
	out.hasTexture = mesh.hasTexture;
	out.texCoordOffset = mesh.texCoordOffset;
	out.hasColor = mesh.hasColor;
	out.colorOffset = mesh.colorOffset;
	out.hasNormals = mesh.hasNormals;
	out.normalOffset = mesh.normalOffset;
	out.hasBrightness = mesh.hasBrightness;
	out.brightnessOffset = mesh.brightnessOffset;

	return true;
}

bool renderDrawCaptured(const RenderCapturedMesh& mesh)
{
	if (mesh.empty())
		return false;

	RenderInterleavedMesh view;
	view.data = mesh.raw.data();
	view.stride = mesh.stride;
	view.first = 0;
	view.count = mesh.vertexCount;
	view.primitive = mesh.primitive;
	view.positionShort = mesh.positionShort;
	view.hasTexture = mesh.hasTexture;
	view.texCoordOffset = mesh.texCoordOffset;
	view.hasColor = mesh.hasColor;
	view.colorOffset = mesh.colorOffset;
	view.hasNormals = mesh.hasNormals;
	view.normalOffset = mesh.normalOffset;
	view.hasBrightness = mesh.hasBrightness;
	view.brightnessOffset = mesh.brightnessOffset;
	return drawInterleavedMesh(view);
}

// -----------------------------------------------------------------------------
// State -- VERIFIED unless noted.
// -----------------------------------------------------------------------------

void renderEnable(RenderCapability capability)
{
	switch (capability)
	{
		case RenderCapability::Texture2D: glEnable(GL_TEXTURE_2D); break;
		case RenderCapability::AlphaTest: glEnable(GL_ALPHA_TEST); break;
		case RenderCapability::Blend:     glEnable(GL_BLEND); break;
		case RenderCapability::Fog:       glEnable(GL_FOG); g_poly.fogEnabled = true; markPolyDirty(); break;
		case RenderCapability::CullFace:  g_poly.cullEnabled = true; markPolyDirty(); break;
		case RenderCapability::Light0:    g_poly.light0 = true; markPolyDirty(); break;
		case RenderCapability::Light1:    g_poly.light1 = true; markPolyDirty(); break;
		// APPROXIMATED: DS has no ColorMaterial/DepthTest-disable/Normalize/
		// RescaleNormal/PolygonOffsetFill/Lighting-as-a-toggle equivalents.
		// Lighting itself is implied by Light0/Light1 being on; the rest are
		// safe no-ops on this hardware (DepthTest is always on for opaque
		// polygons -- see renderDepthMask for the closest available control).
		default: break;
	}
}

void renderDisable(RenderCapability capability)
{
	switch (capability)
	{
		case RenderCapability::Texture2D: glDisable(GL_TEXTURE_2D); break;
		case RenderCapability::AlphaTest: glDisable(GL_ALPHA_TEST); break;
		case RenderCapability::Blend:     glDisable(GL_BLEND); break;
		case RenderCapability::Fog:       glDisable(GL_FOG); g_poly.fogEnabled = false; markPolyDirty(); break;
		case RenderCapability::CullFace:  g_poly.cullEnabled = false; markPolyDirty(); break;
		case RenderCapability::Light0:    g_poly.light0 = false; markPolyDirty(); break;
		case RenderCapability::Light1:    g_poly.light1 = false; markPolyDirty(); break;
		default: break;
	}
}

// APPROXIMATED: the DS 3D engine has no generalised src/dst blend-factor
// pipeline, only "this polygon has alpha N" (POLY_ALPHA, applied through
// renderColor4f below) plus the GL_BLEND on/off toggle above. Minecraft only
// ever calls this with (SrcAlpha, OneMinusSrcAlpha) or (SrcAlpha,
// OneMinusSrcAlpha)-equivalent pairs for normal translucency, which is exactly
// what POLY_ALPHA already does, so the factors themselves are intentionally
// ignored rather than rejected.
void renderBlendFunc(RenderBlendFactor source, RenderBlendFactor destination)
{
	(void)source;
	(void)destination;
}

void renderDepthMask(bool enabled)
{
	g_poly.depthMaskEnabled = enabled;
	markPolyDirty();
}

// APPROXIMATED: DS depth comparison is fixed at less-or-equal in the standard
// pipeline; POLY_DEPTH_TEST_EQUAL (already available via glPolyFmt, not
// wired up here) is the only other option and does not match any
// RenderCompare value Minecraft actually requests, so this is a no-op.
void renderDepthFunc(RenderCompare)
{
}

void renderAlphaFunc(RenderCompare, float reference)
{
	// glAlphaFunc() takes a single 0-31 threshold and always compares as
	// "greater than"; RenderCompare's function is ignored because Minecraft
	// only ever requests alpha-greater-than-threshold (leaf/glass cutouts).
	int threshold = static_cast<int>(reference * 31.0f + 0.5f);
	if (threshold < 0) threshold = 0;
	if (threshold > 31) threshold = 31;
	glAlphaFunc(threshold);
}

void renderCullFace(RenderFace face)
{
	g_poly.cullFace = face; // FrontAndBack has no DS equivalent; treated as Back.
	markPolyDirty();
}

// APPROXIMATED: DS has no per-channel colour write mask; always writes all
// four. Minecraft only ever disables all four together (never a partial
// mask), so this degrades to "do nothing" rather than "wrong".
void renderColorMask(bool, bool, bool, bool)
{
}

void renderBindTexture(int texture)
{
	g_boundTexture = texture;
	if (texture > 0)
		glBindTexture(0, texture);
}

void renderSetActiveTextureUnit(int textureUnit)
{
	// DS has one texture unit; remembered only so renderSetMultiTextureCoord
	// can tell the lightmap unit apart from the block-texture unit.
	g_lightmapUnit = (textureUnit != 0x84C0) ? textureUnit : -1;
}

void renderSetClientActiveTextureUnit(int)
{
}

// See the "Lightmap-as-vertex-colour" block above.
void renderSetMultiTextureCoord(int textureUnit, float u, float v)
{
	if (textureUnit == g_lightmapUnit || g_lightmapUnit == -1)
		applyLightmapColorAt(u, v);
}

void renderSetLightmapColors(const std::uint32_t* colors, int count)
{
	if (!colors || count <= 0)
	{
		g_lightmapColors.clear();
		return;
	}
	g_lightmapColors.assign(colors, colors + count);
}

void renderColor4f(float r, float g, float b, float a)
{
	g_poly.alpha31 = static_cast<std::uint8_t>(a * 31.0f + 0.5f);
	markPolyDirty();
	glColor3f(r, g, b);
}

void renderColor3f(float r, float g, float b)
{
	glColor3f(r, g, b);
}

void renderNormal3f(float x, float y, float z)
{
	glNormal3f(x, y, z);
}

void renderGenerateTextures(int count, int* textures)
{
	glGenTextures(count, textures);
	for (int i = 0; i < count; ++i)
		if (DsiTexture* tex = textureSlot(textures[i]))
			*tex = DsiTexture{};
}

void renderDeleteTextures(int count, const int* textures)
{
	std::vector<int> mutableCopy(textures, textures + count);
	glDeleteTextures(count, mutableCopy.data());
	for (int i = 0; i < count; ++i)
		if (textures[i] > 0 && static_cast<std::size_t>(textures[i]) < g_textures.size())
			g_textures[textures[i]] = DsiTexture{};
}

void renderTextureSubImageRgba(int level, int x, int y, int width, int height, const void* pixels)
{
	// Mip levels other than 0 are not tracked in the CPU-side shadow copy yet
	// (see the struct comment); only the base level can be patched correctly.
	if (level != 0)
		return;

	DsiTexture* tex = textureSlot(g_boundTexture);
	if (!tex || !tex->allocated || tex->rgba.empty())
		return;

	for (int row = 0; row < height; ++row)
	{
		const std::uint8_t* srcRow = static_cast<const std::uint8_t*>(pixels) + (std::size_t)row * width * 4;
		std::uint8_t* dstRow = tex->rgba.data() + ((std::size_t)(y + row) * tex->width + x) * 4;
		std::memcpy(dstRow, srcRow, (std::size_t)width * 4);
	}
	// Same dimensions as the already-successful initial upload, so this can't
	// newly fail the power-of-two check -- but propagate honestly anyway
	// rather than assume, in case VRAM pressure is what fails it this time.
	tex->allocated = uploadTexture(g_boundTexture, *tex);
}

void renderTextureImageRgba(int level, int width, int height, const void* pixels)
{
	if (level != 0)
		return; // See the mip-level note above.

	DsiTexture* tex = textureSlot(g_boundTexture);
	if (!tex)
		return;

	tex->width = width;
	tex->height = height;
	tex->rgba.assign(static_cast<const std::uint8_t*>(pixels),
	                  static_cast<const std::uint8_t*>(pixels) + (std::size_t)width * height * 4);
	// See uploadTexture()'s own comment: a real hardware upload can fail
	// (most likely a non-power-of-two source image) where the old code here
	// always reported success. renderTextureIsValid() reads this flag, and
	// RenderEngine.cpp's caller treats "invalid" the same as "failed to
	// decode" -- binding the checkerboard placeholder instead of leaving a
	// textureless polygon at its flat vertex colour.
	tex->allocated = uploadTexture(g_boundTexture, *tex);
}

// Real-hardware symptom this fixes: once the panorama texture upload above
// started actually succeeding, the main menu collapsed to roughly one
// redraw every few seconds, with a white line visibly "climbing" the
// screen as each one happened. legacyDrawPanorama() calls this every
// single frame with the same fixed arguments (true, false, true) purely to
// (re-)apply wrap/clamp -- but this used to call the full uploadTexture()
// (CPU RGBA->DS pixel-format conversion over every pixel, then a real
// glTexImage2D VRAM DMA of the whole texture) on every one of those calls.
// While the panorama upload was still silently failing (before the fix
// above), that cost nothing: glTexImageNtr2D() rejects a non-power-of-two
// size in its first few instructions, before touching VRAM. Once it
// started succeeding, "every frame" became "one full texture reupload to
// VRAM every frame" -- for a background image, not a 2-frame animated
// tile -- which is exactly the kind of sustained VRAM bus traffic that
// would visibly race the display controller's own scanout of that same
// VRAM bank, matching the reported "line climbing the screen" artifact.
//
// Wrap mode is a texture FORMAT FLAG (GL_TEXTURE_WRAP_S/T, set via
// glTexParameter() below), not pixel data -- libnds's glTexParameter()
// (nds/arm9/video/videoGL.c) just ORs a few bits into the active texture's
// already-uploaded format register, no VRAM write at all. So re-applying
// wrap mode every frame never needed a reupload in the first place; it
// only reupload-ed because uploadTexture() bundled both operations
// together. "blur" is tracked in DsiTexture but never turned into a GPU
// parameter bit anywhere in this file (bilinear filtering isn't wired up
// on this backend yet), so there is nothing to reapply for it here either.
void renderTextureParameters(bool blur, bool, bool clamp)
{
	if (DsiTexture* tex = textureSlot(g_boundTexture))
	{
		tex->blur = blur;
		tex->clamp = clamp;
		if (tex->allocated)
		{
			int param = 0;
			if (!tex->clamp)
				param |= GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T;
			glBindTexture(0, g_boundTexture);
			glTexParameter(0, param);
		}
	}
}

int renderGetMaxAnisotropy() { return 1; } // No anisotropic filtering on this hardware.
int renderGetMaxSamples() { return 1; }    // No MSAA on this hardware.

bool renderTextureBeginUpload(int texture, int width, int height, int, bool blur, bool clamp, bool, bool)
{
	renderBindTexture(texture);
	DsiTexture* tex = textureSlot(texture);
	if (!tex)
		return false;
	tex->width = width;
	tex->height = height;
	tex->blur = blur;
	tex->clamp = clamp;
	return true;
}

bool renderTextureIsValid(int texture)
{
	DsiTexture* tex = textureSlot(texture);
	return tex && tex->allocated;
}

void renderResetResources()
{
	glResetTextures();
	g_textures.clear();
	g_boundTexture = 0;
}

// APPROXIMATED -- see the fog comment block above; only colour is forwarded
// for now, not the density table.
void renderFogf(RenderFogParameter parameter, float value)
{
	switch (parameter)
	{
		case RenderFogParameter::Density: g_fogDensity = value; break;
		case RenderFogParameter::Start:   g_fogStart = value; break;
		case RenderFogParameter::End:     g_fogEnd = value; break;
		default: break;
	}
}

void renderFogi(RenderFogParameter, RenderFogMode)
{
	// DS fog has no exp/exp2/linear mode switch -- it is whatever shape the
	// density table encodes. See the fog comment block above.
}

void renderFogColor(const float* values)
{
	const auto toChannel = [](float v) -> std::uint8_t
	{
		if (v < 0.0f) v = 0.0f;
		if (v > 1.0f) v = 1.0f;
		return static_cast<std::uint8_t>(v * 31.0f + 0.5f);
	};
	glFogColor(toChannel(values[0]), toChannel(values[1]), toChannel(values[2]), 31);
}

// APPROXIMATED: DS lighting is direction-only (no positional/attenuated
// lights) and takes one packed colour per light, not separate ambient/
// diffuse/specular terms -- see the glLight() doc in videoGL.h. Diffuse is
// forwarded as that colour since it is the dominant visual term for the flat-
// shaded blocky lighting Minecraft's console ports already use elsewhere;
// ambient/specular/position are accepted but not separately representable.
void renderLightfv(int lightIndex, RenderLightParameter parameter, const float* values)
{
	if (lightIndex != 0 && lightIndex != 1)
		return;

	if (parameter == RenderLightParameter::Diffuse)
	{
		const rgb color = RGB15(
			static_cast<int>(values[0] * 31.0f),
			static_cast<int>(values[1] * 31.0f),
			static_cast<int>(values[2] * 31.0f));
		// Direction defaults to "straight down" until a Position update
		// supplies a real one; good enough for a first pass on top-down
		// block lighting.
		glLight(lightIndex, color, 0, floattov10(-1.0f), 0);
	}
}

void renderLightModelAmbient(const float*)
{
	// No global ambient term on this hardware's fixed lighting model.
}

void renderColorMaterial(RenderFace, RenderColorMaterialMode)
{
	// DS materials are set with glMaterialf() per material type, not toggled
	// on/off per face the way desktop GL_COLOR_MATERIAL is; nothing here maps
	// cleanly, and Minecraft's lighting mostly comes from the vertex colours
	// already carried by the mesh (see the lightmap block above), so this is
	// a no-op rather than a guess.
}

void renderShadeModel(RenderShadeModel model)
{
	g_poly.shade = model;
	markPolyDirty();
}

void renderClear(unsigned int mask)
{
	// libnds clears both colour and depth every frame as part of the render
	// pipeline itself (there is no separate "clear now" call); RenderClearMask
	// bits are accepted for API parity but the actual clearing happens via
	// renderClearColor()/renderClearDepth() below, applied at the next
	// glFlush(). Reject nothing: a caller clearing only Depth or only Color on
	// this hardware still gets both, which is a behavior difference worth
	// knowing about once something visible needs just one of them cleared.
	(void)mask;
}

void renderFinishGpu()
{
	// glFlush() below is already the synchronization point (it waits for
	// vblank); nothing extra to force here.
}

void renderSubmitFrame()
{
	// See RenderAPI.h's own doc comment on this function: DS has no
	// asynchronous present to kick off early, so this is a no-op and the
	// caller's own swap (glFlush()) remains the real synchronization point.
}

void renderClearColor(float r, float g, float b, float a)
{
	const auto toChannel = [](float v) -> std::uint8_t
	{
		if (v < 0.0f) v = 0.0f;
		if (v > 1.0f) v = 1.0f;
		return static_cast<std::uint8_t>(v * 31.0f + 0.5f);
	};
	glClearColor(toChannel(r), toChannel(g), toChannel(b), toChannel(a));
}

void renderClearDepth(double depth)
{
	if (depth < 0.0) depth = 0.0;
	if (depth > 1.0) depth = 1.0;
	glClearDepth(static_cast<fixed12d3>(depth * GL_MAX_DEPTH));
}

void renderPolygonOffset(float, float)
{
	// No polygon-offset-fill equivalent on this hardware's rasterizer.
}

void renderLineWidth(float)
{
	// DS lines are always 1 pixel wide; no width control exists.
}

void renderViewport(int x, int y, int width, int height)
{
	glViewport(static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y),
	           static_cast<std::uint8_t>(x + width - 1), static_cast<std::uint8_t>(y + height - 1));
}

void renderGetViewport(int* values)
{
	// glGetInt(GL_GET_VIEWPORT, ...) is a wider libnds call libnds exposes
	// for GFX_VIEWPORT read-back; not wired up because nothing in the shared
	// engine currently calls renderGetViewport() outside of screenshot code
	// this backend does not implement (PLATFORM_FRAMEBUFFER_READBACK is 0 for
	// DSi -- see PlatformConfig.h). Returns 0s rather than uninitialised
	// memory if that changes.
	values[0] = values[1] = values[2] = values[3] = 0;
}

void renderGetMatrix(RenderMatrixQuery query, float* values)
{
	GL_GET_ENUM which = GL_GET_MATRIX_POSITION;
	switch (query)
	{
		case RenderMatrixQuery::ModelView:  which = GL_GET_MATRIX_POSITION; break;
		case RenderMatrixQuery::Projection: which = GL_GET_MATRIX_PROJECTION; break;
		case RenderMatrixQuery::Texture:    which = GL_GET_MATRIX_POSITION; break; // No texture-matrix readback on this hardware.
	}
	int fixed[16];
	glGetFixed(which, fixed);
	for (int i = 0; i < 16; ++i)
		values[i] = static_cast<float>(fixed[i]) / (1 << 12); // f32 -> float
}

const unsigned char* renderGetString(RenderStringQuery query)
{
	switch (query)
	{
		case RenderStringQuery::Vendor:   return reinterpret_cast<const unsigned char*>("Nintendo");
		case RenderStringQuery::Renderer: return reinterpret_cast<const unsigned char*>("DS/DSi 3D engine (BlocksDS libnds)");
		case RenderStringQuery::Version:  return reinterpret_cast<const unsigned char*>("1.0 DSi");
		case RenderStringQuery::Extensions: return reinterpret_cast<const unsigned char*>("");
	}
	return reinterpret_cast<const unsigned char*>("");
}

bool renderSupportsFeature(RenderFeature feature)
{
	switch (feature)
	{
		case RenderFeature::Mipmaps: return true; // glTexImageNtr2D supports mip levels.
		default: return false; // No fancy fog distance, occlusion queries, anisotropic filtering or MSAA.
	}
}

unsigned int renderGetError()
{
	return 0; // libnds's GL wrapper has no glGetError()-equivalent to forward.
}

void renderFogHint(RenderHintMode)
{
	// No fastest/nicest fog quality switch on this hardware.
}

void renderMatrixMode(RenderMatrixMode mode)
{
	switch (mode)
	{
		case RenderMatrixMode::ModelView:  glMatrixMode(GL_MODELVIEW); break;
		case RenderMatrixMode::Projection: glMatrixMode(GL_PROJECTION); break;
		case RenderMatrixMode::Texture:    glMatrixMode(GL_TEXTURE); break;
	}
}

void renderLoadIdentity() { glLoadIdentity(); }
void renderPushMatrix() { glPushMatrix(); }
// glPopMatrix() takes a count (how many matrices to pop); RenderAPI.h's
// renderPopMatrix() always pops exactly one, matching every call site in the
// shared engine, which always pairs one renderPushMatrix() with one
// renderPopMatrix().
void renderPopMatrix() { glPopMatrix(1); }
void renderTranslate(float x, float y, float z) { glTranslatef(x, y, z); }
void renderRotate(float angle, float x, float y, float z) { glRotatef(angle, x, y, z); }
void renderScale(float x, float y, float z) { glScalef(x, y, z); }
void renderScaleDouble(double x, double y, double z)
{
	glScalef(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}
void renderFrustum(double left, double right, double bottom, double top, double nearValue, double farValue)
{
	glFrustumf32(floattof32((float)left), floattof32((float)right), floattof32((float)bottom),
	             floattof32((float)top), floattof32((float)nearValue), floattof32((float)farValue));
}
void renderOrtho(double left, double right, double bottom, double top, double nearValue, double farValue)
{
	glOrthof32(floattof32((float)left), floattof32((float)right), floattof32((float)bottom),
	           floattof32((float)top), floattof32((float)nearValue), floattof32((float)farValue));
}

bool renderCopyFramebufferToBoundTexture(int, int, int, int)
{
	// No framebuffer-to-texture copy path implemented yet; nothing in the
	// shared engine calls this unless a backend advertises support for it
	// through renderSupportsFeature(), which this one does not.
	return false;
}

void renderSetLegacyPresentationGamma(bool)
{
	// See the doc comment on this function in RenderAPI.h: Wii maps it to a
	// GX copy gamma, PS2 to its LUT fallback. Neither has an equivalent wired
	// up here yet; the DS 3D engine's own gamma/brightness fade registers
	// (see nds/arm9/video.h) are the place to add it if the Legacy Look
	// preset needs it later.
}

#endif // DSI_PLATFORM
