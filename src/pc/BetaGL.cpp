#define B173_NATIVE_GL
#include "OpenGL.h"
#include "BetaGLCalls.h"
#include "client/renderer/backend/BackendHost.h"
#include "client/renderer/portable/LegacyState.h"
#include "client/renderer/portable/Topology.h"
#include "client/renderer/portable/LineGeometry.h"
#include "lwjgl/GLContext.h"
#include "SDL.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace BetaGL
{
using namespace b173::render;

static Backend selectedBackend = Backend::Compatibility;
static bool selectionInitialized = false;
static bool selectedByArgument = false;
static bool selectionLocked = false;

static Backend parseBackendName(const char *value)
{
	if (!value || std::strcmp(value, "compat") == 0)
		return Backend::Compatibility;
	if (std::strcmp(value, "gl33") == 0)
		return Backend::Core33;
	if (std::strcmp(value, "gles2") == 0)
		return Backend::ES20;
	if (std::strcmp(value, "vulkan") == 0)
		return Backend::Vulkan;
	if (std::strcmp(value, "d3d12") == 0)
		return Backend::D3D12;
	throw std::invalid_argument("Unknown backend '" + std::string(value) +
								"'; choose compat, gl33, gles2, vulkan or d3d12");
}

Backend backend()
{
	if (!selectionInitialized)
	{
		selectedBackend = parseBackendName(std::getenv("B173_RENDERER"));
		selectionInitialized = true;
	}
	return selectedBackend;
}

bool consumeBackendArgument(int &index, int argc, char *const argv[])
{
	const char *value = nullptr;
	if (std::strcmp(argv[index], "--backend") == 0)
	{
		if (++index >= argc)
			throw std::invalid_argument("Missing value for --backend");
		value = argv[index];
	}
	else if (std::strncmp(argv[index], "--backend=", sizeof("--backend=") - 1) == 0)
		value = argv[index] + sizeof("--backend=") - 1;
	else
		return false;
	if (selectionLocked)
		throw std::logic_error("Backend selection must precede graphics initialization");
	if (selectedByArgument)
		throw std::invalid_argument("--backend may only be specified once");
	selectedBackend = parseBackendName(value);
	selectionInitialized = true;
	selectedByArgument = true;
	return true;
}

bool modern()
{
	return backend() != Backend::Compatibility;
}

struct CachedMesh
{
	MeshId id = 0;
	MeshData attributes;
	std::size_t firstByte = 0, endByte = 0;
};

struct Buffer
{
	std::vector<unsigned char> bytes;
	std::map<std::array<std::uintptr_t, 16>, CachedMesh> meshes;
};

struct Texture
{
	TextureDesc desc;
	std::vector<unsigned char> pixels;
	TextureId id = 0;
	bool samplingDirty = false;
	bool pixelsDirty = false;
};

struct Array
{
	GLint size = 0;
	GLenum type = 0;
	GLsizei stride = 0;
	const void *pointer = nullptr;
	GLuint buffer = 0;
	bool enabled = false;
};

// The seven list producers contain only meshes, a glyph advance, or a palette
// color. No GL command interpreter is needed. Draw state remains call-time state.
struct GeometryList
{
	std::vector<CachedMesh> meshes;
	Vec4 advance, color;
	bool hasColor = false;
};

struct Context
{
	std::unique_ptr<BackendHost> host;
	Device *device = nullptr;
	bool drawable = false;
	LegacyState state;
	std::string description;
	std::unordered_map<GLuint, Buffer> buffers;
	std::unordered_map<GLuint, Texture> textures;
	std::unordered_map<GLuint, GeometryList> lists;
	std::array<Array, 4> arrays;
	MeshData scratchMesh;
	MeshData expandedLines;
	MeshData listMesh;
	GLuint arrayBuffer = 0, indexBuffer = 0, texture = 0, compiling = 0;
	GLuint nextBuffer = 1, nextTexture = 1, nextList = 1;
	Fog fogMode = Fog::Exp;
	bool fogEnabled = false, cullEnabled = false;
	Cull cullMode = Cull::Back;
	Vec4 clearColor;
	float clearDepth = 1;
	int packAlignment = 4, unpackAlignment = 4;
	GLenum matrixMode = GL_MODELVIEW;
};

static std::unique_ptr<Context> context;

static Context &ctx()
{
	if (!context)
		throw std::logic_error("renderer is not initialized");
	return *context;
}

bool hasDrawableTarget()
{
	return !modern() || ctx().drawable;
}

void initialize(SDL_Window *window)
{
	selectionLocked = true;
	if (!modern())
		return;
	if (context)
		throw std::logic_error("renderer initialized twice");
	std::unique_ptr<Context> c(new Context);
	// The retained 4.2+ compatibility oracle maps signed normal byte zero to
	// zero. The old 2.1 bias adds visible diffuse light to perpendicular faces.
	c->state.normalConversion = NormalConversion::ModernSnorm;
	switch (backend())
	{
	case Backend::Core33:
		c->host = createGLHost(window, false);
		break;
	case Backend::ES20:
		c->host = createGLHost(window, true);
		break;
	case Backend::Vulkan:
#if defined(B173_RENDER_VULKAN)
		c->host = createVulkanHost(window);
		break;
#else
		throw std::runtime_error("Vulkan backend was not enabled in this build");
#endif
	case Backend::D3D12:
#if defined(B173_RENDER_D3D12)
		c->host = createD3D12Host(window);
		break;
#else
		throw std::runtime_error("Direct3D 12 backend was not enabled in this build");
#endif
	default:
		throw std::logic_error("compatibility rendering has no portable host");
	}
	c->device = &c->host->device();
	c->description = c->host->description();
	context = std::move(c);
	beginFrame();
}

void drawableSize(int *width, int *height)
{
	if (modern())
		ctx().host->drawableSize(width, height);
	else
		SDL_GL_GetDrawableSize(lwjgl::GLContext::detail::getWindow(), width, height);
}

void beginFrame()
{
	if (!modern())
		return;
	ctx().drawable = ctx().host->beginFrame();
}

void present()
{
	ctx().host->present();
}

bool supportsOcclusion()
{
	return modern() && ctx().host->supportsOcclusion();
}

void shutdown()
{
	context.reset();
}

const char *description()
{
	return modern() ? ctx().description.c_str() : "OpenGL compatibility";
}

static void require(bool condition, const char *operation)
{
	if (!condition)
		throw std::invalid_argument(std::string("unsupported Beta renderer operation: ") + operation);
}

static Vec4 vec4(const GLfloat *v)
{
	return {v[0], v[1], v[2], v[3]};
}

static Compare compare(GLenum value)
{
	require(value >= GL_NEVER && value <= GL_ALWAYS, "comparison");
	return static_cast<Compare>(value - GL_NEVER);
}

static BlendFactor blend(GLenum value)
{
	switch (value)
	{
	case GL_ZERO:
		return BlendFactor::Zero;
	case GL_ONE:
		return BlendFactor::One;
	case GL_SRC_COLOR:
		return BlendFactor::SrcColor;
	case GL_ONE_MINUS_SRC_COLOR:
		return BlendFactor::OneMinusSrcColor;
	case GL_DST_COLOR:
		return BlendFactor::DstColor;
	case GL_ONE_MINUS_DST_COLOR:
		return BlendFactor::OneMinusDstColor;
	case GL_SRC_ALPHA:
		return BlendFactor::SrcAlpha;
	case GL_ONE_MINUS_SRC_ALPHA:
		return BlendFactor::OneMinusSrcAlpha;
	case GL_DST_ALPHA:
		return BlendFactor::DstAlpha;
	case GL_ONE_MINUS_DST_ALPHA:
		return BlendFactor::OneMinusDstAlpha;
	case GL_SRC_ALPHA_SATURATE:
		return BlendFactor::SrcAlphaSaturate;
	default:
		throw std::invalid_argument("unsupported blend factor");
	}
}

static void materialColor()
{
	LegacyState &s = ctx().state;
	if (!s.colorMaterial)
		return;
	if (s.colorMaterialAmbient)
		s.materialAmbient = s.currentColor;
	if (s.colorMaterialDiffuse)
		s.materialDiffuse = s.currentColor;
}

static void setColor(Vec4 color)
{
	Context &c = ctx();
	if (c.compiling)
	{
		GeometryList &list = c.lists.at(c.compiling);
		require(list.meshes.empty() && c.listMesh.indices.empty(), "color after cached geometry");
		list.color = color;
		list.hasColor = true;
		return;
	}
	c.state.currentColor = color;
	materialColor();
}

static int arrayIndex(GLenum value)
{
	switch (value)
	{
	case GL_VERTEX_ARRAY:
		return 0;
	case GL_TEXTURE_COORD_ARRAY:
		return 1;
	case GL_COLOR_ARRAY:
		return 2;
	case GL_NORMAL_ARRAY:
		return 3;
	default:
		throw std::invalid_argument("unsupported vertex array");
	}
}

static void capability(GLenum value, bool enabled)
{
	Context &c = ctx();
	LegacyState &s = c.state;
	switch (value)
	{
	case GL_TEXTURE_2D:
		s.textureEnabled = enabled;
		break;
	case GL_ALPHA_TEST:
		s.alphaTest = enabled;
		break;
	case GL_BLEND:
		s.pipeline.blend = enabled;
		break;
	case GL_DEPTH_TEST:
		s.pipeline.depthTest = enabled;
		break;
	case GL_CULL_FACE:
		c.cullEnabled = enabled;
		s.pipeline.cull = enabled ? c.cullMode : Cull::None;
		break;
	case GL_FOG:
		c.fogEnabled = enabled;
		s.fog = enabled ? c.fogMode : Fog::None;
		break;
	case GL_LIGHTING:
		s.lighting = enabled;
		break;
	case GL_LIGHT0:
	case GL_LIGHT1:
		s.lights[value - GL_LIGHT0].enabled = enabled;
		break;
	case GL_COLOR_MATERIAL:
		s.colorMaterial = enabled;
		materialColor();
		break;
	case GL_NORMALIZE:
		s.normalizeNormals = enabled;
		break;
	case GL_RESCALE_NORMAL:
		s.rescaleNormals = enabled;
		break;
	case GL_POLYGON_OFFSET_FILL:
		s.pipeline.polygonOffset = enabled;
		break;
	default:
		throw std::invalid_argument("unsupported renderer capability " + std::to_string(value));
	}
}

static void arrayPointer(unsigned index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	Array &a = ctx().arrays[index];
	a.size = size;
	a.type = type;
	a.stride = stride;
	a.pointer = pointer;
	a.buffer = ctx().arrayBuffer;
}

static std::size_t scalarSize(GLenum type)
{
	switch (type)
	{
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		return 1;
	case GL_UNSIGNED_SHORT:
		return 2;
	case GL_FLOAT:
	case GL_UNSIGNED_INT:
		return 4;
	default:
		throw std::invalid_argument("unsupported vertex/index scalar");
	}
}

static const unsigned char *arrayData(const Array &a, std::size_t vertex)
{
	const std::size_t bytes = a.size * scalarSize(a.type);
	const std::size_t offset = vertex * (a.stride ? a.stride : bytes);
	if (!a.buffer)
		return static_cast<const unsigned char *>(a.pointer) + offset;
	const Buffer &buffer = ctx().buffers.at(a.buffer);
	const std::size_t start = reinterpret_cast<std::uintptr_t>(a.pointer) + offset;
	require(start <= buffer.bytes.size() && bytes <= buffer.bytes.size() - start, "vertex buffer range");
	return buffer.bytes.data() + start;
}

static TextureId boundTexture()
{
	Context &c = ctx();
	if (!c.state.textureEnabled || !c.texture)
		return 0;
	Texture &t = c.textures.at(c.texture);
	require(!t.pixels.empty(), "incomplete texture");
	if (!t.id || t.samplingDirty)
	{
		if (t.id)
			c.device->destroyTexture(t.id);
		t.id = c.device->createTexture(t.desc, t.pixels.data(), t.pixels.size());
		t.samplingDirty = false;
		t.pixelsDirty = false;
	}
	else if (t.pixelsDirty)
	{
		c.device->updateTexture(t.id, {0, 0, t.desc.width, t.desc.height}, t.pixels.data(), t.pixels.size());
		t.pixelsDirty = false;
	}
	return t.id;
}

static BlendFactor opaqueDestination(BlendFactor factor)
{
	// SDL's compatibility target has no alpha channel. Preserve its destination
	// alpha of one even on native swapchains that only offer RGBA formats.
	if (factor == BlendFactor::DstAlpha)
		return BlendFactor::One;
	if (factor == BlendFactor::OneMinusDstAlpha)
		return BlendFactor::Zero;
	return factor;
}

static Draw drawState(const MeshData &attributes)
{
	Context &c = ctx();
	Draw draw;
	draw.texture = boundTexture();
	draw.pipeline = c.state.pipeline;
	draw.pipeline.srcRGB = opaqueDestination(draw.pipeline.srcRGB);
	draw.pipeline.dstRGB = opaqueDestination(draw.pipeline.dstRGB);
	draw.uniforms = c.state.uniforms(attributes, nullptr, ClipConvention::OpenGL);
	return draw;
}

static CachedMesh cacheMesh(const MeshData &mesh)
{
	CachedMesh cached;
	cached.id = ctx().device->createMesh(mesh);
	cached.attributes.primitive = mesh.primitive;
	cached.attributes.hasTexture = mesh.hasTexture;
	cached.attributes.hasColor = mesh.hasColor;
	cached.attributes.hasNormal = mesh.hasNormal;
	return cached;
}

static void flushListMesh()
{
	Context &c = ctx();
	if (c.listMesh.indices.empty())
		return;
	c.lists.at(c.compiling).meshes.push_back(cacheMesh(c.listMesh));
	c.listMesh.vertices.clear();
	c.listMesh.indices.clear();
}

static void appendListMesh(const MeshData &mesh)
{
	Context &c = ctx();
	const Vec4 advance = c.lists.at(c.compiling).advance;
	require(advance.x == 0 && advance.y == 0 && advance.z == 0, "geometry after glyph advance");
	MeshData &list = c.listMesh;
	if (!list.indices.empty() && (list.primitive != mesh.primitive || list.hasTexture != mesh.hasTexture ||
								  list.hasColor != mesh.hasColor || list.hasNormal != mesh.hasNormal))
		flushListMesh();
	list.primitive = mesh.primitive;
	list.hasTexture = mesh.hasTexture;
	list.hasColor = mesh.hasColor;
	list.hasNormal = mesh.hasNormal;
	const std::uint32_t base = static_cast<std::uint32_t>(list.vertices.size());
	list.vertices.insert(list.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
	for (std::uint32_t index : mesh.indices)
		list.indices.push_back(base + index);
}

static void drawCached(const CachedMesh &mesh)
{
	if (!ctx().drawable)
		return;
	Draw draw = drawState(mesh.attributes);
	draw.mesh = mesh.id;
	ctx().device->draw(draw);
}

static SourceTopology topology(GLenum mode)
{
	switch (mode)
	{
	case GL_TRIANGLES:
		return SourceTopology::Triangles;
	case GL_TRIANGLE_STRIP:
		return SourceTopology::TriangleStrip;
	case GL_TRIANGLE_FAN:
		return SourceTopology::TriangleFan;
	case GL_LINES:
		return SourceTopology::Lines;
	case GL_LINE_STRIP:
		return SourceTopology::LineStrip;
	case GL_LINE_LOOP:
		return SourceTopology::LineLoop;
	case GL_POINTS:
		return SourceTopology::Points;
	default:
		throw std::invalid_argument("unsupported source topology");
	}
}

static void submit(GLenum mode, GLint first, GLsizei count, GLenum indexType, const void *indices)
{
	if (!count)
		return;
	require(first >= 0 && count > 0, "draw range");
	Context &c = ctx();
	if (!c.drawable && !c.compiling)
		return;
	require(c.arrays[0].enabled, "missing vertex array");
	Buffer *owner = c.arrays[0].buffer ? &c.buffers.at(c.arrays[0].buffer) : nullptr;
	std::array<std::uintptr_t, 16> key{};
	key[0] = mode;
	key[1] = first;
	key[2] = count;
	key[3] = indexType;
	key[4] = c.indexBuffer;
	key[5] = reinterpret_cast<std::uintptr_t>(indices);
	for (unsigned a = 0; a < 4; ++a)
	{
		key[6 + a * 2] = reinterpret_cast<std::uintptr_t>(c.arrays[a].pointer);
		key[7 + a * 2] = c.arrays[a].enabled ? c.arrays[a].stride + 1 : 0;
	}
	if (owner && !c.compiling)
	{
		auto found = owner->meshes.find(key);
		if (found != owner->meshes.end())
		{
			drawCached(found->second);
			return;
		}
	}
	MeshData &mesh = c.scratchMesh;
	mesh.hasTexture = c.arrays[1].enabled;
	mesh.hasColor = c.arrays[2].enabled;
	mesh.hasNormal = c.arrays[3].enabled;
	mesh.indices.resize(count);
	std::size_t vertexCount = static_cast<std::size_t>(first) + count;
	if (indexType)
	{
		const unsigned char *input = static_cast<const unsigned char *>(indices);
		if (c.indexBuffer)
		{
			const Buffer &buffer = c.buffers.at(c.indexBuffer);
			std::size_t offset = reinterpret_cast<std::uintptr_t>(indices);
			require(offset <= buffer.bytes.size() &&
						static_cast<std::size_t>(count) * scalarSize(indexType) <= buffer.bytes.size() - offset,
					"index buffer range");
			input = buffer.bytes.data() + offset;
		}
		vertexCount = 0;
		for (GLsizei i = 0; i < count; ++i)
		{
			std::uint32_t index;
			if (indexType == GL_UNSIGNED_SHORT)
			{
				std::uint16_t narrow;
				std::memcpy(&narrow, input + i * sizeof(narrow), sizeof(narrow));
				index = narrow;
			}
			else
				std::memcpy(&index, input + i * sizeof(index), sizeof(index));
			mesh.indices[i] = index;
			vertexCount = std::max(vertexCount, static_cast<std::size_t>(index) + 1);
		}
	}
	else
		std::iota(mesh.indices.begin(), mesh.indices.end(), static_cast<std::uint32_t>(first));
	mesh.vertices.resize(vertexCount);
	for (std::size_t i = 0; i < vertexCount; ++i)
	{
		Vertex &v = mesh.vertices[i];
		std::memcpy(&v.x, arrayData(c.arrays[0], i), 12);
		if (mesh.hasTexture)
			std::memcpy(&v.u, arrayData(c.arrays[1], i), 8);
		if (mesh.hasColor)
			std::memcpy(v.rgba.data(), arrayData(c.arrays[2], i), 4);
		if (mesh.hasNormal)
			std::memcpy(v.normal.data(), arrayData(c.arrays[3], i), 3);
	}
	if (mode == GL_TRIANGLES)
		mesh.primitive = Primitive::Triangles;
	else
		normalizeTopology(mesh, topology(mode));
	if (mesh.indices.empty())
		return;
	if (!c.compiling && mesh.primitive == Primitive::Lines && c.state.pipeline.lineWidth > c.host->maximumLineWidth())
	{
		const Mat4 transform =
			c.state.matrix(b173::render::MatrixMode::Projection) * c.state.matrix(b173::render::MatrixMode::ModelView);
		expandLines(mesh, transform, c.state.pipeline.viewport, c.state.pipeline.lineWidth, c.state.pipeline.flat,
					c.expandedLines);
		if (c.expandedLines.indices.empty())
			return;
		Draw draw = drawState(c.expandedLines);
		draw.pipeline.lineWidth = 1;
		draw.pipeline.flat = false;
		draw.pipeline.cull = Cull::None;
		draw.pipeline.polygonOffset = false;
		c.device->drawTransient(c.expandedLines, draw);
		return;
	}
	if (c.compiling)
		appendListMesh(mesh);
	else if (owner)
	{
		CachedMesh cached = cacheMesh(mesh);
		cached.firstByte = reinterpret_cast<std::uintptr_t>(c.arrays[0].pointer);
		cached.endByte = cached.firstByte + vertexCount * c.arrays[0].stride;
		owner->meshes.emplace(key, cached);
		drawCached(cached);
	}
	else
		c.device->drawTransient(mesh, drawState(mesh));
}

static Buffer &boundBuffer(GLenum target)
{
	require(target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER, "buffer target");
	Context &c = ctx();
	return c.buffers.at(target == GL_ARRAY_BUFFER ? c.arrayBuffer : c.indexBuffer);
}

static void invalidate(Buffer &buffer, std::size_t first, std::size_t end)
{
	for (auto it = buffer.meshes.begin(); it != buffer.meshes.end();)
	{
		if (it->second.firstByte < end && first < it->second.endByte)
		{
			ctx().device->destroyMesh(it->second.id);
			it = buffer.meshes.erase(it);
		}
		else
			++it;
	}
}

static void fogParameter(GLenum pname, GLfloat value)
{
	Context &c = ctx();
	switch (pname)
	{
	case GL_FOG_MODE:
		require(value == GL_LINEAR || value == GL_EXP || value == GL_EXP2, "fog mode");
		c.fogMode = value == GL_LINEAR ? Fog::Linear : value == GL_EXP ? Fog::Exp : Fog::Exp2;
		c.state.fog = c.fogEnabled ? c.fogMode : Fog::None;
		break;
	case GL_FOG_START:
		c.state.fogStart = value;
		break;
	case GL_FOG_END:
		c.state.fogEnd = value;
		break;
	case GL_FOG_DENSITY:
		c.state.fogDensity = value;
		break;
	case GL_FOG_DISTANCE_MODE_NV:
		require(value == GL_EYE_RADIAL_NV || value == GL_EYE_PLANE_ABSOLUTE_NV, "fog distance");
		c.state.radialFog = value == GL_EYE_RADIAL_NV;
		break;
	default:
		throw std::invalid_argument("unsupported fog parameter");
	}
}
} // namespace BetaGL

namespace BetaGL
{
void APIENTRY AlphaFunc(GLenum func, GLfloat ref)
{
	if (!modern())
		return glAlphaFunc(func, ref);
	ctx().state.alphaCompare = compare(func);
	ctx().state.alphaReference = std::max(0.0f, std::min(1.0f, ref));
}

void APIENTRY BeginQuery(GLenum target, GLuint id)
{
	if (!modern())
		return glBeginQuery(target, id);
	require(target == GL_SAMPLES_PASSED && supportsOcclusion(), "occlusion query capability");
	if (ctx().drawable)
		ctx().host->beginQuery(id);
}

void APIENTRY BindBuffer(GLenum target, GLuint buffer)
{
	if (!modern())
		return glBindBuffer(target, buffer);
	require(target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER, "buffer target");
	auto &c = ctx();
	if (buffer && c.buffers.find(buffer) == c.buffers.end())
		c.buffers.emplace(buffer, Buffer{});
	(target == GL_ARRAY_BUFFER ? c.arrayBuffer : c.indexBuffer) = buffer;
}

void APIENTRY BindTexture(GLenum target, GLuint texture)
{
	if (!modern())
		return glBindTexture(target, texture);
	require(target == GL_TEXTURE_2D, "texture target");
	auto &c = ctx();
	c.texture = texture;
	if (texture && c.textures.find(texture) == c.textures.end())
		c.textures.emplace(texture, Texture{});
}

void APIENTRY BlendFunc(GLenum sfactor, GLenum dfactor)
{
	if (!modern())
		return glBlendFunc(sfactor, dfactor);
	auto &p = ctx().state.pipeline;
	p.srcRGB = p.srcAlpha = blend(sfactor);
	p.dstRGB = p.dstAlpha = blend(dfactor);
}

void APIENTRY BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	if (!modern())
		return glBufferData(target, size, data, usage);
	require(size >= 0, "buffer size");
	auto &b = boundBuffer(target);
	invalidate(b, 0, b.bytes.size());
	b.bytes.resize(static_cast<std::size_t>(size));
	if (data && size)
		std::memcpy(b.bytes.data(), data, static_cast<std::size_t>(size));
	(void)usage;
}

void APIENTRY CallList(GLuint list)
{
	if (!modern())
		return glCallList(list);
	auto &c = ctx();
	auto found = c.lists.find(list);
	if (found == c.lists.end())
		return;
	const auto &entry = found->second;
	if (entry.hasColor)
		setColor(entry.color);
	for (const auto &mesh : entry.meshes)
		drawCached(mesh);
	if (entry.advance.x != 0 || entry.advance.y != 0 || entry.advance.z != 0)
		c.state.translate(entry.advance.x, entry.advance.y, entry.advance.z);
}

void APIENTRY CallLists(GLsizei n, GLenum type, const void *lists)
{
	if (!modern())
		return glCallLists(n, type, lists);
	require(type == GL_UNSIGNED_INT, "list index format");
	const GLuint *ids = static_cast<const GLuint *>(lists);
	for (GLsizei i = 0; i < n; ++i)
		CallList(ids[i]);
}

void APIENTRY Clear(GLbitfield mask)
{
	if (!modern())
		return glClear(mask);
	require((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) == 0, "clear mask");
	auto &c = ctx();
	if (c.drawable)
		c.host->clear(c.clearColor, c.clearDepth, (mask & GL_COLOR_BUFFER_BIT) != 0, (mask & GL_DEPTH_BUFFER_BIT) != 0,
					  c.state.pipeline);
}

void APIENTRY ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	if (!modern())
		return glClearColor(red, green, blue, alpha);
	ctx().clearColor = {red, green, blue, alpha};
}

