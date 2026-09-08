#include "client/renderer/portable/LineGeometry.h"
#include "client/renderer/portable/Topology.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace b173::render;

static void check(bool value, const char *message)
{
	if (!value)
		throw std::runtime_error(message);
}

static bool near(double a, double b, double tolerance = 1e-5)
{
	return std::abs(a - b) <= tolerance;
}

// Window position and normalized-device depth, computed in double so these
// assertions measure the expanded geometry instead of the test's own rounding.
struct WindowPoint
{
	double x = 0, y = 0, depth = 0;
};

static WindowPoint windowPoint(const Mat4 &m, Rect viewport, double x, double y, double z)
{
	double clip[4];
	for (int row = 0; row < 4; ++row)
		clip[row] = m.v[row] * x + m.v[4 + row] * y + m.v[8 + row] * z + m.v[12 + row];
	check(clip[3] > 0, "expanded geometry landed behind the eye plane");
	WindowPoint p;
	p.x = (clip[0] / clip[3] * 0.5 + 0.5) * viewport.width + viewport.x;
	p.y = (clip[1] / clip[3] * 0.5 + 0.5) * viewport.height + viewport.y;
	p.depth = clip[2] / clip[3];
	return p;
}

static WindowPoint windowPoint(const Mat4 &m, Rect viewport, const Vertex &v)
{
	return windowPoint(m, viewport, v.x, v.y, v.z);
}

// Everything one expanded quad claims about itself, in framebuffer pixels.
struct Ribbon
{
	double width0 = 0, width1 = 0, area = 0, length = 0, skew = 0;
	double depth[4] = {0, 0, 0, 0};
	WindowPoint start, end; // Centers of the two ends: the line it was built around.
};

static Ribbon measure(const MeshData &mesh, std::size_t quad, const Mat4 &m, Rect viewport)
{
	check(mesh.vertices.size() >= (quad + 1) * 4, "expected another expanded segment");
	WindowPoint c[4];
	for (int i = 0; i < 4; ++i)
		c[i] = windowPoint(m, viewport, mesh.vertices[quad * 4 + i]);
	Ribbon r;
	r.width0 = std::hypot(c[0].x - c[3].x, c[0].y - c[3].y);
	r.width1 = std::hypot(c[1].x - c[2].x, c[1].y - c[2].y);
	for (int i = 0; i < 4; ++i)
	{
		const WindowPoint &next = c[(i + 1) % 4];
		r.area += c[i].x * next.y - next.x * c[i].y;
		r.depth[i] = c[i].depth;
	}
	r.area *= 0.5;
	r.start.x = (c[0].x + c[3].x) * 0.5;
	r.start.y = (c[0].y + c[3].y) * 0.5;
	r.start.depth = (c[0].depth + c[3].depth) * 0.5;
	r.end.x = (c[1].x + c[2].x) * 0.5;
	r.end.y = (c[1].y + c[2].y) * 0.5;
	r.end.depth = (c[1].depth + c[2].depth) * 0.5;
	r.length = std::hypot(r.end.x - r.start.x, r.end.y - r.start.y);
	const double along = r.length > 0 ? r.length : 1;
	r.skew = ((c[3].x - c[0].x) * (r.end.x - r.start.x) + (c[3].y - c[0].y) * (r.end.y - r.start.y)) / along;
	return r;
}

static Vertex point(double x, double y, double z)
{
	Vertex v;
	v.x = float(x);
	v.y = float(y);
	v.z = float(z);
	return v;
}

static MeshData lineMesh(const std::vector<Vertex> &vertices)
{
	MeshData mesh;
	mesh.primitive = Primitive::Lines;
	mesh.vertices = vertices;
	mesh.indices.resize(vertices.size());
	for (std::uint32_t i = 0; i < mesh.indices.size(); ++i)
		mesh.indices[i] = i;
	return mesh;
}

template <typename Call> static bool rejects(Call call)
{
	try
	{
		call();
	}
	catch (const std::exception &)
	{
		return true;
	}
	return false;
}

