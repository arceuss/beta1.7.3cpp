#include "world/level/tile/SlabTile.h"
#include "world/level/Level.h"
#include "world/level/LevelSource.h"

namespace
{
	constexpr int_t STONE_SIDE_TEXTURE = 5;
	constexpr int_t STONE_TOP_TEXTURE = 6;
	constexpr int_t SANDSTONE_SIDE_TEXTURE = 192;
	constexpr int_t SANDSTONE_TOP_TEXTURE = 176;
	constexpr int_t SANDSTONE_BOTTOM_TEXTURE = 208;
	constexpr int_t WOOD_TEXTURE = 4;
	constexpr int_t COBBLESTONE_TEXTURE = 16;
}

SlabTile::SlabTile(int_t id, bool fullBlock) : Tile(id, STONE_TOP_TEXTURE, Material::stone), fullBlock(fullBlock)
{
	if (!fullBlock)
		updateDefaultShape();
	setLightBlock(255);
}

int_t SlabTile::getTexture(Facing face, int_t data)
{
	switch (data & 3)
	{
	case 0:
		return (face == Facing::UP || face == Facing::DOWN) ? STONE_TOP_TEXTURE : STONE_SIDE_TEXTURE;
	case 1:
		if (face == Facing::DOWN)
			return SANDSTONE_BOTTOM_TEXTURE;
		if (face == Facing::UP)
			return SANDSTONE_TOP_TEXTURE;
		return SANDSTONE_SIDE_TEXTURE;
	case 2:
		return WOOD_TEXTURE;
	case 3:
	default:
		return COBBLESTONE_TEXTURE;
	}
}

bool SlabTile::isCubeShaped()
{
	return fullBlock;
}

bool SlabTile::isSolidRender()
{
	return fullBlock;
}

bool SlabTile::shouldRenderFace(LevelSource &level, int_t x, int_t y, int_t z, Facing face)
{
	if (fullBlock)
		return Tile::shouldRenderFace(level, x, y, z, face);
	if (face == Facing::UP || face == Facing::DOWN)
		return true;
	if (!Tile::shouldRenderFace(level, x, y, z, face))
		return false;
	return level.getTile(x, y, z) != id;
}

void SlabTile::onPlace(Level &level, int_t x, int_t y, int_t z)
{
	if (fullBlock)
		return;

	int_t data = level.getData(x, y, z) & 3;
	if (level.getTile(x, y - 1, z) != Tile::slabSingle.id)
		return;
	if ((level.getData(x, y - 1, z) & 3) != data)
		return;

	level.setTile(x, y, z, 0);
	level.setTileAndData(x, y - 1, z, Tile::slabDouble.id, data);
}

int_t SlabTile::getResourceCount(Random &random)
{
	(void)random;
	return fullBlock ? 2 : 1;
}

int_t SlabTile::getResource(int_t data, Random &random)
{
	(void)data;
	(void)random;
	return Tile::slabSingle.id;
}

int_t SlabTile::getSpawnResourcesAuxValue(int_t data)
{
	return data & 3;
}

void SlabTile::updateDefaultShape()
{
	if (fullBlock)
		setShape(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	else
		setShape(0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f);
}