void APIENTRY ClearDepth(GLdouble depth)
{
	if (!modern())
		return glClearDepth(depth);
	ctx().clearDepth = static_cast<float>(std::max(0.0, std::min(1.0, depth)));
}

void APIENTRY Color3f(GLfloat red, GLfloat green, GLfloat blue)
{
	if (!modern())
		return glColor3f(red, green, blue);
	setColor({red, green, blue, 1});
}

void APIENTRY Color4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	if (!modern())
		return glColor4f(red, green, blue, alpha);
	setColor({red, green, blue, alpha});
}

void APIENTRY ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	if (!modern())
		return glColorMask(red, green, blue, alpha);
	ctx().state.pipeline.colorMask = (red ? 1 : 0) | (green ? 2 : 0) | (blue ? 4 : 0) | (alpha ? 8 : 0);
}

void APIENTRY ColorMaterial(GLenum face, GLenum mode)
{
	if (!modern())
		return glColorMaterial(face, mode);
	require((face == GL_FRONT || face == GL_FRONT_AND_BACK) && (mode == GL_AMBIENT || mode == GL_AMBIENT_AND_DIFFUSE),
			"color material");
	auto &s = ctx().state;
	s.colorMaterialAmbient = true;
	s.colorMaterialDiffuse = mode == GL_AMBIENT_AND_DIFFUSE;
	materialColor();
}

