#include "client/renderer/portable/LegacyState.h"
#include "client/renderer/portable/Topology.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace b173::render;

static void check(bool value, const char *message)
{
	if (!value)
		throw std::runtime_error(message);
}

static bool near(float a, float b)
{
	return std::abs(a - b) < 0.00001f;
}

int main()
try
{
	MeshData quad;
	quad.vertices.resize(4);
	quad.indices = quadIndices(1);
	for (unsigned i = 0; i < 4; ++i)
	{
		quad.vertices[i].rgba[0] = static_cast<unsigned char>(i * 40);
		quad.vertices[i].normal[0] = static_cast<unsigned char>(i * 20);
	}
	check(quad.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}), "quad diagonal changed");
	MeshData flat = flatMesh(quad);
	for (unsigned i = 0; i < 6; ++i)
	{
		check(flat.vertices[i].rgba[0] == (i < 3 ? 80 : 120), "flat quad provoking color changed");
		check(flat.vertices[i].normal[0] == (i < 3 ? 40 : 60), "flat quad provoking normal changed");
	}
	MeshData strip;
	strip.vertices.resize(5);
	normalizeTopology(strip, SourceTopology::TriangleStrip);
	check(strip.indices == std::vector<std::uint32_t>({0, 1, 2, 2, 1, 3, 2, 3, 4}),
		  "strip winding or provoking vertex changed");
	MeshData fan;
	fan.vertices.resize(4);
	normalizeTopology(fan, SourceTopology::TriangleFan);
	check(fan.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}), "sky fan changed");
	MeshData large;
	large.vertices.resize(65538);
	for (std::uint32_t i = 0; i < 65538; ++i)
	{
		large.vertices[i].x = static_cast<float>(i);
		large.indices.push_back(i);
	}
	const auto batches = split16(large);
	std::size_t at = 0;
	for (const auto &batch : batches)
	{
		check(batch.indices.size() % 3 == 0, "GLES partition split a triangle");
		for (std::uint16_t index : batch.indices)
			check(batch.vertices[index].x == static_cast<float>(at++), "GLES partition reordered vertices");
	}
	check(at == large.indices.size(), "GLES partition lost geometry");
	LegacyState state;
	state.translate(2, 0, 0);
	state.rotate(90, 0, 0, 1);
	Vec4 p = state.matrix(MatrixMode::ModelView) * Vec4{1, 0, 0, 1};
	check(near(p.x, 2) && near(p.y, 1), "matrix operations must postmultiply");
	state.pushMatrix();
	state.scale(2, 3, 4);
	state.pushMatrix();
	state.translate(9, 8, 7);
	state.popMatrix();
	state.popMatrix();
	p = state.matrix(MatrixMode::ModelView) * Vec4{1, 0, 0, 1};
	check(near(p.x, 2) && near(p.y, 1), "nested matrix stack restoration failed");
	state.setLightPosition(0, {1, 0, 0, 0});
	state.loadIdentity();
	check(near(state.lights[0].eyePosition.x, 0) && near(state.lights[0].eyePosition.y, 1),
		  "light must retain setter-time transform");
	state.matrixMode(MatrixMode::Texture);
	state.translate(0.25, 0.5, 0);
	state.matrixMode(MatrixMode::ModelView);
	check(near(state.matrix(MatrixMode::Texture).v[12], 0.25f) && near(state.matrix(MatrixMode::ModelView).v[12], 0),
		  "texture matrix contaminated modelview");
	const Mat4 projection = Mat4::frustum(-1, 1, -1, 1, 1, 100);
	const Vec4 nearPoint = projection * Vec4{0, 0, -1, 1};
	const Vec4 farPoint = projection * Vec4{0, 0, -100, 1};
	check(near(nearPoint.z / nearPoint.w, -1) && near(farPoint.z / farPoint.w, 1), "canonical clip depth changed");
	check(!alphaPass(0.1f, Compare::Greater, 0.1f), "equal alpha must fail GL_GREATER");
	check(alphaPass(std::nextafter(0.1f, 1.0f), Compare::Greater, 0.1f), "alpha immediately above threshold must pass");
	check(!alphaPass(std::nextafter(0.1f, 0.0f), Compare::Greater, 0.1f),
		  "alpha immediately below threshold must fail");
	check(near(normalByte(128, NormalConversion::Legacy21), -1) &&
			  near(normalByte(0, NormalConversion::Legacy21), 1.0f / 255),
		  "signed normal conversion changed");
	PipelineState opaque;
	opaque.depthTest = true;
	PipelineState transparent = opaque;
	transparent.depthWrite = false;
	check(pipelineKey(opaque, Primitive::Triangles) != pipelineKey(transparent, Primitive::Triangles),
		  "depth writes collapsed into depth testing");
	transparent.depthTest = false;
	check(pipelineKey(transparent, Primitive::Triangles) != pipelineKey(opaque, Primitive::Triangles),
		  "depth test missing from pipeline key");
	std::cout << "renderer semantic regressions passed\n";
	return 0;
}
catch (const std::exception &error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
