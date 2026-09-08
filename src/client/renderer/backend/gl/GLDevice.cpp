#include "client/renderer/backend/gl/GLDevice.h"

#include "client/renderer/portable/LegacyState.h"
#include "client/renderer/portable/ShaderSource.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
#define B173_GL_CALL __stdcall
#else
#define B173_GL_CALL
#endif

namespace b173
{
namespace render
{
namespace GLDetail
{
using Enum = unsigned;
using UInt = unsigned;
using Int = int;
using Size = int;
using Bool = unsigned char;
using PtrSize = std::ptrdiff_t;
constexpr Enum ARRAY_BUFFER = 0x8892, ELEMENT_BUFFER = 0x8893, STATIC_DRAW = 0x88E4, STREAM_DRAW = 0x88E0;
constexpr Enum FLOAT = 0x1406, UBYTE = 0x1401, USHORT = 0x1403, UINT = 0x1405;
constexpr Enum VERTEX_SHADER = 0x8B31, FRAGMENT_SHADER = 0x8B30, COMPILE_STATUS = 0x8B81, LINK_STATUS = 0x8B82,
			   INFO_LOG_LENGTH = 0x8B84;
constexpr Enum TEXTURE_2D = 0x0DE1, TEXTURE0 = 0x84C0, RGBA = 0x1908, RGBA8 = 0x8058;
constexpr Enum MIN_FILTER = 0x2801, MAG_FILTER = 0x2800, WRAP_S = 0x2802, WRAP_T = 0x2803;
constexpr Enum NEAREST = 0x2600, LINEAR = 0x2601, REPEAT = 0x2901, EDGE = 0x812F;
constexpr Enum BLEND = 0x0BE2, DEPTH_TEST = 0x0B71, CULL_FACE = 0x0B44, SCISSOR = 0x0C11, POLYGON_OFFSET = 0x8037;
constexpr Enum BACK = 0x0405, FRONT = 0x0404, BOTH = 0x0408, CCW = 0x0901, CW = 0x0900;
constexpr Enum PACK_ALIGNMENT = 0x0D05, UNPACK_ALIGNMENT = 0x0CF5, UNPACK_ROW_LENGTH = 0x0CF2, PACK_ROW_LENGTH = 0x0D02;
constexpr Enum FRAMEBUFFER = 0x8D40, VERSION = 0x1F02, RENDERER = 0x1F01, HIGH_FLOAT = 0x8DF2;
constexpr Enum DEPTH_BIT = 0x100, COLOR_BIT = 0x4000, PIXEL_PACK_BUFFER = 0x88EB, PIXEL_UNPACK_BUFFER = 0x88EC;
constexpr Enum FRAMEBUFFER_SRGB = 0x8DB9, ALIASED_LINE_WIDTH_RANGE = 0x846E;
// GL_NEVER; Compare is declared in the same order as GL_NEVER..GL_ALWAYS.
constexpr Enum COMPARE_BASE = 0x0200;

#define B173_GL_REQUIRED(X)                                                                                            \
	X(void, GenBuffers, (Size, UInt *))                                                                                \
	X(void, DeleteBuffers,                                                                                             \
	  (Size, const UInt *)) X(void, BindBuffer, (Enum, UInt)) X(void, BufferData, (Enum, PtrSize, const void *, Enum)) \
		X(void, BufferSubData, (Enum, PtrSize, PtrSize, const void *)) X(UInt, CreateShader, (Enum)) X(                \
			void, ShaderSource, (UInt, Size, const char *const *, const Int *)) X(void, CompileShader, (UInt))         \
			X(void, GetShaderiv, (UInt, Enum, Int *)) X(void, GetShaderInfoLog, (UInt, Size, Size *, char *)) X(       \
				void, DeleteShader, (UInt)) X(UInt, CreateProgram, ()) X(void, AttachShader, (UInt, UInt))             \
				X(void, BindAttribLocation, (UInt, UInt, const char *)) X(void, LinkProgram, (UInt)) X(                \
					void,                                                                                              \
					GetProgramiv,                                                                                      \
					(UInt, Enum, Int *)) X(void, GetProgramInfoLog, (UInt, Size, Size *, char *))                      \
					X(void, DeleteProgram, (UInt)) X(void, UseProgram, (UInt)) X(                                      \
						Int, GetUniformLocation, (UInt, const char *)) X(void, Uniform4fv, (Int, Size, const float *)) \
						X(void, Uniform1i, (Int, Int)) X(void, EnableVertexAttribArray, (UInt)) X(                     \
							void,                                                                                      \
							VertexAttribPointer,                                                                       \
							(UInt, Int, Enum, Bool, Size, const void *)) X(void, GenTextures, (Size, UInt *))          \
							X(void, DeleteTextures, (Size, const UInt *)) X(void, ActiveTexture, (Enum)) X(            \
								void,                                                                                  \
								BindTexture, (Enum, UInt)) X(void, TexParameteri, (Enum, Enum, Int))                   \
								X(void, TexImage2D, (Enum, Int, Int, Size, Size, Int, Enum, Enum, const void *)) X(    \
									void,                                                                              \
									TexSubImage2D, (Enum, Int, Int, Int, Size, Size, Enum, Enum, const void *))        \
									X(void, PixelStorei, (Enum, Int)) X(void, Enable, (Enum)) X(void, Disable, (Enum)) \
										X(void, DepthFunc, (Enum)) X(void, DepthMask, (Bool)) X(                       \
											void, ColorMask, (Bool, Bool, Bool, Bool))                                 \
											X(void, BlendFuncSeparate,                                                 \
											  (Enum, Enum, Enum, Enum)) X(void, BlendEquationSeparate, (Enum, Enum))   \
												X(void, CullFace, (Enum)) X(void, FrontFace, (Enum)) X(                \
													void,                                                              \
													PolygonOffset, (float, float)) X(void, LineWidth, (float))         \
													X(void, Viewport,                                                  \
													  (Int, Int, Size, Size)) X(void, Scissor, (Int, Int, Size, Size)) \
														X(void, DrawElements, (Enum, Size, Enum, const void *))        \
															X(void, BindFramebuffer, (Enum, UInt))                     \
																X(void, ReadPixels,                                    \
																  (Int, Int, Size, Size, Enum, Enum,                   \
																   void *)) X(void, Clear, (Enum))                     \
																	X(void, ClearColor, (float, float, float, float))  \
																		X(Enum, GetError, ()) X(                       \
																			const unsigned char *, GetString, (Enum))  \
																			X(void, GetIntegerv, (Enum, Int *))        \
																				X(void, GetFloatv, (Enum, float *))

struct API
{
#define B173_GL_DECLARE(result, name, arguments) result(B173_GL_CALL *name) arguments = nullptr;
	B173_GL_REQUIRED(B173_GL_DECLARE)
	B173_GL_DECLARE(void, GenVertexArrays, (Size, UInt *))
	B173_GL_DECLARE(void, BindVertexArray, (UInt))
	B173_GL_DECLARE(void, DeleteVertexArrays, (Size, const UInt *))
	B173_GL_DECLARE(void, GetShaderPrecisionFormat, (Enum, Enum, Int *, Int *))
	B173_GL_DECLARE(void, ClearDepth, (double))
	B173_GL_DECLARE(void, ClearDepthf, (float))
#undef B173_GL_DECLARE