void APIENTRY ColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	if (!modern())
		return glColorPointer(size, type, stride, pointer);
	require(size == 4 && type == GL_UNSIGNED_BYTE, "color format");
	arrayPointer(2, size, type, stride, pointer);
}

void APIENTRY CullFace(GLenum mode)
{
	if (!modern())
		return glCullFace(mode);
	require(mode == GL_FRONT || mode == GL_BACK || mode == GL_FRONT_AND_BACK, "cull mode");
	auto &c = ctx();
	c.cullMode = mode == GL_FRONT ? Cull::Front : mode == GL_BACK ? Cull::Back : Cull::Both;
	c.state.pipeline.cull = c.cullEnabled ? c.cullMode : Cull::None;
}

void APIENTRY DeleteLists(GLuint list, GLsizei range)
{
	if (!modern())
		return glDeleteLists(list, range);
	auto &c = ctx();
	for (GLsizei i = 0; i < range; ++i)
	{
		auto found = c.lists.find(list + i);
		if (found == c.lists.end())
			continue;
		for (const auto &mesh : found->second.meshes)
			c.device->destroyMesh(mesh.id);
		c.lists.erase(found);
	}
}

void APIENTRY DeleteQueries(GLsizei n, const GLuint *ids)
{
	if (!modern())
		return glDeleteQueries(n, ids);
	require(supportsOcclusion(), "occlusion query capability");
	ctx().host->deleteQueries(n, ids);
}

