#include "client/renderer/portable/Topology.h"

#include <limits>
#include <numeric>

namespace b173
{
namespace render
{
void normalizeTopology(MeshData &mesh, SourceTopology topology)
{
	if (mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max())
		throw std::length_error("topology vertex count exceeds index range");
	std::vector<std::uint32_t> input = mesh.indices;
	if (input.empty())
	{
		input.resize(mesh.vertices.size());
		std::iota(input.begin(), input.end(), std::uint32_t(0));
	}
	for (std::uint32_t n : input)
		if (n >= mesh.vertices.size())
			throw std::out_of_range("topology index out of bounds");
	std::vector<std::uint32_t> output;
	const std::size_t count = input.size();
	if (count > std::size_t(std::numeric_limits<int>::max()) / 3)
		throw std::length_error("normalized topology exceeds draw limits");
	Primitive primitive = Primitive::Triangles;
	switch (topology)
	{
	case SourceTopology::Points:
		primitive = Primitive::Points;
		output = std::move(input);
		break;
	case SourceTopology::Lines:
		primitive = Primitive::Lines;
		input.resize(count - count % 2);
		output = std::move(input);
		break;
	case SourceTopology::LineStrip:
	case SourceTopology::LineLoop:
		primitive = Primitive::Lines;
		output.reserve(count * 2);
		for (std::size_t i = 1; i < count; ++i)
			output.insert(output.end(), {input[i - 1], input[i]});
		if (topology == SourceTopology::LineLoop && count > 1)
			output.insert(output.end(), {input.back(), input.front()});
		break;
	case SourceTopology::Triangles:
		input.resize(count - count % 3);
		output = std::move(input);
		break;
	case SourceTopology::TriangleStrip:
		output.reserve(count * 3);
		// Odd triangles swap their first two vertices, keeping both the winding
		// and the last-vertex provoking convention of GL primitive assembly.
		for (std::size_t i = 2; i < count; ++i)
		{
			if (i & 1)
				output.insert(output.end(), {input[i - 1], input[i - 2], input[i]});
			else
				output.insert(output.end(), {input[i - 2], input[i - 1], input[i]});
		}
		break;
	case SourceTopology::TriangleFan:
		output.reserve(count * 3);
		for (std::size_t i = 2; i < count; ++i)
			output.insert(output.end(), {input[0], input[i - 1], input[i]});
		break;
	default:
		throw std::invalid_argument("unsupported source topology");
	}
	mesh.primitive = primitive;
	mesh.indices = std::move(output);
}
} // namespace render
} // namespace b173
