#include "world/level/tile/SaplingTile.h"
#include "ClientTarget.h"

#include <memory>

#include "world/level/Level.h"
#include "world/level/tile/Tile.h"
#include "world/level/levelgen/feature/Feature.h"
#include "world/level/levelgen/feature/TreeFeature.h"
#include "world/level/levelgen/feature/BigTreeFeature.h"
#include "world/level/levelgen/feature/ForestFeature.h"
#include "world/level/levelgen/feature/Taiga2Feature.h"

SaplingTile::SaplingTile(int_t id, int_t tex) : FlowerTile(id, tex)
{
	updateDefaultShape();
}

void SaplingTile::updateDefaultShape()
{
	float radius = 0.4f;
	setShape(0.5f - radius, 0.0f, 0.5f - radius, 0.5f + radius, radius * 2.0f, 0.5f + radius);
}

void SaplingTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	if (level.isOnline)
		return;

	FlowerTile::tick(level, x, y, z, random);

	if (level.getRawBrightness(x, y + 1, z) >= 9 && random.nextInt(30) == 0)
	{
		int_t data = level.getData(x, y, z);
		if ((data & 8) == 0)
			level.setData(x, y, z, data | 8);
		else
			growTree(level, x, y, z, random);
	}
}

void SaplingTile::growTree(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	int_t type = level.getData(x, y, z) & 3;
	level.setTileNoUpdate(x, y, z, 0);

	std::unique_ptr<Feature> feature;
	if (type == 1)
	{
		feature = std::make_unique<Taiga2Feature>();
	}
	else if (type == 2)
	{
		feature = std::make_unique<ForestFeature>();
	}
	else
	{
		feature = std::make_unique<TreeFeature>();
		if (random.nextInt(10) == 0)
			feature = std::make_unique<BigTreeFeature>();
	}

	if (!feature->place(level, random, x, y, z))
		level.setTileAndDataNoUpdate(x, y, z, id, type);
}

int_t SaplingTile::getTexture(Facing face, int_t data)
{
	if (ClientTarget::isAlphaPlace())
		return tex;
	int_t type = data & 3;
	if (type == 1) return 63;
	if (type == 2) return 79;
	return tex;
}

int_t SaplingTile::getResource(int_t data, Random &random)
{
	(void)data;
	(void)random;
	return id;
}

int_t SaplingTile::getSpawnResourcesAuxValue(int_t data)
{
	return data & 3;
}