void APIENTRY DeleteTextures(GLsizei n, const GLuint *textures)
{
	if (!modern())
		return glDeleteTextures(n, textures);
	auto &c = ctx();
	for (GLsizei i = 0; i < n; ++i)
	{
		auto found = c.textures.find(textures[i]);
		if (found == c.textures.end())
			continue;
		if (found->second.id)
			c.device->destroyTexture(found->second.id);
		c.textures.erase(found);
		if (c.texture == textures[i])
			c.texture = 0;
	}
}

void APIENTRY DepthFunc(GLenum func)
{
	if (!modern())
		return glDepthFunc(func);
	ctx().state.pipeline.depthCompare = compare(func);
}

void APIENTRY DepthMask(GLboolean flag)
{
	if (!modern())
		return glDepthMask(flag);
	ctx().state.pipeline.depthWrite = flag != 0;
}

void APIENTRY Disable(GLenum cap)
{
	if (!modern())
		return glDisable(cap);
	capability(cap, false);
}

void APIENTRY DisableClientState(GLenum array)
{
	if (!modern())
		return glDisableClientState(array);
	ctx().arrays[arrayIndex(array)].enabled = false;
}

void APIENTRY DrawArrays(GLenum mode, GLint first, GLsizei count)
{
	if (!modern())
		return glDrawArrays(mode, first, count);
	submit(mode, first, count, 0, nullptr);
}

