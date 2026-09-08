#pragma once

#include "client/renderer/portable/RenderTypes.h"

namespace b173
{
namespace render
{
// B173 - Screen-space expansion of the game's line primitives into triangles,
// for backends that have no wide-line rasterizer (GL core, GLES, D3D, Vulkan,
// Metal). The input is a mesh already normalized by normalizeTopology, so
// GL_LINES, GL_LINE_STRIP and GL_LINE_LOOP all arrive as independent pairs.
//
// The requested width is a framebuffer-pixel width, exactly like glLineWidth:
// the offset is applied in clip space scaled by each endpoint's own w, so the
// ribbon keeps that pixel width at every distance instead of becoming a
// world-space tube. Both corners of an endpoint keep that endpoint's clip z and
// w, so per-endpoint depth (and therefore depth testing along the line) is
// preserved and no polygon offset is needed.
//
// Positions are emitted in the input's own object space: the offset clip-space
// corners are unprojected through the inverse of modelProjection, so the game's
// unchanged 28-byte Vertex and the unchanged shaders (which apply modelview and
// projection themselves) reproduce the intended clip position. modelProjection
// must therefore be exactly the projection * modelview product the draw will
// use, and viewport the framebuffer rectangle it will use.
//
// Segments are clipped in homogeneous coordinates before any division: against
// near and far, and against the x/y side planes pushed out by half a width plus
// half a pixel, which is the furthest the ribbon can reach past its centerline.
// Nothing that could cover a viewport pixel is dropped, and no endpoint with a
// non-positive w ever reaches the divide. A segment is skipped, contributing no
// triangles, when it lies fully outside that volume or when its window-space
// projection has zero length (a zero-length segment, or one aimed at the eye);
// aliased GL lines produce no fragments for those either.
//
// flat selects GL's last-vertex provoking convention: the color and normal of
// the second endpoint - the original one, not the interpolated clip point - are
// given to all four corners, matching flatMesh. Texture coordinates, and every
// attribute of a smooth-shaded segment, are interpolated at the clip point with
// the same homogeneous parameter as the position, as GL clipping does.
//
// output is reused: its vectors are cleared, keeping their capacity, and refilled
// with Primitive::Triangles, four vertices and six indices per surviving segment
// on the tessellator's 0,1,2 0,2,3 diagonal, counter-clockwise in window space.
// An empty result is legal, exactly as for normalizeTopology, and the caller must
// skip the draw instead of validating it. Throws for a malformed mesh, a
// non-positive width, a singular matrix, or an output aliasing the input.
void expandLines(const MeshData &input, const Mat4 &modelProjection, Rect viewport, float width, bool flat,
				 MeshData &output);
} // namespace render
} // namespace b173
