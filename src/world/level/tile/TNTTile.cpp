#include "world/level/tile/TNTTile.h"

#include "world/level/Level.h"
#include "world/entity/PrimedTNT.h"
#include "world/entity/item/EntityItem.h"
#include "world/item/Items.h"
#include "world/item/ItemInstance.h"
#include "world/item/ItemFlintAndSteel.h"
#include "java/Random.h"

TNTTile::TNTTile(int_t id, int_t tex) : Tile(id, tex, Material::tnt)
{
	updateCachedProperties();
}

int_t TNTTile::getTexture(Facing face, int_t data)
{
	(void)data;
	if (face == Facing::DOWN)
		return tex + 2;
	if (face == Facing::UP)
		return tex + 1;
	return tex;
}

void TNTTile::onPlace(Level &level, int_t x, int_t y, int_t z)
{
	if (level.isBlockIndirectlyGettingPowered(x, y, z))
	{
		level.setTile(x, y, z, 0);
		onBlockDestroyedByExplosion(level, x, y, z);
	}
}

void TNTTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	(void)tile;
	if (level.isBlockIndirectlyGettingPowered(x, y, z))
	{
		level.setTile(x, y, z, 0);
		onBlockDestroyedByExplosion(level, x, y, z);
	}
}

void TNTTile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	ItemInstance *selected = player.getSelectedItem();
	if (selected != nullptr && selected->itemID == Items::flintAndSteel->getShiftedIndex())
	{
		level.setData(x, y, z, 1);
	}
	Tile::attack(level, x, y, z, player);
}

int_t TNTTile::getResource(int_t data, Random &random)
{
	(void)data;
	(void)random;
	return 0;
}

void TNTTile::playerDestroy(Level &level, int_t x, int_t y, int_t z, int_t data)
{
	if ((data & 1) == 0)
	{
		auto item = std::make_shared<EntityItem>(level, x + 0.5, y + 0.5, z + 0.5, ItemInstance(id, 1, 0));
		level.addEntity(item);
	}
	else
	{
		auto entity = std::make_shared<PrimedTNT>(level, x + 0.5, y + 0.5, z + 0.5);
		level.addEntity(entity);
		level.playSoundEffect(x + 0.5, y + 0.5, z + 0.5, u"random.fuse", 1.0f, 1.0f);
	}
}

void TNTTile::onBlockDestroyedByExplosion(Level &level, int_t x, int_t y, int_t z)
{
	auto entity = std::make_shared<PrimedTNT>(level, x + 0.5, y + 0.5, z + 0.5);
	entity->fuse = level.random.nextInt(entity->fuse / 4) + entity->fuse / 8;
	level.addEntity(entity);
}