void APIENTRY Enable(GLenum cap)
{
	if (!modern())
		return glEnable(cap);
	capability(cap, true);
}

void APIENTRY EnableClientState(GLenum array)
{
	if (!modern())
		return glEnableClientState(array);
	ctx().arrays[arrayIndex(array)].enabled = true;
}

void APIENTRY EndList()
{
	if (!modern())
		return glEndList();
	require(ctx().compiling != 0, "list end");
	flushListMesh();
	ctx().compiling = 0;
}

void APIENTRY EndQuery(GLenum target)
{
	if (!modern())
		return glEndQuery(target);
	require(target == GL_SAMPLES_PASSED && supportsOcclusion(), "occlusion query capability");
	if (ctx().drawable)
		ctx().host->endQuery();
}

void APIENTRY Fogf(GLenum pname, GLfloat param)
{
	if (!modern())
		return glFogf(pname, param);
	fogParameter(pname, param);
}

void APIENTRY Fogfv(GLenum pname, const GLfloat *params)
{
	if (!modern())
		return glFogfv(pname, params);
	if (pname == GL_FOG_COLOR)
		ctx().state.fogColor = vec4(params);
	else
		fogParameter(pname, *params);
}

void APIENTRY Fogi(GLenum pname, GLint param)
{
	if (!modern())
		return glFogi(pname, param);
	fogParameter(pname, static_cast<float>(param));
}