	template <class T> static T load(GLProcLoader loader, const char *name)
	{
		const GLProc p = loader(name);
		if (!p)
			throw std::runtime_error(std::string("missing GL entry point: ") + name);
		return reinterpret_cast<T>(p);
	}

	API(GLProcLoader loader, bool es)
	{
		if (!loader)
			throw std::invalid_argument("GL entry-point loader is required");
#define B173_GL_LOAD(result, name, arguments) name = load<decltype(name)>(loader, "gl" #name);
		B173_GL_REQUIRED(B173_GL_LOAD)
#undef B173_GL_LOAD
		if (es)
		{
			GetShaderPrecisionFormat = load<decltype(GetShaderPrecisionFormat)>(loader, "glGetShaderPrecisionFormat");
			ClearDepthf = load<decltype(ClearDepthf)>(loader, "glClearDepthf");
		}
		else
		{
			GenVertexArrays = load<decltype(GenVertexArrays)>(loader, "glGenVertexArrays");
			BindVertexArray = load<decltype(BindVertexArray)>(loader, "glBindVertexArray");
			DeleteVertexArrays = load<decltype(DeleteVertexArrays)>(loader, "glDeleteVertexArrays");
			ClearDepth = load<decltype(ClearDepth)>(loader, "glClearDepth");
		}
	}

