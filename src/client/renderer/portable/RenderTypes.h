#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

// B173 - API-neutral render data (portable renderer boundary). No GL, Vulkan or
// Windows header may reach game code through this file.
namespace b173
{
namespace render
{
struct alignas(16) Vec4
{
	float x = 0, y = 0, z = 0, w = 0;
};

static_assert(sizeof(Vec4) == 16, "Vec4 must occupy exactly one std140 float4 slot");

struct Mat4
{
	std::array<float, 16> v{}; // Column-major; vectors are multiplied on the right.
	static Mat4 identity();
	static Mat4 translation(double x, double y, double z);
	static Mat4 scale(double x, double y, double z);
	static Mat4 rotation(double degrees, double x, double y, double z);
	static Mat4 frustum(double l, double r, double b, double t, double n, double f);
	static Mat4 ortho(double l, double r, double b, double t, double n, double f);
};

Mat4 operator*(const Mat4 &a, const Mat4 &b);
Vec4 operator*(const Mat4 &a, const Vec4 &b);
// Rows of the inverse transpose of the upper-left 3x3 block. Component .z of the
// three returned rows is the third row of the plain inverse, which is the vector
// GL_RESCALE_NORMAL is defined against.
std::array<Vec4, 3> inverseTranspose3(const Mat4 &m);

struct Vertex
{
	float x = 0, y = 0, z = 0;
	float u = 0, v = 0;
	std::array<std::uint8_t, 4> rgba{{255, 255, 255, 255}};
	// Two's-complement signed byte representation, decoded explicitly in shaders.
	// Do not bind this as hardware SNORM: GL 2.1 and modern SNORM differ at zero.
	std::array<std::uint8_t, 4> normal{{0, 0, 127, 0}};
};

static_assert(sizeof(Vertex) == 28, "the portable vertex is the game's 28-byte terrain slot");
static_assert(offsetof(Vertex, u) == 12, "position is three floats");
static_assert(offsetof(Vertex, rgba) == 20, "uv is two floats");
static_assert(offsetof(Vertex, normal) == 24, "color is four bytes");

enum class Primitive : std::uint8_t
{
	Triangles,
	Lines,
	Points
};
enum class Compare : std::uint8_t
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};
enum class BlendFactor : std::uint8_t
{
	Zero,
	One,
	SrcColor,
	OneMinusSrcColor,
	DstColor,
	OneMinusDstColor,
	SrcAlpha,
	OneMinusSrcAlpha,
	DstAlpha,
	OneMinusDstAlpha,
	SrcAlphaSaturate
};
enum class BlendOp : std::uint8_t
{
	Add,
	Subtract,
	ReverseSubtract
};
enum class Cull : std::uint8_t
{
	None,
	Back,
	Front,
	Both
};
enum class Wrap : std::uint8_t
{
	Repeat,
	Edge,
	LegacyClamp
};
enum class Filter : std::uint8_t
{
	Nearest,
	Linear
};
enum class Fog : std::uint8_t
{
	None,
	Linear,
	Exp,
	Exp2
};
enum class TextureEnv : std::uint8_t
{
	Modulate,
	Replace,
	Decal,
	Add,
	Blend
};
enum class ClipConvention : std::uint8_t
{
	OpenGL,
	Vulkan,
	Direct3D
};
enum class NormalConversion : std::uint8_t
{
	Legacy21,
	ModernSnorm
};

struct Rect
{
	int x = 0, y = 0, width = 0, height = 0; // Logical GL bottom-left coordinates.
};

Rect topLeftRect(Rect r, int targetHeight);

struct PipelineState
{
	bool depthTest = false;
	bool depthWrite = true;
	Compare depthCompare = Compare::Less;
	bool blend = false;
	BlendFactor srcRGB = BlendFactor::One, dstRGB = BlendFactor::Zero;
	BlendFactor srcAlpha = BlendFactor::One, dstAlpha = BlendFactor::Zero;
	BlendOp rgbOp = BlendOp::Add, alphaOp = BlendOp::Add;
	Cull cull = Cull::None;
	bool frontCCW = true;
	std::uint8_t colorMask = 15; // R=1, G=2, B=4, A=8.
	bool polygonOffset = false;
	float offsetFactor = 0, offsetUnits = 0;
	float lineWidth = 1;
	bool flat = false;
	bool scissor = false;
	Rect viewport{}, scissorRect{};
};

struct TextureDesc
{
	int width = 0, height = 0;
	Filter minFilter = Filter::Nearest, magFilter = Filter::Nearest;
	Wrap wrapS = Wrap::Repeat, wrapT = Wrap::Repeat;
	Vec4 border{0, 0, 0, 0};
	// The pinned game has Textures::MIPMAP=false. This module deliberately accepts
	// one RGBA8 level; no silent mip generation, sRGB conversion, or NPOT resize.
};

// std140/HLSL-compatible, 32 vertex and 8 fragment vec4 slots. Both stages share
// a single native constant buffer; GL uses two ordinary uniform arrays (ES 2.0).
struct alignas(16) Uniforms
{
	std::array<Vec4, 32> v{};
	std::array<Vec4, 8> f{};
};

static_assert(sizeof(Uniforms) == 640, "the shader ABI is 32 vertex plus 8 fragment float4 slots");

using MeshId = std::uint64_t;
using TextureId = std::uint64_t;

struct MeshData
{
	Primitive primitive = Primitive::Triangles;
	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;
	bool hasTexture = true, hasColor = true, hasNormal = true;
};

struct Batch16
{
	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;
};

struct Draw
{
	MeshId mesh = 0;
	TextureId texture = 0; // Zero means the backend's opaque white texture.
	PipelineState pipeline;
	Uniforms uniforms;
};

void validateMesh(const MeshData &mesh);
void validateTexture(const TextureDesc &desc, bool strictES2);
void validatePipeline(const PipelineState &state);
std::size_t rgbaByteCount(int width, int height);
std::vector<std::uint8_t> unpackRGBA(const void *pixels, std::size_t availableBytes, int width, int height,
									 std::size_t rowStride = 0);
std::vector<std::uint8_t> readbackRGBA(const void *pixels, std::size_t availableBytes, int width, int height,
									   std::size_t rowStride, bool topDown, bool bgra);
std::vector<Batch16> split16(const MeshData &mesh);
MeshData flatMesh(const MeshData &mesh);
// Reusing the caller's storage keeps per-draw flat shading allocation-free.
void flatMesh(const MeshData &mesh, MeshData &out);
std::vector<std::uint32_t> quadIndices(std::size_t quads);
MeshData captureMesh(const void *bytes, std::size_t byteCount, std::size_t vertexCount, std::size_t stride,
					 bool indexedQuads, Primitive primitive, bool hasTexture, bool hasColor, bool hasNormal);
// Static pipeline state only, packed explicitly without padding or allocation.
using PipelineKey = std::array<std::uint64_t, 3>;

struct PipelineKeyHash
{
	std::size_t operator()(const PipelineKey &key) const
	{
		const std::hash<std::uint64_t> hash;
		return hash(key[0]) ^ (hash(key[1]) << 1) ^ (hash(key[2]) << 2);
	}
};

PipelineKey pipelineKey(const PipelineState &state, Primitive primitive);
float normalByte(std::uint8_t value, NormalConversion policy);
bool alphaPass(float alpha, Compare comparison, float reference);
} // namespace render
} // namespace b173