void APIENTRY Frustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	if (!modern())
		return glFrustum(left, right, bottom, top, zNear, zFar);
	ctx().state.multMatrix(Mat4::frustum(left, right, bottom, top, zNear, zFar));
}

void APIENTRY GenBuffers(GLsizei n, GLuint *buffers)
{
	if (!modern())
		return glGenBuffers(n, buffers);
	auto &c = ctx();
	for (GLsizei i = 0; i < n; ++i)
	{
		buffers[i] = c.nextBuffer++;
		c.buffers.emplace(buffers[i], Buffer{});
	}
}

GLuint APIENTRY GenLists(GLsizei range)
{
	if (!modern())
		return glGenLists(range);
	require(range > 0, "list range");
	auto &c = ctx();
	GLuint first = c.nextList;
	c.nextList += range;
	return first;
}

void APIENTRY GenQueries(GLsizei n, GLuint *ids)
{
	if (!modern())
		return glGenQueries(n, ids);
	require(supportsOcclusion(), "occlusion query capability");
	ctx().host->genQueries(n, ids);
}

void APIENTRY GenTextures(GLsizei n, GLuint *textures)
{
	if (!modern())
		return glGenTextures(n, textures);
	auto &c = ctx();
	for (GLsizei i = 0; i < n; ++i)
	{
		textures[i] = c.nextTexture++;
		c.textures.emplace(textures[i], Texture{});
	}
}

GLenum APIENTRY GetError()
{
	if (!modern())
		return glGetError();
	return ctx().host->error();
}

void APIENTRY GetFloatv(GLenum pname, GLfloat *data)
{
	if (!modern())
		return glGetFloatv(pname, data);
	b173::render::MatrixMode mode;
	if (pname == GL_MODELVIEW_MATRIX)
		mode = b173::render::MatrixMode::ModelView;
	else if (pname == GL_PROJECTION_MATRIX)
		mode = b173::render::MatrixMode::Projection;
	else if (pname == GL_TEXTURE_MATRIX)
		mode = b173::render::MatrixMode::Texture;
	else
		throw std::invalid_argument("unsupported float state query");
	std::memcpy(data, ctx().state.matrix(mode).v.data(), sizeof(float) * 16);
}

void APIENTRY GetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
	if (!modern())
		return glGetQueryObjectuiv(id, pname, params);
	require(supportsOcclusion() && (pname == GL_QUERY_RESULT_AVAILABLE || pname == GL_QUERY_RESULT),
			"occlusion query result");
	*params = ctx().host->queryResult(id, pname == GL_QUERY_RESULT_AVAILABLE);
}

const GLubyte *APIENTRY GetString(GLenum name)
{
	if (!modern())
		return glGetString(name);
	if (name == GL_VERSION || name == GL_RENDERER || name == GL_VENDOR)
		return reinterpret_cast<const GLubyte *>(description());
	if (name == GL_EXTENSIONS)
		return reinterpret_cast<const GLubyte *>("GL_NV_fog_distance");
	throw std::invalid_argument("unsupported string state query");
}

void APIENTRY LightModelfv(GLenum pname, const GLfloat *params)
{
	if (!modern())
		return glLightModelfv(pname, params);
	require(pname == GL_LIGHT_MODEL_AMBIENT, "light model");
	ctx().state.globalAmbient = vec4(params);
}

void APIENTRY Lightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	if (!modern())
		return glLightfv(light, pname, params);
	require(light == GL_LIGHT0 || light == GL_LIGHT1, "light index");
	auto &s = ctx().state;
	switch (pname)
	{
	case GL_POSITION:
		s.setLightPosition(light - GL_LIGHT0, vec4(params));
		break;
	case GL_AMBIENT:
		s.lights[light - GL_LIGHT0].ambient = vec4(params);
		break;
	case GL_DIFFUSE:
		s.lights[light - GL_LIGHT0].diffuse = vec4(params);
		break;
	case GL_SPECULAR:
		require(params[0] == 0 && params[1] == 0 && params[2] == 0, "nonzero specular light");
		break;
	default:
		throw std::invalid_argument("unsupported light parameter");
	}
}

