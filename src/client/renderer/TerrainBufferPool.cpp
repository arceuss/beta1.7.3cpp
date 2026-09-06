#include "client/renderer/TerrainBufferPool.h"
#include "client/renderer/TerrainVertex.h"

#include <algorithm>
#include <map>
#include <tuple>

using TerrainPoolKey = std::tuple<int, int, int>;

// Only the render thread accesses pools. Chunks own them; this index does not.
static std::map<TerrainPoolKey, std::weak_ptr<TerrainBufferPool>> &terrainPools()
{
	static std::map<TerrainPoolKey, std::weak_ptr<TerrainBufferPool>> pools;
	return pools;
}

std::shared_ptr<TerrainBufferPool> TerrainBufferPool::get(int x, int z, int layer)
{
	const TerrainPoolKey key(x, z, layer);
	std::weak_ptr<TerrainBufferPool> &entry = terrainPools()[key];
	std::shared_ptr<TerrainBufferPool> pool = entry.lock();
	if (!pool)
	{
		pool = std::make_shared<TerrainBufferPool>(x, z, layer);
		entry = pool;
	}
	return pool;
}

TerrainBufferPool::~TerrainBufferPool()
{
	if (buffer != 0)
		glDeleteBuffers(1, &buffer);
	auto &pools = terrainPools();
	auto found = pools.find(TerrainPoolKey(x, z, layer));
	if (found != pools.end() && found->second.expired())
		pools.erase(found);
}

void TerrainBufferPool::grow(std::size_t required)
{
	if (required <= capacity)
		return;
	std::size_t next = std::max(capacity, static_cast<std::size_t>(4096 * TERRAIN_VERTEX_STRIDE));
	while (next < required)
		next += next / 2;

	if (buffer == 0)
		glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	// GL 2.1 has no copy-buffer target. Read back only when storage must grow;
	// normal rebuilds upload just their changed range, without a CPU mirror.
	std::vector<char> previous(end);
	if (end != 0)
		glGetBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(end), previous.data());
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(next), nullptr, GL_STATIC_DRAW);
	if (end != 0)
		glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(end), previous.data());
	capacity = next;
}

TerrainBufferPool::Range TerrainBufferPool::allocate(std::size_t bytes)
{
	for (auto it = freeRanges.begin(); it != freeRanges.end(); ++it)
	{
		if (it->capacity < bytes)
			continue;
		Range result{it->offset, bytes};
		it->offset += bytes;
		it->capacity -= bytes;
		if (it->capacity == 0)
			freeRanges.erase(it);
		return result;
	}
	grow(end + bytes);
	Range result{end, bytes};
	end += bytes;
	return result;
}

void TerrainBufferPool::release(Range &range)
{
	if (range.capacity == 0)
		return;
	auto at = std::lower_bound(freeRanges.begin(), freeRanges.end(), range.offset,
		[](const Range &entry, std::size_t offset) { return entry.offset < offset; });
	at = freeRanges.insert(at, range);
	if (at != freeRanges.begin())
	{
		auto before = at - 1;
		if (before->offset + before->capacity == at->offset)
		{
			before->capacity += at->capacity;
			at = freeRanges.erase(at) - 1;
		}
	}
	if (at + 1 != freeRanges.end() && at->offset + at->capacity == (at + 1)->offset)
	{
		at->capacity += (at + 1)->capacity;
		freeRanges.erase(at + 1);
	}
	if (!freeRanges.empty() && freeRanges.back().offset + freeRanges.back().capacity == end)
	{
		end = freeRanges.back().offset;
		freeRanges.pop_back();
	}
	range = Range();
}

void TerrainBufferPool::upload(Range &range, const void *data, std::size_t bytes)
{
	if (bytes == 0)
	{
		release(range);
		return;
	}
	if (range.capacity < bytes)
	{
		release(range);
		range = allocate(bytes);
	}
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(range.offset), static_cast<GLsizeiptr>(bytes), data);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
