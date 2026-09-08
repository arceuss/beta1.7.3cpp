#include "client/renderer/portable/Device.h"

#include "client/renderer/portable/LegacyState.h"

#include <algorithm>

namespace b173
{
namespace render
{
MeshData clearTriangle(float depth)
{
	MeshData mesh;
	mesh.hasTexture = mesh.hasColor = mesh.hasNormal = false;
	const float z = 2 * std::max(0.0f, std::min(1.0f, depth)) - 1;
	mesh.vertices.resize(3);
	mesh.vertices[0].x = -1;
	mesh.vertices[0].y = -1;
	mesh.vertices[0].z = z;
	mesh.vertices[1].x = 3;
	mesh.vertices[1].y = -1;
	mesh.vertices[1].z = z;
	mesh.vertices[2].x = -1;
	mesh.vertices[2].y = 3;
	mesh.vertices[2].z = z;
	mesh.indices = {0, 1, 2};
	return mesh;
}

Draw clearDraw(MeshId triangle, int width, int height, Vec4 color, bool colorBit, bool depthBit,
			   const PipelineState &caller)
{
	Draw d;
	d.mesh = triangle;
	d.pipeline.viewport = {0, 0, width, height};
	d.pipeline.scissor = caller.scissor;
	d.pipeline.scissorRect = caller.scissorRect;
	d.pipeline.depthWrite = depthBit && caller.depthWrite;
	d.pipeline.depthTest = d.pipeline.depthWrite;
	d.pipeline.depthCompare = Compare::Always;
	d.pipeline.colorMask = colorBit ? caller.colorMask : 0;
	LegacyState state;
	state.currentColor = color;
	MeshData attributes;
	attributes.hasTexture = attributes.hasColor = attributes.hasNormal = false;
	d.uniforms = state.uniforms(attributes, nullptr, ClipConvention::OpenGL);
	return d;
}
} // namespace render
} // namespace b173