void APIENTRY LineWidth(GLfloat width)
{
	if (!modern())
		return glLineWidth(width);
	ctx().state.pipeline.lineWidth = width;
}

void APIENTRY LoadIdentity()
{
	if (!modern())
		return glLoadIdentity();
	ctx().state.loadIdentity();
}

void APIENTRY MatrixMode(GLenum mode)
{
	if (!modern())
		return glMatrixMode(mode);
	require(mode == GL_MODELVIEW || mode == GL_PROJECTION || mode == GL_TEXTURE, "matrix mode");
	ctx().matrixMode = mode;
	ctx().state.matrixMode(mode == GL_MODELVIEW	   ? b173::render::MatrixMode::ModelView
						   : mode == GL_PROJECTION ? b173::render::MatrixMode::Projection
												   : b173::render::MatrixMode::Texture);
}

void APIENTRY NewList(GLuint list, GLenum mode)
{
	if (!modern())
		return glNewList(list, mode);
	require(mode == GL_COMPILE && !ctx().compiling, "list compilation");
	auto &c = ctx();
	auto &entry = c.lists[list];
	for (const auto &mesh : entry.meshes)
		c.device->destroyMesh(mesh.id);
	entry = GeometryList{};
	c.compiling = list;
	c.listMesh.vertices.clear();
	c.listMesh.indices.clear();
}

void APIENTRY Normal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	if (!modern())
		return glNormal3f(nx, ny, nz);
	ctx().state.currentNormal = {nx, ny, nz, 0};
}

void APIENTRY NormalPointer(GLenum type, GLsizei stride, const void *pointer)
{
	if (!modern())
		return glNormalPointer(type, stride, pointer);
	require(type == GL_BYTE, "normal format");
	arrayPointer(3, 3, type, stride, pointer);
}

void APIENTRY Ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	if (!modern())
		return glOrtho(left, right, bottom, top, zNear, zFar);
	ctx().state.multMatrix(Mat4::ortho(left, right, bottom, top, zNear, zFar));
}

void APIENTRY PixelStorei(GLenum pname, GLint param)
{
	if (!modern())
		return glPixelStorei(pname, param);
	require((pname == GL_PACK_ALIGNMENT || pname == GL_UNPACK_ALIGNMENT) &&
				(param == 1 || param == 2 || param == 4 || param == 8),
			"pixel alignment");
	(pname == GL_PACK_ALIGNMENT ? ctx().packAlignment : ctx().unpackAlignment) = param;
}

void APIENTRY PolygonOffset(GLfloat factor, GLfloat units)
{
	if (!modern())
		return glPolygonOffset(factor, units);
	ctx().state.pipeline.offsetFactor = factor;
	ctx().state.pipeline.offsetUnits = units;
}

void APIENTRY PopMatrix()
{
	if (!modern())
		return glPopMatrix();
	ctx().state.popMatrix();
}

void APIENTRY PushMatrix()
{
	if (!modern())
		return glPushMatrix();
	ctx().state.pushMatrix();
}

void APIENTRY ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
	if (!modern())
		return glReadPixels(x, y, width, height, format, type, pixels);
	require(type == GL_UNSIGNED_BYTE && (format == GL_RGB || format == GL_RGBA), "readback format");
	auto rgba = ctx().host->readPixels({x, y, width, height});
	const std::size_t components = format == GL_RGBA ? 4 : 3;
	const std::size_t row = static_cast<std::size_t>(width) * components;
	const std::size_t stride = (row + ctx().packAlignment - 1) & ~(static_cast<std::size_t>(ctx().packAlignment) - 1);
	for (int yy = 0; yy < height; ++yy)
		for (int xx = 0; xx < width; ++xx)
		{
			unsigned char *out = static_cast<unsigned char *>(pixels) + yy * stride + xx * components;
			std::memcpy(out, rgba.data() + (static_cast<std::size_t>(yy) * width + xx) * 4, 3);
			if (components == 4)
				out[3] = 255;
		}
}

void APIENTRY Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	if (!modern())
		return glRotatef(angle, x, y, z);
	ctx().state.rotate(angle, x, y, z);
}

void APIENTRY Scaled(GLdouble x, GLdouble y, GLdouble z)
{
	if (!modern())
		return glScaled(x, y, z);
	ctx().state.scale(x, y, z);
}

void APIENTRY Scalef(GLfloat x, GLfloat y, GLfloat z)
{
	if (!modern())
		return glScalef(x, y, z);
	ctx().state.scale(x, y, z);
}

void APIENTRY ShadeModel(GLenum mode)
{
	if (!modern())
		return glShadeModel(mode);
	require(mode == GL_FLAT || mode == GL_SMOOTH, "shade model");
	ctx().state.pipeline.flat = mode == GL_FLAT;
}

void APIENTRY TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	if (!modern())
		return glTexCoordPointer(size, type, stride, pointer);
	require(size == 2 && type == GL_FLOAT, "UV format");
	arrayPointer(1, size, type, stride, pointer);
}

void APIENTRY TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
						 GLenum format, GLenum type, const void *pixels)
{
	if (!modern())
		return glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
	require(target == GL_TEXTURE_2D && level == 0 && border == 0 && internalformat == GL_RGBA && format == GL_RGBA &&
				type == GL_UNSIGNED_BYTE,
			"texture image format");
	auto &c = ctx();
	auto &t = c.textures.at(c.texture);
	if (t.id)
	{
		c.device->destroyTexture(t.id);
		t.id = 0;
	}
	t.desc.width = width;
	t.desc.height = height;
	t.pixels.resize(rgbaByteCount(width, height));
	if (pixels)
		std::memcpy(t.pixels.data(), pixels, t.pixels.size());
	t.samplingDirty = true;
}

