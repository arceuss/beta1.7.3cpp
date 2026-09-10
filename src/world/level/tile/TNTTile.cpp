#include "world/level/tile/TNTTile.h"

#include "world/level/Level.h"
#include "world/entity/PrimedTNT.h"
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
	Tile::onPlace(level, x, y, z);
	if (level.hasNeighborSignal(x, y, z))
	{
		destroy(level, x, y, z, 1);
		level.setTile(x, y, z, 0);
	}
}

void TNTTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	if (tile > 0 && Tile::tiles[tile]->isSignalSource() && level.hasNeighborSignal(x, y, z))
	{
		destroy(level, x, y, z, 1);
		level.setTile(x, y, z, 0);
	}
}

void TNTTile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	ItemInstance *selected = player.getSelectedItem();
	if (selected != nullptr && selected->itemID == Items::flintAndSteel->getShiftedIndex())
	{
		// Silent write: arming TNT must not notify neighbours.
		level.setDataNoUpdate(x, y, z, 1);
	}
	Tile::attack(level, x, y, z, player);
}

int_t TNTTile::getResourceCount(Random &random)
{
	(void)random;
	return 0;
}

void TNTTile::destroy(Level &level, int_t x, int_t y, int_t z, int_t data)
{
	if (level.isOnline)
		return;

	if ((data & 1) == 0)
		popResource(level, x, y, z, ItemInstance(Tile::tnt.id, 1, 0));
	else
	{
		// Beta computes the spawn centre in float.
		auto entity = std::make_shared<PrimedTNT>(level,
			static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f);
		level.addEntity(entity);
		level.playSoundAtEntity(*entity, u"random.fuse", 1.0f, 1.0f);
	}
}

void TNTTile::onBlockDestroyedByExplosion(Level &level, int_t x, int_t y, int_t z)
{
	auto entity = std::make_shared<PrimedTNT>(level,
		static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f);
	entity->fuse = level.random.nextInt(entity->fuse / 4) + entity->fuse / 8;
	level.addEntity(entity);
}
