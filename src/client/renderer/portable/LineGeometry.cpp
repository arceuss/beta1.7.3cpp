#include "client/renderer/portable/LineGeometry.h"

#include <cmath>
#include <limits>
#include <utility>

namespace b173
{
namespace render
{
// out = m * (x, y, z, 1). The matrix is column-major and multiplies on the right,
// the same convention the vertex shader applies to aPosition.
static void projectPoint(const Mat4 &m, double x, double y, double z, double *out)
{
	for (int row = 0; row < 4; ++row)
		out[row] = m.v[row] * x + m.v[4 + row] * y + m.v[8 + row] * z + m.v[12 + row];
}

// out = m * p with both in double: the unprojection of an already homogeneous
// clip-space corner, so w is carried instead of assumed to be one.
static void projectHomogeneous(const double *m, const double *p, double *out)
{
	for (int row = 0; row < 4; ++row)
		out[row] = m[row] * p[0] + m[4 + row] * p[1] + m[8 + row] * p[2] + m[12 + row] * p[3];
}

// Gauss-Jordan elimination with partial pivoting, in double, into a column-major
// result. No epsilon is invented for "nearly singular": only an exactly zero (or
// non-finite) pivot fails here, and every unprojected coordinate is range-checked
// afterwards, so a badly conditioned matrix drops segments instead of emitting
// infinities into a vertex buffer.
static bool invertMatrix(const Mat4 &m, double *inverse)
{
	double a[4][8];
	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			a[row][col] = m.v[col * 4 + row];
			a[row][4 + col] = row == col ? 1.0 : 0.0;
		}
	}
	for (int col = 0; col < 4; ++col)
	{
		int pivot = col;
		for (int row = col + 1; row < 4; ++row)
			if (std::abs(a[row][col]) > std::abs(a[pivot][col]))
				pivot = row;
		if (!(std::abs(a[pivot][col]) > 0))
			return false;
		if (pivot != col)
			for (int k = col; k < 8; ++k)
				std::swap(a[col][k], a[pivot][k]);
		const double scale = 1.0 / a[col][col];
		for (int k = col; k < 8; ++k)
			a[col][k] *= scale;
		for (int row = 0; row < 4; ++row)
		{
			if (row == col)
				continue;
			const double factor = a[row][col];
			if (factor == 0)
				continue;
			for (int k = col; k < 8; ++k)
				a[row][k] -= factor * a[col][k];
		}
	}
	for (int row = 0; row < 4; ++row)
		for (int col = 0; col < 4; ++col)
		{
			if (!std::isfinite(a[row][4 + col]))
				return false;
			inverse[col * 4 + row] = a[row][4 + col];
		}
	return true;
}

// One half-space of the homogeneous view volume. The signed distance is linear in
// the segment parameter, so Liang-Barsky tightens the surviving range [t0, t1]
// without ever dividing by w. Returns false once nothing survives.
static bool clipHalfSpace(double d0, double d1, double &t0, double &t1)
{
	const double delta = d1 - d0;
	if (delta == 0)
		return d0 >= 0; // Parallel to the plane: both endpoints share its side.
	const double t = -d0 / delta;
	if (delta > 0)
	{
		if (t > t0)
			t0 = t; // Entering.
	}
	else if (t < t1)
	{
		t1 = t; // Leaving.
	}
	return t0 <= t1;
}

static std::uint8_t mixByte(std::uint8_t a, std::uint8_t b, double t)
{
	const double value = a + (int(b) - int(a)) * t;
	if (value <= 0)
		return 0;
	if (value >= 255)
		return 255;
	return std::uint8_t(value + 0.5);
}

// The normal slot is two's-complement signed, decoded in the shader, so it has to
// be interpolated as signed bytes rather than as the unsigned pattern.
static std::uint8_t mixSignedByte(std::uint8_t a, std::uint8_t b, double t)
{
	const int from = static_cast<std::int8_t>(a), to = static_cast<std::int8_t>(b);
	double value = from + (to - from) * t;
	if (value <= -128)
		value = -128;
	else if (value >= 127)
		value = 127;
	const int rounded = int(value >= 0 ? value + 0.5 : value - 0.5);
	return std::uint8_t(static_cast<std::int8_t>(rounded));
}

