#include "world/level/tile/FenceTile.h"

#include "world/level/Level.h"
#include "world/level/material/Material.h"

FenceTile::FenceTile(int_t id, int_t tex, const Material &material) : Tile(id, tex, material)
{
	updateCachedProperties();
}

bool FenceTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt - a fence below always carries another fence,
	// otherwise the block below only needs a solid material, not a normal cube
	if (level.getTile(x, y - 1, z) == id)
		return true;
	if (!level.getMaterial(x, y - 1, z).isSolid())
		return false;
	return Tile::mayPlace(level, x, y, z);
}

bool FenceTile::isCubeShaped()
{
	return false;
}

bool FenceTile::isSolidRender()
{
	return false;
}

Tile::Shape FenceTile::getRenderShape()
{
	return SHAPE_FENCE;
}

AABB *FenceTile::getAABB(Level &level, int_t x, int_t y, int_t z)
{
	return AABB::newTemp(x, y, z, x + 1, static_cast<float>(y) + 1.5f, z + 1);
}