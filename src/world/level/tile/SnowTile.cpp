#include "world/level/tile/SnowTile.h"

#include "world/entity/item/EntityItem.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/LevelSource.h"
#include "world/level/material/Material.h"
#include "world/stats/StatList.h"

SnowTile::SnowTile(int_t id, int_t tex) : Tile(id, tex, Material::snow())
{
	setTicking(true);
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

bool SnowTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt - the supporting tile's live render opacity decides,
	// not the registration-time cache, so fast/fancy leaves answer differently
	int_t below = level.getTile(x, y - 1, z);
	Tile *belowTile = (below == 0) ? nullptr : Tile::tiles[below];
	if (belowTile == nullptr || !belowTile->isSolidRender())
		return false;
	return level.getMaterial(x, y - 1, z).blocksMotion();
}

void SnowTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	(void)tile;
	checkCanSurvive(level, x, y, z);
}

bool SnowTile::checkCanSurvive(Level &level, int_t x, int_t y, int_t z)
{
	if (!mayPlace(level, x, y, z))
	{
		spawnResources(level, x, y, z, level.getData(x, y, z));
		level.setTile(x, y, z, 0);
		return false;
	}
	return true;
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

void SnowTile::harvestBlock(Level &level, Player &player, int_t x, int_t y, int_t z, int_t data)
{
	(void)data;
	// b173: harvestBlock - the controller already decided the block was
	// harvestable, so the snowball drops without re-inspecting the held tool
	int_t snowballId = Items::snowball->getShiftedIndex();
	float spread = 0.7f;
	double xo = level.random.nextFloat() * spread + (1.0f - spread) * 0.5;
	double yo = level.random.nextFloat() * spread + (1.0f - spread) * 0.5;
	double zo = level.random.nextFloat() * spread + (1.0f - spread) * 0.5;
	auto entity = std::make_shared<EntityItem>(level, x + xo, y + yo, z + zo, ItemInstance(snowballId, 1, 0));
	entity->throwTime = 10;
	level.addEntity(entity);
	level.setTile(x, y, z, 0);
	if (StatBase *stat = StatList::mineBlockStats[id])
		player.addStat(*stat, 1);
}

int_t SnowTile::getResource(int_t data, Random &random)
{
	(void)data;
	(void)random;
	return Items::snowball->getShiftedIndex();
}

int_t SnowTile::getResourceCount(Random &random)
{
	(void)random;
	return 0;
}

bool SnowTile::shouldRenderFace(LevelSource &level, int_t x, int_t y, int_t z, Facing face)
{
	if (face == Facing::UP)
		return true;
	return Tile::shouldRenderFace(level, x, y, z, face);
}

void SnowTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	(void)random;
	if (level.getBrightness(LightLayer::Block, x, y, z) > 11)
	{
		spawnResources(level, x, y, z, level.getData(x, y, z));
		level.setTile(x, y, z, 0);
	}
}
