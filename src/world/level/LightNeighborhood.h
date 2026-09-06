#pragma once

#include "world/level/Level.h"
#include <cassert>

// Retains ownership only for the current lighting update, never across ticks.
class LightNeighborhood
{
	Level &level;
	int_t centerX = 0, centerZ = 0;
	bool centered = false;
	std::shared_ptr<LevelChunk> chunks[9];
	bool checked[9] = {};

public:
	explicit LightNeighborhood(Level &level) : level(level) {}

	void setCenter(int_t x, int_t z)
	{
		const int_t xc = x >> 4, zc = z >> 4;
		if (centered && xc == centerX && zc == centerZ)
			return;
		centerX = xc;
		centerZ = zc;
		centered = true;
		for (int i = 0; i < 9; ++i)
		{
			chunks[i].reset();
			checked[i] = false;
		}
	}

	int_t getBrightness(int_t layer, int_t x, int_t y, int_t z)
	{
		if (y < 0) y = 0;
		if (y >= Level::DEPTH) y = Level::DEPTH - 1;
		// Level::getBrightness intentionally accepts z == MAX_LEVEL_SIZE.
		if (x < -Level::MAX_LEVEL_SIZE || z < -Level::MAX_LEVEL_SIZE ||
			x >= Level::MAX_LEVEL_SIZE || z > Level::MAX_LEVEL_SIZE)
			return LightLayer::surrounding(layer);
		const int_t dx = (x >> 4) - centerX, dz = (z >> 4) - centerZ;
		assert(centered && dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1);
		const int_t slot = (dx + 1) * 3 + dz + 1;
		if (!checked[slot])
		{
			if (level.hasChunkAt(x, 0, z))
				chunks[slot] = level.getChunk(x >> 4, z >> 4);
			checked[slot] = true;
		}
		LevelChunk *chunk = chunks[slot].get();
		return chunk ? chunk->getBrightness(layer, x & 15, y, z & 15) : 0;
	}
};