// GL clips before it rasterizes, so a clipped endpoint carries the attributes of
// the clip point, interpolated with the same parameter as the position.
static void mixAttributes(const Vertex &a, const Vertex &b, double t, Vertex &out)
{
	out.u = float(a.u + (b.u - a.u) * t);
	out.v = float(a.v + (b.v - a.v) * t);
	for (unsigned i = 0; i < 4; ++i)
	{
		out.rgba[i] = mixByte(a.rgba[i], b.rgba[i], t);
		out.normal[i] = mixSignedByte(a.normal[i], b.normal[i], t);
	}
}

void expandLines(const MeshData &input, const Mat4 &modelProjection, Rect viewport, float width, bool flat,
				 MeshData &output)
{
	if (&input == &output)
		throw std::invalid_argument("line expansion cannot expand a mesh into itself");
	if (input.primitive != Primitive::Lines)
		throw std::invalid_argument("line expansion consumes normalized line pairs");
	if (input.indices.size() % 2)
		throw std::invalid_argument("incomplete line segment");
	if (!(width > 0) || !std::isfinite(width))
		throw std::invalid_argument("line width must be positive");
	if (viewport.width < 0 || viewport.height < 0)
		throw std::invalid_argument("negative viewport size");
	for (std::uint32_t index : input.indices)
		if (index >= input.vertices.size())
			throw std::out_of_range("line index out of bounds");
	const std::size_t segments = input.indices.size() / 2;
	if (segments > std::size_t(std::numeric_limits<std::uint32_t>::max()) / 4 ||
		segments > std::size_t(std::numeric_limits<int>::max()) / 6)
		throw std::length_error("expanded lines exceed draw limits");
	output.primitive = Primitive::Triangles;
	output.hasTexture = input.hasTexture;
	output.hasColor = input.hasColor;
	output.hasNormal = input.hasNormal;
	output.vertices.clear();
	output.indices.clear();
	// An empty viewport rasterizes nothing, so it needs no geometry and no matrix.
	if (segments == 0 || viewport.width == 0 || viewport.height == 0)
		return;
	double inverse[16];
	if (!invertMatrix(modelProjection, inverse))
		throw std::invalid_argument("line expansion needs an invertible model-projection matrix");
	output.vertices.reserve(segments * 4);
	output.indices.reserve(segments * 6);
	const double half = double(width) * 0.5;
	// Normalized-device units per framebuffer pixel, per axis: this is what keeps a
	// non-square viewport from stretching the ribbon.
	const double ndcPerPixelX = 2.0 / viewport.width, ndcPerPixelY = 2.0 / viewport.height;
	// The ribbon reaches half a width past its centerline, plus half a pixel to the
	// nearest pixel center, so pushing the side planes out by exactly that much
	// cannot clip away covered pixels.
	const double padX = 1.0 + (half + 0.5) * ndcPerPixelX;
	const double padY = 1.0 + (half + 0.5) * ndcPerPixelY;
	const double coordinateLimit = double(std::numeric_limits<float>::max());
	for (std::size_t segment = 0; segment < segments; ++segment)
	{
		const Vertex &first = input.vertices[input.indices[segment * 2]];
		const Vertex &second = input.vertices[input.indices[segment * 2 + 1]];
		double p0[4], p1[4];
		projectPoint(modelProjection, first.x, first.y, first.z, p0);
		projectPoint(modelProjection, second.x, second.y, second.z, p1);
		double t0 = 0, t1 = 1;
		const double planes[6][2] = {{p0[2] + p0[3], p1[2] + p1[3]}, // Near: z >= -w.
									 {p0[3] - p0[2], p1[3] - p1[2]}, // Far: z <= w.
									 {p0[0] + padX * p0[3], p1[0] + padX * p1[3]},
									 {padX * p0[3] - p0[0], padX * p1[3] - p1[0]},
									 {p0[1] + padY * p0[3], p1[1] + padY * p1[3]},
									 {padY * p0[3] - p0[1], padY * p1[3] - p1[1]}};
		bool visible = true;
		for (const auto &plane : planes)
		{
			if (!clipHalfSpace(plane[0], plane[1], t0, t1))
			{
				visible = false;
				break;
			}
		}
		if (!visible)
			continue;
		double c0[4], c1[4];
		for (int k = 0; k < 4; ++k)
		{
			c0[k] = p0[k] + (p1[k] - p0[k]) * t0;
			c1[k] = p0[k] + (p1[k] - p0[k]) * t1;
		}
		// Clipping to both z planes leaves w >= 0; the remaining degenerate case, and
		// any non-finite input coordinate, is rejected here rather than divided by.
		if (!(c0[3] > 0) || !(c1[3] > 0))
			continue;
		const double x0 = c0[0] / c0[3], y0 = c0[1] / c0[3];
		const double x1 = c1[0] / c1[3], y1 = c1[1] / c1[3];
		// Direction in framebuffer pixels, so the perpendicular is perpendicular on
		// screen rather than in a squashed normalized-device square.
		const double dx = (x1 - x0) / ndcPerPixelX, dy = (y1 - y0) / ndcPerPixelY;
		const double length = std::sqrt(dx * dx + dy * dy);
		if (!(length > 0))
			continue; // Zero window length: an aliased GL line has no fragments either.
		// Rotating the direction by +90 degrees makes the corner order below
		// counter-clockwise in window space for every segment orientation.
		const double offsetX = -dy / length * half, offsetY = dx / length * half;
		Vertex ends[2] = {first, second};
		if (t0 > 0)
			mixAttributes(first, second, t0, ends[0]);
		if (t1 < 1)
			mixAttributes(first, second, t1, ends[1]);
		if (flat)
		{
			// GL's provoking vertex for a line is its second vertex, and clipping does
			// not move a flat attribute the way it moves an interpolated one.
			for (Vertex &end : ends)
			{
				end.rgba = second.rgba;
				end.normal = second.normal;
			}
		}
		const double *corners[4] = {c0, c1, c1, c0};
		const double signs[4] = {-1, -1, 1, 1};
		Vertex quad[4];
		bool emit = true;
		for (int corner = 0; corner < 4 && emit; ++corner)
		{
			const double *clip = corners[corner];
			const double sign = signs[corner];
			// Only x and y move, and each by the endpoint's own w: the pixel width is
			// therefore distance-invariant while z and w, and so this endpoint's depth,
			// are exactly the line's.
			const double offset[4] = {clip[0] + sign * offsetX * ndcPerPixelX * clip[3],
									  clip[1] + sign * offsetY * ndcPerPixelY * clip[3], clip[2], clip[3]};
			double object[4];
			projectHomogeneous(inverse, offset, object);
			if (!(std::abs(object[3]) > 0))
			{
				emit = false;
				break;
			}
			const double scale = 1.0 / object[3];
			const double x = object[0] * scale, y = object[1] * scale, z = object[2] * scale;
			if (!(std::abs(x) <= coordinateLimit && std::abs(y) <= coordinateLimit && std::abs(z) <= coordinateLimit))
			{
				emit = false;
				break;
			}
			Vertex &vertex = quad[corner];
			vertex = ends[corner == 1 || corner == 2 ? 1 : 0];
			vertex.x = float(x);
			vertex.y = float(y);
			vertex.z = float(z);
		}
		if (!emit)
			continue;
		const std::uint32_t base = std::uint32_t(output.vertices.size());
		for (const Vertex &vertex : quad)
			output.vertices.push_back(vertex);
		// The tessellator's quad diagonal; both triangles inherit the quad's winding.
		output.indices.insert(output.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
	}
}
} // namespace render
} // namespace b173