	void check(const char *where)
	{
		const Enum e = GetError();
		if (e)
		{
			std::ostringstream s;
			s << where << ": GL error 0x" << std::hex << e;
			throw std::runtime_error(s.str());
		}
	}
};

#undef B173_GL_REQUIRED

static Enum blendFactor(BlendFactor f)
{
	constexpr Enum table[] = {0, 1, 0x0300, 0x0301, 0x0306, 0x0307, 0x0302, 0x0303, 0x0304, 0x0305, 0x0308};
	return table[unsigned(f)];
}

static Enum blendOp(BlendOp op)
{
	constexpr Enum table[] = {0x8006, 0x800A, 0x800B};
	return table[unsigned(op)];
}

static Enum primitive(Primitive p)
{
	return p == Primitive::Triangles ? 4 : p == Primitive::Lines ? 1 : 0;
}

static bool sameRect(Rect a, Rect b)
{
	return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

// ES 2.0 requires every buffer datum to sit at a multiple of its own size, so
// keeping all stream suballocations 4-byte aligned serves floats and both index
// widths at once. Vertices are 28 bytes, which is already a multiple of four.
static std::size_t alignUp4(std::size_t value)
{
	return (value + 3) & ~std::size_t(3);
}

// Upload straight out of the caller's pixels when the rows are already tight.
// Only a strided source needs the repacking copy, and the diagnostics match
// unpackRGBA either way.
static const void *packedPixels(const void *rgba, std::size_t bytes, int width, int height, std::size_t stride,
								std::vector<std::uint8_t> &scratch)
{
	if (stride && stride != std::size_t(width) * 4)
	{
		scratch = unpackRGBA(rgba, bytes, width, height, stride);
		return scratch.data();
	}
	if (!rgba || bytes < rgbaByteCount(width, height))
		throw std::invalid_argument("RGBA source is truncated");
	return rgba;
}
} // namespace GLDetail

using namespace GLDetail;

struct GLDevice::Impl
{
	API gl;
	bool es = false, highp = true, validState = false, attributesEnabled = false;
	bool validateDraws = false;
	unsigned program = 0, vao = 0;
	unsigned boundTexture = 0;
	int vertexUniform = -1, fragmentUniform = -1, width = 0, height = 0;
	std::uint64_t nextMesh = 1, nextTexture = 1;
	PipelineState previous;
	float lineWidths[2]{1, 1};

	struct Part
	{
		unsigned vb = 0, ib = 0, vao = 0;
		int count = 0;
		bool narrow = false;
		std::size_t vertexOffset = 0, indexOffset = 0;
	};

	struct StreamPage
	{
		unsigned vb = 0, ib = 0;
		std::size_t vertexCapacity = 0, indexCapacity = 0, vertexUsed = 0, indexUsed = 0;
	};

	struct Mesh
	{
		MeshData source;
		std::vector<Part> smooth, flat;
	};

	struct Texture
	{
		TextureDesc desc;
		unsigned name = 0;
	};

	std::vector<StreamPage> streams;
	std::unordered_map<MeshId, Mesh> meshes;
	std::unordered_map<TextureId, Texture> textures;
	// Transient draws reuse this storage. A per-draw mesh handle, buffer object or
	// map node would allocate once per GUI quad; the stream pages already exist.
	std::vector<Part> transientParts;
	std::vector<std::uint16_t> narrowIndices;
	MeshData deindexed;

	explicit Impl(const GLCreateInfo &ci)
		: gl(ci.getProc, ci.profile == GLProfile::ES20), es(ci.profile == GLProfile::ES20),
		  validateDraws(ci.validateDraws)
	{
		if (!gl.GetString(VERSION))
			throw std::runtime_error("no current GL context");
		gl.GetFloatv(ALIASED_LINE_WIDTH_RANGE, lineWidths);
		if (es)
		{
			int range[2]{}, precision = 0;
			gl.GetShaderPrecisionFormat(FRAGMENT_SHADER, HIGH_FLOAT, range, &precision);
			// ES 2.0 guarantees mediump, not fragment highp. A zero precision
			// result selects matching mediump fragment inputs and vertex outputs.
			gl.check("query fragment precision");
			highp = precision > 0;
		}
		try
		{
			const ShaderLanguage lang = es ? ShaderLanguage::ES100 : ShaderLanguage::GL330;
			unsigned vs = compile(VERTEX_SHADER, vertexShader(lang, highp)), fs = 0;
			try
			{
				fs = compile(FRAGMENT_SHADER, fragmentShader(lang, highp));
			}
			catch (...)
			{
				gl.DeleteShader(vs);
				throw;
			}
			program = gl.CreateProgram();
			gl.AttachShader(program, vs);
			gl.AttachShader(program, fs);
			// ESSL 100 has no layout qualifiers; these must agree with legacy330.vert.
			gl.BindAttribLocation(program, 0, "aPosition");
			gl.BindAttribLocation(program, 1, "aUV");
			gl.BindAttribLocation(program, 2, "aColor");
			gl.BindAttribLocation(program, 3, "aNormal");
			gl.LinkProgram(program);
			gl.DeleteShader(vs);
			gl.DeleteShader(fs);
			int ok = 0;
			gl.GetProgramiv(program, LINK_STATUS, &ok);
			if (!ok)
			{
				int n = 0;
				gl.GetProgramiv(program, INFO_LOG_LENGTH, &n);
				std::string log(std::size_t(std::max(n, 1)), '\0');
				gl.GetProgramInfoLog(program, n, nullptr, &log[0]);
				throw std::runtime_error("GL program link: " + log);
			}
			gl.UseProgram(program);
			vertexUniform = gl.GetUniformLocation(program, "uV[0]");
			fragmentUniform = gl.GetUniformLocation(program, "uF[0]");
			if (vertexUniform < 0 || fragmentUniform < 0)
				throw std::runtime_error("legacy uniform contract missing");
			gl.Uniform1i(gl.GetUniformLocation(program, "uTexture"), 0);
			if (!es)
			{
				gl.GenVertexArrays(1, &vao);
				gl.BindVertexArray(vao);
			}
			// GL_DITHER keeps its enabled default: the compatibility oracle this
			// backend is compared against never disables it. This module writes
			// linear RGBA8, so sRGB encoding stays off.
			if (!es)
				gl.Disable(FRAMEBUFFER_SRGB); // Not an ES 2.0 core enum.
			TextureDesc white;
			white.width = white.height = 1;
			const std::uint8_t pixel[] = {255, 255, 255, 255};
			textures.emplace(0, uploadTexture(white, pixel));
			gl.check("GL backend initialization");
		}
		catch (...)
		{
			cleanup();
			throw;
		}
	}