int main()
try
{
	const Rect viewport{0, 0, 854, 480};
	const double aspect = 480.0 / 854.0;
	const Mat4 projection = Mat4::frustum(-0.1, 0.1, -0.1 * aspect, 0.1 * aspect, 0.1, 1000.0);
	MeshData out;

	// A framebuffer-pixel width is not a world-space thickness: the same segment
	// must measure the requested number of pixels one block away and five hundred
	// blocks away, and the ribbon must stay centered on, and perpendicular to, the
	// line it replaces.
	for (double distance : {1.0, 5.0, 50.0, 500.0})
	{
		for (float width : {1.0f, 2.0f, 3.7f})
		{
			// The far endpoint slopes away from the near one, so "keeps its endpoint's
			// depth" cannot be satisfied by a constant.
			MeshData mesh = lineMesh({point(-0.5, 0.3, -distance), point(0.5, -0.2, -distance * 1.3)});
			expandLines(mesh, projection, viewport, width, false, out);
			check(out.primitive == Primitive::Triangles, "expansion must emit triangles");
			check(out.vertices.size() == 4 && out.indices.size() == 6, "one segment must expand to one quad");
			const Ribbon r = measure(out, 0, projection, viewport);
			check(near(r.width0, width, 1e-3) && near(r.width1, width, 1e-3),
				  "the ribbon lost the requested framebuffer-pixel width");
			check(near(r.skew, 0, 1e-3), "the offset must be perpendicular in window space");
			check(near(r.area, r.length * width, 1e-3 * (r.length + 1.0)),
				  "the ribbon must be a parallelogram exactly one width across");
			check(r.area > 0, "the expanded quad must wind counter-clockwise in window space");
			const WindowPoint a = windowPoint(projection, viewport, mesh.vertices[0]);
			const WindowPoint b = windowPoint(projection, viewport, mesh.vertices[1]);
			check(near(r.start.x, a.x, 1e-3) && near(r.start.y, a.y, 1e-3) && near(r.end.x, b.x, 1e-3) &&
					  near(r.end.y, b.y, 1e-3),
				  "the ribbon must stay centered on the original line");
			check(near(r.depth[0], r.depth[3], 1e-9) && near(r.depth[1], r.depth[2], 1e-9),
				  "both corners of an endpoint must share that endpoint's depth");
			check(near(r.depth[0], a.depth, 1e-6) && near(r.depth[1], b.depth, 1e-6),
				  "expansion must preserve each endpoint's depth exactly");
			check(!near(r.depth[0], r.depth[1], 1e-6), "the test segment must actually slope in depth");
		}
	}

	// Near-plane crossing. The modelview is identity here, so object space is eye
	// space and the surviving endpoint must land on z = -near itself, with every
	// attribute interpolated at that clip point.
	{
		Vertex behind = point(0.02, 0.01, 1.0), ahead = point(0.05, 0.02, -10.0);
		behind.rgba = {{200, 0, 0, 255}};
		ahead.rgba = {{0, 0, 100, 255}};
		behind.u = 0;
		ahead.u = 1;
		behind.normal = {{100, 0, 127, 0}};
		ahead.normal = {{0xF6, 0, 127, 0}}; // -10 as a two's-complement byte.
		const MeshData mesh = lineMesh({behind, ahead});
		expandLines(mesh, projection, viewport, 4.0f, false, out);
		check(out.vertices.size() == 4, "a segment crossing the near plane must survive clipped");
		check(near(out.vertices[0].z, -0.1, 1e-6) && near(out.vertices[3].z, -0.1, 1e-6),
			  "the clipped end must land exactly on the near plane");
		check(near((out.vertices[0].x + out.vertices[3].x) * 0.5, 0.023, 1e-6) &&
				  near((out.vertices[0].y + out.vertices[3].y) * 0.5, 0.011, 1e-6),
			  "the clipped end must sit at the near-plane crossing of the line");
		check(near((out.vertices[1].z + out.vertices[2].z) * 0.5, -10.0, 1e-4) &&
				  near((out.vertices[1].x + out.vertices[2].x) * 0.5, 0.05, 1e-5),
			  "the end inside the frustum must not move");
		const Ribbon r = measure(out, 0, projection, viewport);
		check(near(r.depth[0], -1.0, 1e-6) && near(r.depth[3], -1.0, 1e-6),
			  "the clipped end must sit at the near clip depth");
		check(near(r.width0, 4.0, 1e-3) && near(r.width1, 4.0, 1e-3), "clipping must not disturb the pixel width");
		check(r.area > 0, "a clipped segment must keep the canonical winding");
		const std::array<std::uint8_t, 4> clipped{{180, 0, 10, 255}};
		check(out.vertices[0].rgba == clipped && out.vertices[3].rgba == clipped,
			  "smooth shading must interpolate the color at the clip point");
		check(out.vertices[0].normal[0] == 89,
			  "the normal slot must interpolate as signed bytes, not as its unsigned pattern");
		check(near(out.vertices[0].u, 0.1, 1e-5), "texture coordinates must interpolate at the clip point");
		check(out.vertices[1].rgba == ahead.rgba && out.vertices[2].rgba == ahead.rgba,
			  "the unclipped end must keep its own color");

		// Flat shading takes the provoking (second) vertex of the original line, not
		// the color the clip point interpolated to.
		expandLines(mesh, projection, viewport, 4.0f, true, out);
		check(out.vertices.size() == 4, "flat expansion must emit the same quad");
		for (const Vertex &v : out.vertices)
			check(v.rgba == ahead.rgba && v.normal == ahead.normal,
				  "the flat provoking color and normal must survive clipping");
		check(near(out.vertices[0].u, 0.1, 1e-5), "flat shading must not flatten texture coordinates");
	}

	// Degenerate and invisible segments produce no fragments in GL and no triangles
	// here, and a dropped segment must not shift the ones around it.
	{
		MeshData mesh = lineMesh({point(-0.3, 0.2, 1.0), point(0.4, -0.1, 5.0)});
		expandLines(mesh, projection, viewport, 2.0f, false, out);
		check(out.vertices.empty() && out.indices.empty(), "a segment fully behind the eye must vanish");
		// Fully inside the frustum, so only the zero length can drop it.
		mesh = lineMesh({point(0.3, 0.2, -6), point(0.3, 0.2, -6)});
		expandLines(mesh, projection, viewport, 2.0f, false, out);
		check(out.vertices.empty() && out.indices.empty(), "a zero-length segment must vanish");
		mesh = lineMesh({point(0, 0, -1), point(0, 0, -50)});
		expandLines(mesh, projection, viewport, 2.0f, false, out);
		check(out.vertices.empty() && out.indices.empty(),
			  "a segment aimed at the eye projects to a point and must vanish");
		mesh = lineMesh({point(-0.5, 0.3, -5), point(0.5, -0.2, -5.3), point(0.3, 0.2, -6), point(0.3, 0.2, -6),
						 point(-0.5, -0.3, -7), point(0.5, 0.2, -7.3)});
		expandLines(mesh, projection, viewport, 2.0f, false, out);
		check(out.vertices.size() == 8, "a dropped segment must not drop its neighbors");
		check(out.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7}),
			  "expanded quads must use the tessellator's 0,1,2 0,2,3 diagonal without holes");
	}

	// The width is in framebuffer pixels, so the emitted geometry has to change with
	// the viewport while the measured width does not.
	{
		const MeshData mesh = lineMesh({point(-0.5, 0.3, -8.0), point(0.5, -0.2, -8.3)});
		float reference = 0;
		for (Rect target : {Rect{0, 0, 854, 480}, Rect{0, 0, 1708, 960}, Rect{32, 16, 320, 240}})
		{
			expandLines(mesh, projection, target, 3.0f, false, out);
			const Ribbon r = measure(out, 0, projection, target);
			check(near(r.width0, 3.0, 1e-3) && near(r.width1, 3.0, 1e-3),
				  "the pixel width must not follow the viewport size");
			check(r.area > 0, "winding must not depend on the viewport");
			if (target.width == 854)
				reference = out.vertices[0].x;
			else if (target.width == 1708)
				check(reference != out.vertices[0].x, "a viewport twice as wide must halve the emitted offset");
		}
	}

	// Winding is decided in window space, so it must survive both segment
	// directions, both major axes, and the y-flipped GUI ortho.
	{
		const Mat4 gui = Mat4::ortho(0, 854, 480, 0, 1000, 3000) * Mat4::translation(0, 0, -2000);
		const std::vector<std::vector<Vertex>> perspective{{point(-0.5, 0.3, -8.0), point(0.5, -0.2, -8.3)},
														   {point(0.5, -0.2, -8.3), point(-0.5, 0.3, -8.0)},
														   {point(0, -1, -8), point(0, 1, -8)},
														   {point(0, 1, -8), point(0, -1, -8)},
														   {point(-1, 0, -8), point(1, 0, -8)},
														   {point(1, 0, -8), point(-1, 0, -8)}};
		for (const std::vector<Vertex> &segment : perspective)
		{
			expandLines(lineMesh(segment), projection, viewport, 2.0f, false, out);
			const Ribbon r = measure(out, 0, projection, viewport);
			check(r.area > 0, "every segment orientation must expand counter-clockwise");
			check(near(r.width0, 2.0, 1e-3), "orientation must not change the width");
		}
		const std::vector<std::vector<Vertex>> flat2d{{point(10, 20, 0), point(500, 300, 0)},
													  {point(500, 300, 0), point(10, 20, 0)},
													  {point(400, 40, 0), point(400, 400, 0)},
													  {point(40, 240, 0), point(800, 240, 0)}};
		for (const std::vector<Vertex> &segment : flat2d)
		{
			expandLines(lineMesh(segment), gui, viewport, 2.0f, false, out);
			const Ribbon r = measure(out, 0, gui, viewport);
			check(r.area > 0, "the y-flipped GUI ortho must still wind counter-clockwise");
			check(near(r.width0, 2.0, 1e-3) && near(r.width1, 2.0, 1e-3),
				  "an orthographic line must be exactly as wide as requested");
			check(near(r.depth[0], r.depth[1], 1e-9), "an orthographic line must keep one depth");
		}
	}

	// A wide line whose centerline is outside the viewport still covers pixels
	// inside it, so the side planes are padded by half a width; a line beyond that
	// pad covers nothing and is dropped.
	{
		const Mat4 gui = Mat4::ortho(0, 854, 480, 0, 1000, 3000) * Mat4::translation(0, 0, -2000);
		expandLines(lineMesh({point(856, 100, 0), point(856, 300, 0)}), gui, viewport, 8.0f, false, out);
		check(out.vertices.size() == 4, "half of this line covers viewport pixels and must survive");
		const Ribbon kept = measure(out, 0, gui, viewport);
		check(near(kept.width0, 8.0, 1e-3), "a partially offscreen line keeps its width");
		expandLines(lineMesh({point(860, 100, 0), point(860, 300, 0)}), gui, viewport, 8.0f, false, out);
		check(out.vertices.empty(), "a line entirely beyond the padded side plane must be dropped");
	}

	// The output vectors are reused, so a smaller mesh must not inherit the tail of
	// a larger one, and an empty viewport must yield an empty, skippable mesh.
	{
		MeshData many = lineMesh({point(-0.5, 0.3, -5), point(0.5, -0.2, -5.3), point(-0.5, -0.3, -6),
								  point(0.5, 0.2, -6.3), point(-0.4, 0.1, -7), point(0.4, -0.1, -7.3)});
		many.hasTexture = false;
		many.hasColor = true;
		many.hasNormal = false;
		expandLines(many, projection, viewport, 2.0f, false, out);
		check(out.vertices.size() == 12 && out.indices.size() == 18, "three segments must expand to three quads");
		check(!out.hasTexture && out.hasColor && !out.hasNormal, "expansion must carry the attribute flags across");
		const MeshData one = lineMesh({point(-0.5, 0.3, -5), point(0.5, -0.2, -5.3)});
		expandLines(one, projection, viewport, 2.0f, false, out);
		check(out.vertices.size() == 4 && out.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}),
			  "reused output kept stale geometry");
		check(out.hasTexture && out.hasColor && out.hasNormal, "reused output kept stale attribute flags");
		expandLines(one, projection, Rect{0, 0, 0, 0}, 2.0f, false, out);
		check(out.vertices.empty() && out.indices.empty() && out.primitive == Primitive::Triangles,
			  "an empty viewport must produce an empty triangle mesh, not a throw");
	}

	// Malformed input is a caller bug, not something to silently paper over.
	{
		const MeshData lines = lineMesh({point(0, 0, -5), point(1, 0, -5)});
		MeshData triangles = lines;
		triangles.primitive = Primitive::Triangles;
		check(rejects([&] { expandLines(triangles, projection, viewport, 2.0f, false, out); }),
			  "only normalized line pairs may be expanded");
		MeshData odd = lines;
		odd.indices.pop_back();
		check(rejects([&] { expandLines(odd, projection, viewport, 2.0f, false, out); }),
			  "an incomplete line pair must be rejected");
		MeshData outOfRange = lines;
		outOfRange.indices[1] = 7;
		check(rejects([&] { expandLines(outOfRange, projection, viewport, 2.0f, false, out); }),
			  "an out-of-range line index must be rejected");
		check(rejects([&] { expandLines(lines, projection, viewport, 0.0f, false, out); }),
			  "a non-positive line width must be rejected");
		Mat4 singular = Mat4::identity();
		singular.v[10] = 0;
		check(rejects([&] { expandLines(lines, singular, viewport, 2.0f, false, out); }),
			  "a singular model-projection matrix cannot be unprojected");
		MeshData self = lines;
		check(rejects([&] { expandLines(self, projection, viewport, 2.0f, false, self); }),
			  "expanding a mesh into itself must be rejected");
	}

	// LevelRenderer::render(AABB) as the game issues it: two closing line strips and
	// four verticals, untextured, unlit, at glLineWidth(2), under a real modelview.
	{
		const Mat4 modelview = Mat4::translation(0.5, -0.25, -6.0) * Mat4::rotation(35, 0, 1, 0);
		const Mat4 mvp = projection * modelview;
		const double x0 = -0.5, y0 = -0.5, z0 = -0.5, x1 = 0.5, y1 = 0.5, z1 = 0.5;
		std::vector<MeshData> parts(3);
		parts[0].vertices = {point(x0, y0, z0), point(x1, y0, z0), point(x1, y0, z1), point(x0, y0, z1),
							 point(x0, y0, z0)};
		parts[1].vertices = {point(x0, y1, z0), point(x1, y1, z0), point(x1, y1, z1), point(x0, y1, z1),
							 point(x0, y1, z0)};
		parts[2].vertices = {point(x0, y0, z0), point(x0, y1, z0), point(x1, y0, z0), point(x1, y1, z0),
							 point(x1, y0, z1), point(x1, y1, z1), point(x0, y0, z1), point(x0, y1, z1)};
		normalizeTopology(parts[0], SourceTopology::LineStrip);
		normalizeTopology(parts[1], SourceTopology::LineStrip);
		normalizeTopology(parts[2], SourceTopology::Lines);
		std::size_t ribbons = 0;
		for (MeshData &part : parts)
		{
			part.hasTexture = false;
			part.hasNormal = false;
			expandLines(part, mvp, viewport, 2.0f, false, out);
			check(out.vertices.size() == part.indices.size() * 2, "the outline lost an edge");
			for (std::size_t quad = 0; quad < out.vertices.size() / 4; ++quad, ++ribbons)
			{
				const Ribbon r = measure(out, quad, mvp, viewport);
				check(near(r.width0, 2.0, 1e-3) && near(r.width1, 2.0, 1e-3),
					  "a selection outline edge is not two framebuffer pixels wide");
				check(r.area > 0, "a selection outline edge wound the wrong way");
				check(r.length > 1, "a selection outline edge collapsed");
			}
		}
		check(ribbons == 12, "the selection outline must expand to twelve ribbons");
	}

	std::cout << "line geometry semantic regressions passed\n";
	return 0;
}
catch (const std::exception &error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
