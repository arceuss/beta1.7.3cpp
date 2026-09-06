#pragma once

#include "OpenGL.h"
#include <cstddef>
#include <memory>
#include <vector>

class TerrainBufferPool
{
public:
	struct Range
	{
		std::size_t offset = 0;
		std::size_t capacity = 0;
	};

private:
	int x, z, layer;
	GLuint buffer = 0;
	std::size_t capacity = 0;
	std::size_t end = 0;
	std::vector<Range> freeRanges;
	void grow(std::size_t required);
	Range allocate(std::size_t bytes);

public:
	TerrainBufferPool(int x, int z, int layer) : x(x), z(z), layer(layer) {}
	~TerrainBufferPool();
	TerrainBufferPool(const TerrainBufferPool &) = delete;
	TerrainBufferPool &operator=(const TerrainBufferPool &) = delete;
	static std::shared_ptr<TerrainBufferPool> get(int x, int z, int layer);
	void upload(Range &range, const void *data, std::size_t bytes);
	void release(Range &range);
	GLuint getBuffer() const { return buffer; }
};