void APIENTRY TexParameteri(GLenum target, GLenum pname, GLint param)
{
	if (!modern())
		return glTexParameteri(target, pname, param);
	require(target == GL_TEXTURE_2D, "texture target");
	auto &t = ctx().textures.at(ctx().texture);
	if (pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER)
	{
		require(param == GL_NEAREST || param == GL_LINEAR, "texture filter");
		(pname == GL_TEXTURE_MIN_FILTER ? t.desc.minFilter : t.desc.magFilter) =
			param == GL_NEAREST ? Filter::Nearest : Filter::Linear;
	}
	else
	{
		require(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T, "texture parameter");
		require(param == GL_REPEAT || param == GL_CLAMP || param == GL_CLAMP_TO_EDGE, "texture wrap");
		(pname == GL_TEXTURE_WRAP_S ? t.desc.wrapS : t.desc.wrapT) = param == GL_REPEAT	 ? Wrap::Repeat
																	 : param == GL_CLAMP ? Wrap::LegacyClamp
																						 : Wrap::Edge;
	}
	t.samplingDirty = true;
}

void APIENTRY TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
							GLenum format, GLenum type, const void *pixels)
{
	if (!modern())
		return glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
	require(target == GL_TEXTURE_2D && level == 0 && format == GL_RGBA && type == GL_UNSIGNED_BYTE,
			"texture update format");
	auto &c = ctx();
	auto &t = c.textures.at(c.texture);
	require(xoffset >= 0 && yoffset >= 0 && width >= 0 && height >= 0 && xoffset + width <= t.desc.width &&
				yoffset + height <= t.desc.height,
			"texture update rectangle");
	for (int y = 0; y < height; ++y)
		std::memcpy(t.pixels.data() + (static_cast<std::size_t>(yoffset + y) * t.desc.width + xoffset) * 4,
					static_cast<const unsigned char *>(pixels) + static_cast<std::size_t>(y) * width * 4,
					static_cast<std::size_t>(width) * 4);
	if (t.id && width && height)
	{
		if (c.drawable && !t.pixelsDirty && !t.samplingDirty)
			c.device->updateTexture(t.id, {xoffset, yoffset, width, height}, pixels,
									static_cast<std::size_t>(width) * height * 4);
		else
			t.pixelsDirty = true;
	}
}

void APIENTRY Translatef(GLfloat x, GLfloat y, GLfloat z)
{
	if (!modern())
		return glTranslatef(x, y, z);
	auto &c = ctx();
	if (c.compiling)
	{
		auto &v = c.lists.at(c.compiling).advance;
		v.x += x;
		v.y += y;
		v.z += z;
	}
	else
		c.state.translate(x, y, z);
}

void APIENTRY VertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	if (!modern())
		return glVertexPointer(size, type, stride, pointer);
	require(size == 3 && type == GL_FLOAT, "position format");
	arrayPointer(0, size, type, stride, pointer);
}

void APIENTRY Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if (!modern())
		return glViewport(x, y, width, height);
	ctx().state.pipeline.viewport = {x, y, width, height};
}

void APIENTRY DeleteBuffers(GLsizei n, const GLuint *buffers)
{
	if (!modern())
		return glDeleteBuffers(n, buffers);
	auto &c = ctx();
	for (GLsizei i = 0; i < n; ++i)
	{
		auto found = c.buffers.find(buffers[i]);
		if (found == c.buffers.end())
			continue;
		invalidate(found->second, 0, found->second.bytes.size());
		c.buffers.erase(found);
		if (c.arrayBuffer == buffers[i])
			c.arrayBuffer = 0;
		if (c.indexBuffer == buffers[i])
			c.indexBuffer = 0;
	}
}

void APIENTRY BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
	if (!modern())
		return glBufferSubData(target, offset, size, data);
	auto &b = boundBuffer(target);
	require(offset >= 0 && size >= 0 && static_cast<std::size_t>(offset) <= b.bytes.size() &&
				static_cast<std::size_t>(size) <= b.bytes.size() - offset,
			"buffer update range");
	invalidate(b, offset, offset + size);
	if (size)
		std::memcpy(b.bytes.data() + offset, data, static_cast<std::size_t>(size));
}

void APIENTRY GetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
	if (!modern())
		return glGetBufferSubData(target, offset, size, data);
	auto &b = boundBuffer(target);
	require(offset >= 0 && size >= 0 && static_cast<std::size_t>(offset) <= b.bytes.size() &&
				static_cast<std::size_t>(size) <= b.bytes.size() - offset,
			"buffer read range");
	if (size)
		std::memcpy(data, b.bytes.data() + offset, static_cast<std::size_t>(size));
}

void APIENTRY DrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	if (!modern())
		return glDrawElements(mode, count, type, indices);
	require(type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT, "index type");
	submit(mode, 0, count, type, indices);
}

void APIENTRY GetIntegerv(GLenum pname, GLint *data)
{
	if (!modern())
		return glGetIntegerv(pname, data);
	if (pname == GL_PACK_ALIGNMENT)
		*data = ctx().packAlignment;
	else if (pname == GL_UNPACK_ALIGNMENT)
		*data = ctx().unpackAlignment;
	else if (pname == GL_READ_BUFFER)
		*data = GL_BACK;
	else if (pname == GL_MATRIX_MODE)
		*data = ctx().matrixMode;
	else
		throw std::invalid_argument("unsupported integer state query");
}

void APIENTRY ReadBuffer(GLenum mode)
{
	if (!modern())
		return glReadBuffer(mode);
	require(mode == GL_BACK, "read buffer");
}

void APIENTRY Finish()
{
	if (!modern())
		return glFinish();
	ctx().host->finish();
}
} // namespace BetaGL
