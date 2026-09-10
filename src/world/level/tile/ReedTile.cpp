#include "world/level/tile/ReedTile.h"

#include "world/level/Level.h"

#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/material/Material.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/tile/GrassTile.h"
#include "world/level/tile/DirtTile.h"

ReedTile::ReedTile(int_t id, int_t tex) : Tile(id, tex, Material::plants())
{
	setTicking(true);
	updateDefaultShape();
	updateCachedProperties();
}

bool ReedTile::isCubeShaped()
{
	return false;
}

Tile::Shape ReedTile::getRenderShape()
{
	return SHAPE_CROSS_TEXTURE;
}

AABB *ReedTile::getAABB(Level &level, int_t x, int_t y, int_t z)
{
	return nullptr;
}

bool ReedTile::isSolidRender()
{
	return false;
}

void ReedTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
	{
		if (!level.isEmptyTile(x, y + 1, z))
			return;
	
		int_t height = 1;
		while (level.getTile(x, y - height, z) == id)
			++height;
	
		if (height >= 3)
			return;
	
		int_t data = level.getData(x, y, z);
		if (data == 15)
		{
			level.setTile(x, y + 1, z, id);
			level.setData(x, y, z, 0);
		}
		else
		{
			level.setData(x, y, z, data + 1);
		}
	}
	
void ReedTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	(void)tile;
	if (canStay(level, x, y, z))
		return;

	spawnResources(level, x, y, z, level.getData(x, y, z));
	level.setTile(x, y, z, 0);
}

void ReedTile::updateDefaultShape()
{
	float radius = 6.0f / 16.0f;
	setShape(0.5f - radius, 0.0f, 0.5f - radius, 0.5f + radius, 1.0f, 0.5f + radius);
}

bool ReedTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	int_t belowTile = level.getTile(x, y - 1, z);
	if (belowTile == id)
		return true;
	if (belowTile != Tile::grass.id && belowTile != Tile::dirt.id)
		return false;

	return &level.getMaterial(x - 1, y - 1, z) == &Material::water
		|| &level.getMaterial(x + 1, y - 1, z) == &Material::water
		|| &level.getMaterial(x, y - 1, z - 1) == &Material::water
		|| &level.getMaterial(x, y - 1, z + 1) == &Material::water;
}

bool ReedTile::canStay(Level &level, int_t x, int_t y, int_t z)
{
	return mayPlace(level, x, y, z);
}

int_t ReedTile::getResource(int_t data, Random &random)
{
	(void)data;
	(void)random;
	return Items::reed->getShiftedIndex();
}
