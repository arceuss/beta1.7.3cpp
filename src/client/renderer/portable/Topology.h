#pragma once

#include "client/renderer/portable/RenderTypes.h"

namespace b173
{
namespace render
{
// B173 - Normalize the non-quad topologies consumed by the existing tessellator.
// Raw GL_QUADS/GL_POLYGON display-list semantics are deliberately NOT conflated
// with tessellator quads, which have already selected a triangle diagonal.
enum class SourceTopology
{
	Points,
	Lines,
	LineStrip,
	LineLoop,
	Triangles,
	TriangleStrip,
	TriangleFan
};
// Preserve primitive order and the GL last-vertex provoking convention. Trailing
// incomplete primitives are ignored, just as primitive assembly ignores them.
// Empty results are allowed; the caller must skip them before createMesh/draw.
void normalizeTopology(MeshData &mesh, SourceTopology topology);
} // namespace render
} // namespace b173
