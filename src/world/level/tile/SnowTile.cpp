#include "world/level/tile/SnowTile.h"

#include "world/level/Level.h"
#include "world/level/LevelSource.h"
#include "world/level/material/Material.h"

SnowTile::SnowTile(int_t id, int_t tex) : Tile(id, tex, Material::snow())
{
	updateDefaultShape();
	updateCachedProperties();
}

bool SnowTile::isCubeShaped()
{
	return false;
}

bool SnowTile::isSolidRender()
{
	return false;
}

AABB *SnowTile::getAABB(Level &level, int_t x, int_t y, int_t z)
{
	int_t data = level.getData(x, y, z) & 7;
	if (data < 3)
		return nullptr;
	return AABB::newTemp(x + xx0, y + yy0, z + zz0, x + xx1, y + 0.5, z + zz1);
}

void SnowTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	if (!canSnowStay(level, x, y, z))
		level.setTile(x, y, z, 0);
}

void SnowTile::onPlace(Level &level, int_t x, int_t y, int_t z)
{
	if (!canSnowStay(level, x, y, z))
		level.setTile(x, y, z, 0);
}

void SnowTile::updateShape(LevelSource &level, int_t x, int_t y, int_t z)
{
	int_t data = level.getData(x, y, z) & 7;
	float height = static_cast<float>(2 * (1 + data)) / 16.0f;
	setShape(0.0f, 0.0f, 0.0f, 1.0f, height, 1.0f);
}

void SnowTile::updateDefaultShape()
{
	setShape(0.0f, 0.0f, 0.0f, 1.0f, 2.0f / 16.0f, 1.0f);
}

bool SnowTile::canSnowStay(LevelSource &level, int_t x, int_t y, int_t z)
{
	int_t belowTile = level.getTile(x, y - 1, z);
	if (belowTile == 0 || !Tile::solid[belowTile])
		return false;

	const Material &belowMaterial = level.getMaterial(x, y - 1, z);
	return belowMaterial.blocksMotion() && &belowMaterial != &Material::ice();
}
