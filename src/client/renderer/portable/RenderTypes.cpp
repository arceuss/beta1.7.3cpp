#include "client/renderer/portable/RenderTypes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace b173
{
namespace render
{
Mat4 Mat4::identity()
{
	Mat4 m;
	m.v[0] = m.v[5] = m.v[10] = m.v[15] = 1;
	return m;
}

Mat4 Mat4::translation(double x, double y, double z)
{
	Mat4 m = identity();
	m.v[12] = float(x);
	m.v[13] = float(y);
	m.v[14] = float(z);
	return m;
}

Mat4 Mat4::scale(double x, double y, double z)
{
	Mat4 m;
	m.v[0] = float(x);
	m.v[5] = float(y);
	m.v[10] = float(z);
	m.v[15] = 1;
	return m;
}

Mat4 Mat4::rotation(double degrees, double x, double y, double z)
{
	const double length = std::sqrt(x * x + y * y + z * z);
	if (!(length > 0))
		return identity();
	x /= length;
	y /= length;
	z /= length;
	const double angle = degrees * 0.017453292519943295769;
	const double c = std::cos(angle), s = std::sin(angle), t = 1 - c;
	Mat4 m = identity();
	m.v[0] = float(t * x * x + c);
	m.v[4] = float(t * x * y - s * z);
	m.v[8] = float(t * x * z + s * y);
	m.v[1] = float(t * x * y + s * z);
	m.v[5] = float(t * y * y + c);
	m.v[9] = float(t * y * z - s * x);
	m.v[2] = float(t * x * z - s * y);
	m.v[6] = float(t * y * z + s * x);
	m.v[10] = float(t * z * z + c);
	return m;
}

Mat4 Mat4::frustum(double l, double r, double b, double t, double n, double f)
{
	if (l == r || b == t || n == f || n <= 0 || f <= 0)
		throw std::invalid_argument("invalid frustum");
	Mat4 m;
	m.v[0] = float(2 * n / (r - l));
	m.v[5] = float(2 * n / (t - b));
	m.v[8] = float((r + l) / (r - l));
	m.v[9] = float((t + b) / (t - b));
	m.v[10] = float(-(f + n) / (f - n));
	m.v[11] = -1;
	m.v[14] = float(-2 * f * n / (f - n));
	return m;
}

Mat4 Mat4::ortho(double l, double r, double b, double t, double n, double f)
{
	if (l == r || b == t || n == f)
		throw std::invalid_argument("invalid ortho");
	Mat4 m = identity();
	m.v[0] = float(2 / (r - l));
	m.v[5] = float(2 / (t - b));
	m.v[10] = float(-2 / (f - n));
	m.v[12] = float(-(r + l) / (r - l));
	m.v[13] = float(-(t + b) / (t - b));
	m.v[14] = float(-(f + n) / (f - n));
	return m;
}

Mat4 operator*(const Mat4 &a, const Mat4 &b)
{
	Mat4 m;
	for (int col = 0; col < 4; ++col)
		for (int row = 0; row < 4; ++row)
			for (int k = 0; k < 4; ++k)
				m.v[col * 4 + row] += a.v[k * 4 + row] * b.v[col * 4 + k];
	return m;
}

Vec4 operator*(const Mat4 &a, const Vec4 &b)
{
	return {a.v[0] * b.x + a.v[4] * b.y + a.v[8] * b.z + a.v[12] * b.w,
			a.v[1] * b.x + a.v[5] * b.y + a.v[9] * b.z + a.v[13] * b.w,
			a.v[2] * b.x + a.v[6] * b.y + a.v[10] * b.z + a.v[14] * b.w,
			a.v[3] * b.x + a.v[7] * b.y + a.v[11] * b.z + a.v[15] * b.w};
}

std::array<Vec4, 3> inverseTranspose3(const Mat4 &m)
{
	const double a = m.v[0], b = m.v[4], c = m.v[8], d = m.v[1], e = m.v[5], f = m.v[9];
	const double g = m.v[2], h = m.v[6], i = m.v[10];
	const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
	if (!std::isfinite(det) || det == 0)
		throw std::domain_error("singular normal matrix");
	const double r = 1 / det;
	// Rows of inverse transpose (cofactor matrix / determinant).
	return {{{float((e * i - f * h) * r), float((f * g - d * i) * r), float((d * h - e * g) * r), 0},
			 {float((c * h - b * i) * r), float((a * i - c * g) * r), float((b * g - a * h) * r), 0},
			 {float((b * f - c * e) * r), float((c * d - a * f) * r), float((a * e - b * d) * r), 0}}};
}

Rect topLeftRect(Rect r, int targetHeight)
{
	const std::int64_t y = std::int64_t(targetHeight) - r.y - r.height;
	if (r.width < 0 || r.height < 0 || y < std::numeric_limits<int>::min() || y > std::numeric_limits<int>::max())
		throw std::invalid_argument("invalid viewport/scissor rectangle");
	r.y = static_cast<int>(y);
	return r;
}

std::size_t rgbaByteCount(int width, int height)
{
	if (width <= 0 || height <= 0)
		throw std::invalid_argument("texture dimensions must be positive");
	const std::size_t w = static_cast<std::size_t>(width), h = static_cast<std::size_t>(height);
	if (w > std::numeric_limits<std::size_t>::max() / 4 / h)
		throw std::overflow_error("RGBA size overflow");
	return w * h * 4;
}

std::vector<std::uint8_t> unpackRGBA(const void *pixels, std::size_t size, int w, int h, std::size_t stride)
{
	const std::size_t bytes = rgbaByteCount(w, h), row = static_cast<std::size_t>(w) * 4;
	if (!stride)
		stride = row;
	if (stride < row || std::size_t(h - 1) > (std::numeric_limits<std::size_t>::max() - row) / stride)
		throw std::invalid_argument("invalid RGBA pitch");
	const std::size_t required = stride * std::size_t(h - 1) + row;
	if (!pixels || size < required)
		throw std::invalid_argument("RGBA source is truncated");
	std::vector<std::uint8_t> out(bytes);
	for (int y = 0; y < h; ++y)
		std::memcpy(out.data() + std::size_t(y) * row,
					static_cast<const std::uint8_t *>(pixels) + std::size_t(y) * stride, row);
	return out;
}

std::vector<std::uint8_t> readbackRGBA(const void *pixels, std::size_t size, int w, int h, std::size_t stride,
									   bool topDown, bool bgra)
{
	std::vector<std::uint8_t> out = unpackRGBA(pixels, size, w, h, stride);
	const std::size_t row = std::size_t(w) * 4;
	if (topDown)
	{
		for (int y = 0; y < h / 2; ++y)
			for (std::size_t x = 0; x < row; ++x)
				std::swap(out[std::size_t(y) * row + x], out[std::size_t(h - 1 - y) * row + x]);
	}
	if (bgra)
	{
		for (std::size_t i = 0; i < out.size(); i += 4)
			std::swap(out[i], out[i + 2]);
	}
	return out;
}

// Vertices per primitive of the three API-neutral topologies.
static std::size_t primitiveStep(Primitive primitive)
{
	return primitive == Primitive::Triangles ? 3 : primitive == Primitive::Lines ? 2 : 1;
}

void validateMesh(const MeshData &m)
{
	if (static_cast<unsigned>(m.primitive) > static_cast<unsigned>(Primitive::Points))
		throw std::invalid_argument("invalid primitive enum");
	const std::size_t step = primitiveStep(m.primitive);
	if (m.vertices.empty() || m.indices.empty() || m.indices.size() % step)
		throw std::invalid_argument("empty/incomplete mesh primitive");
	if (m.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
		m.indices.size() > std::size_t(std::numeric_limits<int>::max()))
		throw std::length_error("mesh exceeds draw limits");
	for (std::uint32_t i : m.indices)
		if (i >= m.vertices.size())
			throw std::out_of_range("mesh index out of bounds");
}

void validateTexture(const TextureDesc &d, bool es)
{
	if (static_cast<unsigned>(d.minFilter) > static_cast<unsigned>(Filter::Linear) ||
		static_cast<unsigned>(d.magFilter) > static_cast<unsigned>(Filter::Linear) ||
		static_cast<unsigned>(d.wrapS) > static_cast<unsigned>(Wrap::LegacyClamp) ||
		static_cast<unsigned>(d.wrapT) > static_cast<unsigned>(Wrap::LegacyClamp))
		throw std::invalid_argument("invalid texture sampling enum");
	(void)rgbaByteCount(d.width, d.height);
	const bool npot = (d.width & (d.width - 1)) || (d.height & (d.height - 1));
	// ES 2.0 core cannot sample an NPOT texture with GL_REPEAT, so that backend
	// clamps the sampler and repeats in the shader instead of resizing. Both that
	// path and the legacy-clamp border term key off one min==mag filter flag.
	const bool emulated = (es && npot && (d.wrapS == Wrap::Repeat || d.wrapT == Wrap::Repeat)) ||
						  d.wrapS == Wrap::LegacyClamp || d.wrapT == Wrap::LegacyClamp;
	if (emulated && d.minFilter != d.magFilter)
		throw std::invalid_argument("emulated wrapping requires matching min/mag filters in this backend revision");
}

void validatePipeline(const PipelineState &p)
{
	const unsigned factorLimit = static_cast<unsigned>(BlendFactor::SrcAlphaSaturate);
	const unsigned opLimit = static_cast<unsigned>(BlendOp::ReverseSubtract);
	if (static_cast<unsigned>(p.depthCompare) > static_cast<unsigned>(Compare::Always) ||
		static_cast<unsigned>(p.cull) > static_cast<unsigned>(Cull::Both) ||
		static_cast<unsigned>(p.srcRGB) > factorLimit || static_cast<unsigned>(p.dstRGB) > factorLimit ||
		static_cast<unsigned>(p.srcAlpha) > factorLimit || static_cast<unsigned>(p.dstAlpha) > factorLimit ||
		static_cast<unsigned>(p.rgbOp) > opLimit || static_cast<unsigned>(p.alphaOp) > opLimit)
		throw std::invalid_argument("invalid pipeline enum");
	if (p.viewport.width < 0 || p.viewport.height < 0 || p.scissorRect.width < 0 || p.scissorRect.height < 0)
		throw std::invalid_argument("negative viewport/scissor size");
	if (!std::isfinite(p.offsetFactor) || !std::isfinite(p.offsetUnits) || !std::isfinite(p.lineWidth) ||
		p.lineWidth <= 0)
		throw std::invalid_argument("non-finite pipeline scalar");
	if (p.colorMask > 15)
		throw std::invalid_argument("invalid color mask");
	if (p.dstRGB == BlendFactor::SrcAlphaSaturate || p.dstAlpha == BlendFactor::SrcAlphaSaturate)
		throw std::invalid_argument("SRC_ALPHA_SATURATE is not a destination factor");
}

std::vector<std::uint32_t> quadIndices(std::size_t quads)
{
	if (quads > std::size_t(std::numeric_limits<std::uint32_t>::max()) / 4 ||
		quads > std::size_t(std::numeric_limits<int>::max()) / 6)
		throw std::length_error("too many quads");
	std::vector<std::uint32_t> out;
	out.reserve(quads * 6);
	for (std::uint32_t q = 0; q < quads; ++q)
	{
		// The tessellator's quad diagonal, byte for byte: 0,1,2, 0,2,3.
		const std::uint32_t b = q * 4;
		out.insert(out.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
	}
	return out;
}

MeshData captureMesh(const void *bytes, std::size_t size, std::size_t count, std::size_t stride, bool quads,
					 Primitive mode, bool uv, bool color, bool normal)
{
	if ((stride != 28 && stride != 32) || count > std::size_t(std::numeric_limits<int>::max()) ||
		count > std::numeric_limits<std::size_t>::max() / stride || size < count * stride || (!bytes && count))
		throw std::invalid_argument("invalid tessellator capture");
	if (quads && count % 4)
		throw std::invalid_argument("incomplete captured quad");
	MeshData m;
	m.primitive = quads ? Primitive::Triangles : mode;
	m.hasTexture = uv;
	m.hasColor = color;
	m.hasNormal = normal;
	m.vertices.resize(count);
	const std::uint8_t *src = static_cast<const std::uint8_t *>(bytes);
	for (std::size_t i = 0; i < count; ++i)
	{
		Vertex &v = m.vertices[i];
		std::memcpy(&v.x, src + i * stride, 12);
		if (uv)
			std::memcpy(&v.u, src + i * stride + 12, 8);
		if (color)
			std::memcpy(v.rgba.data(), src + i * stride + 20, 4);
		if (normal)
			std::memcpy(v.normal.data(), src + i * stride + 24, 4);
	}
	if (quads)
	{
		m.indices = quadIndices(count / 4);
	}
	else
	{
		m.indices.resize(count);
		for (std::size_t i = 0; i < count; ++i)
			m.indices[i] = std::uint32_t(i);
	}
	validateMesh(m);
	return m;
}

void flatMesh(const MeshData &m, MeshData &out)
{
	validateMesh(m);
	out.primitive = m.primitive;
	out.hasTexture = m.hasTexture;
	out.hasColor = m.hasColor;
	out.hasNormal = m.hasNormal;
	out.vertices.clear();
	out.indices.clear();
	out.vertices.reserve(m.indices.size());
	out.indices.reserve(m.indices.size());
	const std::size_t step = primitiveStep(m.primitive);
	for (std::size_t first = 0; first < m.indices.size(); first += step)
	{
		// GL's last-vertex provoking convention drives every flat attribute.
		const Vertex &last = m.vertices[m.indices[first + step - 1]];
		for (std::size_t j = 0; j < step; ++j)
		{
			Vertex v = m.vertices[m.indices[first + j]];
			v.rgba = last.rgba;
			v.normal = last.normal;
			out.indices.push_back(std::uint32_t(out.vertices.size()));
			out.vertices.push_back(v);
		}
	}
}

MeshData flatMesh(const MeshData &m)
{
	MeshData out;
	flatMesh(m, out);
	return out;
}

std::vector<Batch16> split16(const MeshData &m)
{
	validateMesh(m);
	std::vector<Batch16> out(1);
	std::unordered_map<std::uint32_t, std::uint16_t> map;
	const std::size_t step = primitiveStep(m.primitive);
	for (std::size_t first = 0; first < m.indices.size(); first += step)
	{
		std::size_t needed = 0;
		for (std::size_t j = 0; j < step; ++j)
		{
			const std::uint32_t index = m.indices[first + j];
			bool duplicate = false;
			for (std::size_t k = 0; k < j; ++k)
				duplicate |= m.indices[first + k] == index;
			if (!duplicate && map.find(index) == map.end())
				++needed;
		}
		if (out.back().vertices.size() + needed > 65536)
		{
			out.emplace_back();
			map.clear();
		}
		Batch16 &batch = out.back();
		for (std::size_t j = 0; j < step; ++j)
		{
			const std::uint32_t index = m.indices[first + j];
			auto it = map.find(index);
			if (it == map.end())
			{
				const std::uint16_t local = std::uint16_t(batch.vertices.size());
				batch.vertices.push_back(m.vertices[index]);
				it = map.emplace(index, local).first;
			}
			batch.indices.push_back(it->second);
		}
	}
	return out;
}

PipelineKey pipelineKey(const PipelineState &p, Primitive primitive)
{
	validatePipeline(p);
	std::uint64_t state = 0;
	unsigned shift = 0;
	const auto append = [&](std::uint64_t value, unsigned width) {
		state |= value << shift;
		shift += width;
	};
	append(unsigned(primitive), 2);
	append(p.depthTest, 1);
	append(p.depthWrite, 1);
	append(unsigned(p.depthCompare), 3);
	append(p.blend, 1);
	append(unsigned(p.srcRGB), 4);
	append(unsigned(p.dstRGB), 4);
	append(unsigned(p.srcAlpha), 4);
	append(unsigned(p.dstAlpha), 4);
	append(unsigned(p.rgbOp), 2);
	append(unsigned(p.alphaOp), 2);
	append(unsigned(p.cull), 2);
	append(p.frontCCW, 1);
	append(p.colorMask, 4);
	append(p.polygonOffset, 1);
	std::uint32_t factor = 0, units = 0, lineWidth = 0;
	if (p.polygonOffset)
	{
		const float f = p.offsetFactor == 0 ? 0 : p.offsetFactor;
		const float u = p.offsetUnits == 0 ? 0 : p.offsetUnits;
		std::memcpy(&factor, &f, sizeof(factor));
		std::memcpy(&units, &u, sizeof(units));
	}
	if (primitive == Primitive::Lines)
		std::memcpy(&lineWidth, &p.lineWidth, sizeof(lineWidth));
	// Flat attributes are already baked into the mesh. Line width does not
	// affect triangles, including the expanded selection outline.
	return {{state, std::uint64_t(factor) | (std::uint64_t(units) << 32), lineWidth}};
}

float normalByte(std::uint8_t b, NormalConversion policy)
{
	const int n = b < 128 ? int(b) : int(b) - 256;
	return policy == NormalConversion::Legacy21 ? (2.0f * n + 1.0f) / 255.0f : std::max(n / 127.0f, -1.0f);
}

bool alphaPass(float a, Compare c, float r)
{
	switch (c)
	{
	case Compare::Never:
		return false;
	case Compare::Less:
		return a < r;
	case Compare::Equal:
		return a == r;
	case Compare::LessEqual:
		return a <= r;
	case Compare::Greater:
		return a > r;
	case Compare::NotEqual:
		return a != r;
	case Compare::GreaterEqual:
		return a >= r;
	case Compare::Always:
		return true;
	}
	throw std::invalid_argument("invalid alpha comparison");
}
} // namespace render
} // namespace b173
