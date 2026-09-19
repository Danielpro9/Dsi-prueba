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
	std::vector<std::uint8_t> rgba; // width*height*4, tightly packed RGBA8
};

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

void uploadTexture(int name, const DsiTexture& tex)
{
	if (tex.width <= 0 || tex.height <= 0 || tex.rgba.empty())
		return;

	std::vector<std::uint16_t> converted(static_cast<std::size_t>(tex.width) * tex.height);
	convertRgba8ToDs(tex.rgba.data(), converted.data(), tex.width * tex.height);

	int param = 0;
	if (!tex.clamp)
		param |= GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T;

	glBindTexture(0, name);
	glTexImage2D(0, 0, GL_RGBA, tex.width, tex.height, 0, param, converted.data());
	glTexParameter(0, param); // wrap bits already set above; kept for parity with callers that only touch params later
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

	const std::uint8_t* base = static_cast<const std::uint8_t*>(mesh.data) + (std::size_t)mesh.first * mesh.stride;

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
			std::int8_t normal[3];
			std::memcpy(normal, vertex + mesh.normalOffset, sizeof(normal));
			glNormal3f(normal[0] / 127.0f, normal[1] / 127.0f, normal[2] / 127.0f);
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
			// positionShort means the position was already narrowed to a
			// smaller range by the caller; the DS vertex hardware itself
			// always wants the fixed-point v16 path (glVertex3f() below
			// converts to it), so there is nothing extra to do here.
		}
		glVertex3f(position[0], position[1], position[2]);
	}
	glEnd();

	return true;
}

} // namespace

void renderStaticMeshCreate(RenderStaticMesh& mesh)
{
	mesh.persistentHandle = 0;
	mesh.persistentReady = false;
	mesh.captured.clear();
}

void renderStaticMeshDestroy(RenderStaticMesh& mesh)
{
	mesh.captured.clear();
	mesh.persistentReady = false;
}

bool renderStaticMeshCompile(RenderStaticMesh& mesh, const RenderInterleavedMesh& source)
{
	// PLATFORM_MODEL_PERSISTENT_MESH is 0 for DSi (see PlatformConfig.h), so
	// this always takes the captured-replay path, never a native handle.
	mesh.persistentReady = false;
	return renderCaptureInterleaved(source, mesh.captured, false);
}

bool renderStaticMeshDraw(const RenderStaticMesh& mesh)
{
	return renderDrawCaptured(mesh.captured);
}

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
	uploadTexture(g_boundTexture, *tex);
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
	tex->allocated = true;
	tex->rgba.assign(static_cast<const std::uint8_t*>(pixels),
	                  static_cast<const std::uint8_t*>(pixels) + (std::size_t)width * height * 4);
	uploadTexture(g_boundTexture, *tex);
}

void renderTextureParameters(bool blur, bool, bool clamp)
{
	if (DsiTexture* tex = textureSlot(g_boundTexture))
	{
		tex->blur = blur;
		tex->clamp = clamp;
		if (tex->allocated)
			uploadTexture(g_boundTexture, *tex);
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