	~Impl()
	{
		cleanup();
	}

	void cleanup()
	{
		for (StreamPage &p : streams)
		{
			gl.DeleteBuffers(1, &p.vb);
			gl.DeleteBuffers(1, &p.ib);
		}
		streams.clear();
		for (auto &m : meshes)
		{
			freeParts(m.second.smooth);
			freeParts(m.second.flat);
		}
		meshes.clear();
		for (auto &t : textures)
			gl.DeleteTextures(1, &t.second.name);
		textures.clear();
		if (vao)
		{
			gl.DeleteVertexArrays(1, &vao);
			vao = 0;
		}
		if (program)
		{
			gl.DeleteProgram(program);
			program = 0;
		}
	}

	unsigned compile(Enum stage, const std::string &source)
	{
		const unsigned shader = gl.CreateShader(stage);
		const char *text = source.c_str();
		gl.ShaderSource(shader, 1, &text, nullptr);
		gl.CompileShader(shader);
		int ok = 0;
		gl.GetShaderiv(shader, COMPILE_STATUS, &ok);
		if (!ok)
		{
			int n = 0;
			gl.GetShaderiv(shader, INFO_LOG_LENGTH, &n);
			std::string log(std::size_t(std::max(n, 1)), '\0');
			gl.GetShaderInfoLog(shader, n, nullptr, &log[0]);
			gl.DeleteShader(shader);
			throw std::runtime_error("GL shader compile: " + log);
		}
		return shader;
	}

	void freeParts(std::vector<Part> &parts)
	{
		for (Part &p : parts)
		{
			if (p.vao)
				gl.DeleteVertexArrays(1, &p.vao);
			if (p.vb)
				gl.DeleteBuffers(1, &p.vb);
			if (p.ib)
				gl.DeleteBuffers(1, &p.ib);
		}
		parts.clear();
	}

	void setAttributes(std::size_t offset)
	{
		gl.VertexAttribPointer(0, 3, FLOAT, 0, sizeof(Vertex), reinterpret_cast<const void *>(offset));
		gl.VertexAttribPointer(1, 2, FLOAT, 0, sizeof(Vertex), reinterpret_cast<const void *>(offset + 12));
		gl.VertexAttribPointer(2, 4, UBYTE, 1, sizeof(Vertex), reinterpret_cast<const void *>(offset + 20));
		gl.VertexAttribPointer(3, 4, UBYTE, 1, sizeof(Vertex), reinterpret_cast<const void *>(offset + 24));
	}

	void bindTextureName(unsigned name)
	{
		if (boundTexture == name)
			return;
		gl.BindTexture(TEXTURE_2D, name);
		boundTexture = name;
	}

	Part uploadPart(const std::vector<Vertex> &vertices, const void *indices, std::size_t bytes, int count, bool narrow)
	{
		Part p;
		p.count = count;
		p.narrow = narrow;
		try
		{
			if (!es)
			{
				gl.GenVertexArrays(1, &p.vao);
				gl.BindVertexArray(p.vao);
			}
			gl.GenBuffers(1, &p.vb);
			gl.GenBuffers(1, &p.ib);
			gl.BindBuffer(ARRAY_BUFFER, p.vb);
			gl.BufferData(ARRAY_BUFFER, PtrSize(vertices.size() * sizeof(Vertex)), vertices.data(), STATIC_DRAW);
			gl.BindBuffer(ELEMENT_BUFFER, p.ib);
			gl.BufferData(ELEMENT_BUFFER, PtrSize(bytes), indices, STATIC_DRAW);
			if (!es)
			{
				for (unsigned attribute = 0; attribute < 4; ++attribute)
					gl.EnableVertexAttribArray(attribute);
				setAttributes(0);
			}
			gl.check("mesh upload");
			return p;
		}
		catch (...)
		{
			if (p.vao)
				gl.DeleteVertexArrays(1, &p.vao);
			if (p.vb)
				gl.DeleteBuffers(1, &p.vb);
			if (p.ib)
				gl.DeleteBuffers(1, &p.ib);
			throw;
		}
	}

