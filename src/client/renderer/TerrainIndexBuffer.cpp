#include "client/renderer/TerrainIndexBuffer.h"

#include <cstdint>
#include <vector>

GLuint TerrainIndexBuffer::buffers[2] = {0, 0};
int_t TerrainIndexBuffer::capacity[2] = {0, 0};

// Derived from Tesselator::vertex() quad-to-triangle duplication order.
static const int_t QUAD_PATTERN[6] = {0, 1, 2, 0, 2, 3};

// 16-bit indices address vertices 0..65535, i.e. 16384 four-vertex quads.
static const int_t MAX_SHORT_QUADS = 16384;

GLuint TerrainIndexBuffer::get(int_t quads, GLenum &type)
{
	const int_t wide = quads > MAX_SHORT_QUADS ? 1 : 0;
	type = wide ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	if (capacity[wide] < quads)
	{
		int_t newCapacity = capacity[wide] * 2;
		if (newCapacity < quads) newCapacity = quads;
		if (newCapacity < 4096) newCapacity = 4096;
		if (!wide && newCapacity > MAX_SHORT_QUADS) newCapacity = MAX_SHORT_QUADS;

		if (buffers[wide] == 0)
			glGenBuffers(1, &buffers[wide]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[wide]);

		if (wide)
		{
			std::vector<std::uint32_t> indices(static_cast<std::size_t>(newCapacity) * 6);
			std::size_t at = 0;
			for (int_t q = 0; q < newCapacity; q++)
				for (int_t k = 0; k < 6; k++)
					indices[at++] = static_cast<std::uint32_t>(q * 4 + QUAD_PATTERN[k]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)), indices.data(), GL_STATIC_DRAW);
		}
		else
		{
			std::vector<std::uint16_t> indices(static_cast<std::size_t>(newCapacity) * 6);
			std::size_t at = 0;
			for (int_t q = 0; q < newCapacity; q++)
				for (int_t k = 0; k < 6; k++)
					indices[at++] = static_cast<std::uint16_t>(q * 4 + QUAD_PATTERN[k]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint16_t)), indices.data(), GL_STATIC_DRAW);
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		capacity[wide] = newCapacity;
	}

	return buffers[wide];
}

void TerrainIndexBuffer::release()
{
	glDeleteBuffers(2, buffers);
	buffers[0] = buffers[1] = 0;
	capacity[0] = capacity[1] = 0;
}
