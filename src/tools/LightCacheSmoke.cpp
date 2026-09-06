#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/LightNeighborhood.h"
#include "world/level/tile/LockedChestTile.h"

#include <iostream>
#include <map>

class LightCacheSmokeSource : public ChunkSource
{
public:
	std::map<std::pair<int_t, int_t>, std::shared_ptr<LevelChunk>> chunks;
	bool hasChunk(int_t x, int_t z) override { return chunks.count({x, z}) != 0; }
	std::shared_ptr<LevelChunk> getChunk(int_t x, int_t z) override { return chunks.at({x, z}); }
	void postProcess(ChunkSource &, int_t, int_t) override {}
	bool save(bool, std::shared_ptr<ProgressListener>) override { return true; }
	bool tick() override { return false; }
	bool shouldSave() override { return false; }
	jstring gatherStats() override { return u"light-cache-smoke"; }
};

class LightCacheSmokeLevel : public Level
{
public:
	std::shared_ptr<LightCacheSmokeSource> source;
	LightCacheSmokeLevel() : Level(u"light-cache-smoke", Dimension::Id_Normal, 1234567, false),
		source(std::make_shared<LightCacheSmokeSource>())
	{
		setChunkSource(source);
	}
};

static bool checkBoundary(int_t cx, int_t cz, bool alongX, bool neighbors)
{
	LightCacheSmokeLevel level;
	for (int_t dx = -1; dx <= 1; ++dx)
		for (int_t dz = -1; dz <= 1; ++dz)
			if (neighbors || (dx == 0 && dz == 0))
				level.source->chunks[{cx + dx, cz + dz}] = std::make_shared<LevelChunk>(level, cx + dx, cz + dz);
	std::shared_ptr<LevelChunk> chunk = level.getChunk(cx, cz);
	for (int_t i = 0; i < 16; ++i)
	{
		const int_t x = alongX ? i : 8;
		const int_t z = alongX ? 8 : i;
		chunk->blocks[(x << 11) | (z << 7) | 64] = static_cast<ubyte_t>(Tile::lockedChest.id);
	}
	const int_t x0 = cx * 16 + (alongX ? 0 : 8);
	const int_t z0 = cz * 16 + (alongX ? 8 : 0);
	LightUpdate update(LightLayer::Block, x0, 64, z0, x0 + (alongX ? 15 : 0), 64, z0 + (alongX ? 0 : 15));
	update.update(level);
	for (int_t i = 0; i < 16; ++i)
	{
		const int_t expected = neighbors || (i > 0 && i < 15) ? 15 : 0;
		const int_t actual = chunk->getBrightness(LightLayer::Block, alongX ? i : 8, 64, alongX ? 8 : i);
		if (actual != expected)
		{
			std::cerr << "light-cache-smoke: chunk " << cx << ',' << cz << " axis " << alongX
				<< " neighbors " << neighbors << " column " << i << " expected " << expected << " got " << actual << '\n';
			return false;
		}
	}
	return true;
}

static bool checkNeighborhood(int_t centerX, int_t centerZ)
{
	LightCacheSmokeLevel level;
	Random random(987654321);
	for (int_t dx = -1; dx <= 1; ++dx)
		for (int_t dz = -1; dz <= 1; ++dz)
		{
			if (dx == -1 && dz == 0) continue;
			const int_t cx = (centerX >> 4) + dx, cz = (centerZ >> 4) + dz;
			std::shared_ptr<LevelChunk> chunk = std::make_shared<LevelChunk>(level, cx, cz);
			for (std::size_t i = 0; i < chunk->skyLight.data.size(); ++i)
			{
				chunk->skyLight.data[i] = static_cast<byte_t>(random.nextInt(256));
				chunk->blockLight.data[i] = static_cast<byte_t>(random.nextInt(256));
			}
			level.source->chunks[{cx, cz}] = chunk;
		}
	LightNeighborhood neighborhood(level);
	neighborhood.setCenter(centerX, centerZ);
	for (int_t layer : {LightLayer::Sky, LightLayer::Block})
		for (int_t x = centerX - 1; x <= centerX + 1; ++x)
			for (int_t z = centerZ - 1; z <= centerZ + 1; ++z)
				for (int_t y : {-17, -1, 0, 1, 63, 126, 127, 128, 255})
					if (neighborhood.getBrightness(layer, x, y, z) != level.getBrightness(layer, x, y, z))
					{
						std::cerr << "light-cache-smoke: neighborhood differs at " << x << ',' << y << ',' << z << '\n';
						return false;
					}
	return true;
}

int main()
{
	Tile::initTiles();
	bool ok = true;
	for (bool alongX : {false, true})
		for (bool neighbors : {false, true})
		{
			ok &= checkBoundary(0, 0, alongX, neighbors);
			ok &= checkBoundary(-1, -1, alongX, neighbors);
		}
	for (int_t x : {-Level::MAX_LEVEL_SIZE, -17, -16, -1, 0, 15, 16, Level::MAX_LEVEL_SIZE - 1})
		for (int_t z : {-Level::MAX_LEVEL_SIZE, -1, 0, 15, 16, Level::MAX_LEVEL_SIZE})
			ok &= checkNeighborhood(x, z);
	if (ok)
		std::cout << "light-cache-smoke: boundary propagation and neighborhood reads match Level on both layers, height clamps and world limits\n";
	return ok ? 0 : 1;
}