	void uploadMesh(const MeshData &mesh, std::vector<Part> &parts)
	{
		try
		{
			if (es && mesh.vertices.size() <= 65536)
			{
				narrowIndices.resize(mesh.indices.size());
				for (std::size_t n = 0; n < mesh.indices.size(); ++n)
					narrowIndices[n] = std::uint16_t(mesh.indices[n]);
				parts.push_back(uploadPart(mesh.vertices, narrowIndices.data(), narrowIndices.size() * 2,
										   int(narrowIndices.size()), true));
			}
			else if (es)
			{
				for (const Batch16 &batch : split16(mesh))
					parts.push_back(uploadPart(batch.vertices, batch.indices.data(), batch.indices.size() * 2,
											   int(batch.indices.size()), true));
			}
			else
			{
				parts.push_back(uploadPart(mesh.vertices, mesh.indices.data(), mesh.indices.size() * 4,
										   int(mesh.indices.size()), false));
			}
		}
		catch (...)
		{
			freeParts(parts);
			throw;
		}
	}

	void unpackState()
	{
		gl.PixelStorei(UNPACK_ALIGNMENT, 1);
		if (!es)
		{
			gl.BindBuffer(PIXEL_UNPACK_BUFFER, 0);
			gl.PixelStorei(UNPACK_ROW_LENGTH, 0);
		}
	}

	Texture uploadTexture(const TextureDesc &d, const void *pixels)
	{
		Texture t;
		t.desc = d;
		gl.GenTextures(1, &t.name);
		try
		{
			gl.ActiveTexture(TEXTURE0);
			gl.BindTexture(TEXTURE_2D, t.name);
			boundTexture = t.name;
			unpackState();
			gl.TexParameteri(TEXTURE_2D, MIN_FILTER, d.minFilter == Filter::Linear ? LINEAR : NEAREST);
			gl.TexParameteri(TEXTURE_2D, MAG_FILTER, d.magFilter == Filter::Linear ? LINEAR : NEAREST);
			// ES 2.0 core completeness: an NPOT texture must clamp both axes and
			// never use a mipmap filter. The shader repeats the axes that asked
			// for GL_REPEAT, so no resize and no OES_texture_npot is needed.
			const bool clampBoth = es && ((d.width & (d.width - 1)) || (d.height & (d.height - 1)));
			gl.TexParameteri(TEXTURE_2D, WRAP_S, !clampBoth && d.wrapS == Wrap::Repeat ? REPEAT : EDGE);
			gl.TexParameteri(TEXTURE_2D, WRAP_T, !clampBoth && d.wrapT == Wrap::Repeat ? REPEAT : EDGE);
			gl.TexImage2D(TEXTURE_2D, 0, es ? RGBA : RGBA8, d.width, d.height, 0, RGBA, UBYTE, pixels);
			gl.check("texture upload");
			return t;
		}
		catch (...)
		{
			gl.DeleteTextures(1, &t.name);
			if (boundTexture == t.name)
				boundTexture = 0;
			throw;
		}
	}

	void toggle(Enum cap, bool enabled, bool old)
	{
		if (!validState || enabled != old)
		{
			if (enabled)
				gl.Enable(cap);
			else
				gl.Disable(cap);
		}
	}

	void apply(const PipelineState &p)
	{
		validatePipeline(p);
		const PipelineState &o = previous;
		toggle(DEPTH_TEST, p.depthTest, o.depthTest);
		toggle(BLEND, p.blend, o.blend);
		toggle(CULL_FACE, p.cull != Cull::None, o.cull != Cull::None);
		toggle(SCISSOR, p.scissor, o.scissor);
		toggle(POLYGON_OFFSET, p.polygonOffset, o.polygonOffset);
		if (!validState || p.depthWrite != o.depthWrite)
			gl.DepthMask(Bool(p.depthWrite));
		if (!validState || p.depthCompare != o.depthCompare)
			gl.DepthFunc(COMPARE_BASE + unsigned(p.depthCompare));
		if (!validState || p.colorMask != o.colorMask)
			gl.ColorMask(Bool(p.colorMask & 1), Bool(p.colorMask & 2), Bool(p.colorMask & 4), Bool(p.colorMask & 8));
		if (!validState || p.srcRGB != o.srcRGB || p.dstRGB != o.dstRGB || p.srcAlpha != o.srcAlpha ||
			p.dstAlpha != o.dstAlpha)
			gl.BlendFuncSeparate(blendFactor(p.srcRGB), blendFactor(p.dstRGB), blendFactor(p.srcAlpha),
								 blendFactor(p.dstAlpha));
		if (!validState || p.rgbOp != o.rgbOp || p.alphaOp != o.alphaOp)
			gl.BlendEquationSeparate(blendOp(p.rgbOp), blendOp(p.alphaOp));
		if (!validState || p.cull != o.cull)
			gl.CullFace(p.cull == Cull::Front ? FRONT : p.cull == Cull::Both ? BOTH : BACK);
		if (!validState || p.frontCCW != o.frontCCW)
			gl.FrontFace(p.frontCCW ? CCW : CW);
		if (!validState || p.offsetFactor != o.offsetFactor || p.offsetUnits != o.offsetUnits)
			gl.PolygonOffset(p.offsetFactor, p.offsetUnits);
		if (!validState || p.lineWidth != o.lineWidth)
			gl.LineWidth(p.lineWidth);
		if (!validState || !sameRect(p.viewport, o.viewport))
			gl.Viewport(p.viewport.x, p.viewport.y, p.viewport.width, p.viewport.height);
		if (!validState || !sameRect(p.scissorRect, o.scissorRect))
			gl.Scissor(p.scissorRect.x, p.scissorRect.y, p.scissorRect.width, p.scissorRect.height);
		previous = p;
		validState = true;
	}

	Part streamPart(const std::vector<Vertex> &vertices, const void *indices, std::size_t indexBytes, int count,
					bool narrow)
	{
		if (!es)
			gl.BindVertexArray(vao);
		const std::size_t vertexBytes = vertices.size() * sizeof(Vertex);
		StreamPage *page = nullptr;
		for (StreamPage &candidate : streams)
		{
			if (vertexBytes <= candidate.vertexCapacity - alignUp4(candidate.vertexUsed) &&
				indexBytes <= candidate.indexCapacity - alignUp4(candidate.indexUsed))
			{
				page = &candidate;
				break;
			}
		}
		if (!page)
		{
			StreamPage created;
			created.vertexCapacity = alignUp4(std::max<std::size_t>(4 * 1024 * 1024, vertexBytes));
			created.indexCapacity = alignUp4(std::max<std::size_t>(1024 * 1024, indexBytes));
			gl.GenBuffers(1, &created.vb);
			gl.GenBuffers(1, &created.ib);
			try
			{
				gl.BindBuffer(ARRAY_BUFFER, created.vb);
				gl.BufferData(ARRAY_BUFFER, PtrSize(created.vertexCapacity), nullptr, STREAM_DRAW);
				gl.BindBuffer(ELEMENT_BUFFER, created.ib);
				gl.BufferData(ELEMENT_BUFFER, PtrSize(created.indexCapacity), nullptr, STREAM_DRAW);
				gl.check("allocate GL stream page");
				streams.push_back(created);
			}
			catch (...)
			{
				gl.DeleteBuffers(1, &created.vb);
				gl.DeleteBuffers(1, &created.ib);
				throw;
			}
			page = &streams.back();
		}
		const std::size_t vertexOffset = alignUp4(page->vertexUsed);
		const std::size_t indexOffset = alignUp4(page->indexUsed);
		gl.BindBuffer(ARRAY_BUFFER, page->vb);
		gl.BufferSubData(ARRAY_BUFFER, PtrSize(vertexOffset), PtrSize(vertexBytes), vertices.data());
		gl.BindBuffer(ELEMENT_BUFFER, page->ib);
		gl.BufferSubData(ELEMENT_BUFFER, PtrSize(indexOffset), PtrSize(indexBytes), indices);
		page->vertexUsed = vertexOffset + vertexBytes;
		page->indexUsed = indexOffset + indexBytes;
		if (validateDraws)
			gl.check("upload GL transient mesh");
		Part result;
		result.vb = page->vb;
		result.ib = page->ib;
		result.count = count;
		result.narrow = narrow;
		result.vertexOffset = vertexOffset;
		result.indexOffset = indexOffset;
		return result;
	}

	void streamMesh(const MeshData &mesh, std::vector<Part> &parts)
	{
		if (!es)
		{
			parts.push_back(streamPart(mesh.vertices, mesh.indices.data(), mesh.indices.size() * 4,
									   int(mesh.indices.size()), false));
			return;
		}
		if (mesh.vertices.size() <= 65536)
		{
			// Every index already fits sixteen bits, so narrowing in place beats
			// rebuilding batches: split16 would copy the whole vertex array.
			narrowIndices.resize(mesh.indices.size());
			for (std::size_t n = 0; n < mesh.indices.size(); ++n)
				narrowIndices[n] = std::uint16_t(mesh.indices[n]);
			parts.push_back(streamPart(mesh.vertices, narrowIndices.data(), narrowIndices.size() * 2,
									   int(narrowIndices.size()), true));
			return;
		}
		for (const Batch16 &batch : split16(mesh))
			parts.push_back(streamPart(batch.vertices, batch.indices.data(), batch.indices.size() * 2,
									   int(batch.indices.size()), true));
	}

	// attributes supplies the primitive and the has* shader flags; the parts may
	// live in a static mesh or in this frame's stream pages.
	void drawParts(const std::vector<Part> &parts, const MeshData &attributes, const Draw &d)
	{
		const Texture &texture = textures.at(d.texture);
		if (attributes.primitive == Primitive::Lines &&
			(d.pipeline.lineWidth < lineWidths[0] || d.pipeline.lineWidth > lineWidths[1]))
			throw std::runtime_error("requested line width is outside this GL context's supported range");
		Uniforms u = d.uniforms;
		applyClipConvention(u, ClipConvention::OpenGL);
		applyMeshUniforms(u, attributes);
		applyTextureUniforms(u, texture.desc, es);
		apply(d.pipeline);
		bindTextureName(texture.name);
		gl.Uniform4fv(vertexUniform, 32, &u.v[0].x);
		gl.Uniform4fv(fragmentUniform, 7, &u.f[0].x);
		const Enum mode = GLDetail::primitive(attributes.primitive);
		for (const Part &p : parts)
		{
			if (!es && p.vao)
				gl.BindVertexArray(p.vao);
			else
			{
				if (!es)
					gl.BindVertexArray(vao);
				if (!attributesEnabled)
				{
					for (unsigned attribute = 0; attribute < 4; ++attribute)
						gl.EnableVertexAttribArray(attribute);
					attributesEnabled = true;
				}
				gl.BindBuffer(ARRAY_BUFFER, p.vb);
				gl.BindBuffer(ELEMENT_BUFFER, p.ib);
				setAttributes(p.vertexOffset);
			}
			gl.DrawElements(mode, p.count, p.narrow ? USHORT : UINT, reinterpret_cast<const void *>(p.indexOffset));
		}
		if (validateDraws)
			gl.check("legacy draw");
	}
};

GLDevice::GLDevice(const GLCreateInfo &info)
{
	if (!info.getProc)
		throw std::invalid_argument("GL proc loader is required");
	impl_.reset(new Impl(info));
}

GLDevice::~GLDevice() = default;

void GLDevice::beginFrame(unsigned framebuffer, int width, int height)
{
	if (width <= 0 || height <= 0)
		throw std::invalid_argument("zero-sized framebuffer");
	Impl &i = *impl_;
	i.width = width;
	i.height = height;
	i.gl.BindFramebuffer(FRAMEBUFFER, framebuffer);
	i.gl.UseProgram(i.program);
	i.gl.ActiveTexture(TEXTURE0);
	i.boundTexture = 0;
	if (!i.es)
		i.gl.BindVertexArray(i.vao);
	i.validState = false;
	i.attributesEnabled = false;
	// Orphan every stream page exactly once per presented frame. Calling this
	// twice inside one frame silently invalidates the transient draws already
	// issued; use a separate framebuffer bind for multi-pass work.
	for (Impl::StreamPage &p : i.streams)
	{
		i.gl.BindBuffer(ARRAY_BUFFER, p.vb);
		i.gl.BufferData(ARRAY_BUFFER, PtrSize(p.vertexCapacity), nullptr, STREAM_DRAW);
		i.gl.BindBuffer(ELEMENT_BUFFER, p.ib);
		i.gl.BufferData(ELEMENT_BUFFER, PtrSize(p.indexCapacity), nullptr, STREAM_DRAW);
		p.vertexUsed = p.indexUsed = 0;
	}
}

void GLDevice::invalidateState()
{
	impl_->validState = false;
	impl_->attributesEnabled = false;
	impl_->gl.UseProgram(impl_->program);
	impl_->gl.ActiveTexture(TEXTURE0);
	impl_->boundTexture = 0;
}

std::string GLDevice::description() const
{
	API &g = impl_->gl;
	return std::string(reinterpret_cast<const char *>(g.GetString(VERSION))) + " / " +
		   reinterpret_cast<const char *>(g.GetString(RENDERER));
}

bool GLDevice::fragmentHighp() const
{
	return impl_->highp;
}

void GLDevice::lineWidthRange(float *minimum, float *maximum) const
{
	if (!minimum || !maximum)
		throw std::invalid_argument("line width range needs both outputs");
	*minimum = impl_->lineWidths[0];
	*maximum = impl_->lineWidths[1];
}

MeshId GLDevice::createMesh(const MeshData &source)
{
	validateMesh(source);
	Impl &i = *impl_;
	const MeshId id = i.nextMesh++;
	Impl::Mesh &mesh = i.meshes[id];
	try
	{
		mesh.source = source;
		i.uploadMesh(source, mesh.smooth);
	}
	catch (...)
	{
		i.freeParts(mesh.smooth);
		i.meshes.erase(id);
		throw;
	}
	return id;
}

void GLDevice::destroyMesh(MeshId id)
{
	Impl &i = *impl_;
	auto it = i.meshes.find(id);
	if (it == i.meshes.end())
		throw std::out_of_range("unknown mesh");
	i.freeParts(it->second.smooth);
	i.freeParts(it->second.flat);
	i.meshes.erase(it);
}

TextureId GLDevice::createTexture(const TextureDesc &desc, const void *rgba, std::size_t bytes, std::size_t stride)
{
	Impl &i = *impl_;
	validateTexture(desc, i.es);
	std::vector<std::uint8_t> repacked;
	const void *pixels = packedPixels(rgba, bytes, desc.width, desc.height, stride, repacked);
	Impl::Texture texture = i.uploadTexture(desc, pixels);
	const TextureId id = i.nextTexture++;
	try
	{
		i.textures.emplace(id, texture);
	}
	catch (...)
	{
		i.gl.DeleteTextures(1, &texture.name);
		if (i.boundTexture == texture.name)
			i.boundTexture = 0;
		throw;
	}
	return id;
}

void GLDevice::updateTexture(TextureId id, Rect r, const void *rgba, std::size_t bytes, std::size_t stride)
{
	if (!id)
		throw std::invalid_argument("cannot update the internal white texture");
	Impl &i = *impl_;
	Impl::Texture &t = i.textures.at(id);
	if (r.x < 0 || r.y < 0 || std::int64_t(r.x) + r.width > t.desc.width ||
		std::int64_t(r.y) + r.height > t.desc.height)
		throw std::out_of_range("texture subimage rectangle");
	std::vector<std::uint8_t> repacked;
	const void *pixels = packedPixels(rgba, bytes, r.width, r.height, stride, repacked);
	i.gl.ActiveTexture(TEXTURE0);
	i.bindTextureName(t.name);
	i.unpackState();
	i.gl.TexSubImage2D(TEXTURE_2D, 0, r.x, r.y, r.width, r.height, RGBA, UBYTE, pixels);
	i.gl.check("texture subimage");
}

void GLDevice::destroyTexture(TextureId id)
{
	if (!id)
		throw std::invalid_argument("cannot destroy internal white texture");
	Impl &i = *impl_;
	auto it = i.textures.find(id);
	if (it == i.textures.end())
		throw std::out_of_range("unknown texture");
	i.gl.DeleteTextures(1, &it->second.name);
	if (i.boundTexture == it->second.name)
		i.boundTexture = 0;
	i.textures.erase(it);
}

void GLDevice::draw(const Draw &d)
{
	Impl &i = *impl_;
	if (!i.width)
		throw std::logic_error("beginFrame must precede draw");
	Impl::Mesh &mesh = i.meshes.at(d.mesh);
	std::vector<Impl::Part> &parts = d.pipeline.flat ? mesh.flat : mesh.smooth;
	if (parts.empty())
	{
		// The flat variant is a second, deindexed GPU copy built on first use.
		flatMesh(mesh.source, i.deindexed);
		i.uploadMesh(i.deindexed, parts);
	}
	i.drawParts(parts, mesh.source, d);
}

void GLDevice::drawTransient(const MeshData &input, const Draw &state)
{
	Impl &i = *impl_;
	if (!i.width)
		throw std::logic_error("beginFrame must precede transient draw");
	validateMesh(input);
	const MeshData *source = &input;
	if (state.pipeline.flat)
	{
		flatMesh(input, i.deindexed);
		source = &i.deindexed;
	}
	i.transientParts.clear();
	i.streamMesh(*source, i.transientParts);
	i.drawParts(i.transientParts, *source, state);
}

void GLDevice::clear(Vec4 color, float depth, bool colorBit, bool depthBit, const PipelineState &state)
{
	Impl &i = *impl_;
	i.apply(state);
	i.gl.ClearColor(color.x, color.y, color.z, color.w);
	if (i.es)
		i.gl.ClearDepthf(depth);
	else
		i.gl.ClearDepth(depth);
	i.gl.Clear((colorBit ? COLOR_BIT : 0) | (depthBit ? DEPTH_BIT : 0));
	i.gl.check("clear");
}

std::vector<std::uint8_t> GLDevice::readPixels(Rect r)
{
	Impl &i = *impl_;
	if (r.x < 0 || r.y < 0 || std::int64_t(r.x) + r.width > i.width || std::int64_t(r.y) + r.height > i.height)
		throw std::out_of_range("readback rectangle");
	std::vector<std::uint8_t> pixels(rgbaByteCount(r.width, r.height));
	i.gl.PixelStorei(PACK_ALIGNMENT, 1);
	if (!i.es)
	{
		i.gl.BindBuffer(PIXEL_PACK_BUFFER, 0);
		i.gl.PixelStorei(PACK_ROW_LENGTH, 0);
	}
	i.gl.ReadPixels(r.x, r.y, r.width, r.height, RGBA, UBYTE, pixels.data());
	i.gl.check("readback");
	return pixels;
}
} // namespace render
} // namespace b173

#undef B173_GL_CALL
