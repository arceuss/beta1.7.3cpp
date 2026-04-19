#include "world/level/tile/DispenserTile.h"

#include "client/player/LocalPlayer.h"
#include "util/Mth.h"
#include "util/Memory.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/player/Player.h"
#include "world/item/ItemInstance.h"
#include "world/level/Level.h"
#include "world/level/tile/entity/DispenserTileEntity.h"

DispenserTile::DispenserTile(int_t id, int_t tex, const Material &material) : Tile(id, tex, material)
{
	Tile::isEntityTile[id] = true;
}

int_t DispenserTile::getTexture(Facing face, int_t data)
{
	if (face == Facing::UP || face == Facing::DOWN)
		return tex + 17;
	return face == static_cast<Facing>(data) ? tex + 1 : tex;
}

void DispenserTile::onPlace(Level &level, int_t x, int_t y, int_t z)
{
	if (level.getTileEntity(x, y, z) == nullptr)
		level.setTileEntity(x, y, z, Util::make_shared<DispenserTileEntity>());
	setDefaultDirection(level, x, y, z);
}

void DispenserTile::onRemove(Level &level, int_t x, int_t y, int_t z)
{
	dropContents(level, x, y, z);
	level.removeTileEntity(x, y, z);
}

bool DispenserTile::use(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	LocalPlayer *localPlayer = dynamic_cast<LocalPlayer *>(&player);
	if (localPlayer == nullptr)
		return false;
	auto dispenser = std::dynamic_pointer_cast<DispenserTileEntity>(level.getTileEntity(x, y, z));
	if (dispenser == nullptr)
		return false;
	localPlayer->startDispenser(dispenser);
	return true;
}

void DispenserTile::setPlacedBy(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	int_t rotation = Mth::floor(static_cast<double>(player.yRot) * 4.0 / 360.0 + 0.5) & 3;
	if (rotation == 0) level.setData(x, y, z, 2);
	if (rotation == 1) level.setData(x, y, z, 5);
	if (rotation == 2) level.setData(x, y, z, 3);
	if (rotation == 3) level.setData(x, y, z, 4);
}

void DispenserTile::setDefaultDirection(Level &level, int_t x, int_t y, int_t z) const
{
	if (level.isOnline)
		return;
	int_t north = level.getTile(x, y, z - 1);
	int_t south = level.getTile(x, y, z + 1);
	int_t west = level.getTile(x - 1, y, z);
	int_t east = level.getTile(x + 1, y, z);
	int_t data = 3;
	if (Tile::solid[north] && !Tile::solid[south]) data = 3;
	if (Tile::solid[south] && !Tile::solid[north]) data = 2;
	if (Tile::solid[west] && !Tile::solid[east]) data = 5;
	if (Tile::solid[east] && !Tile::solid[west]) data = 4;
	level.setData(x, y, z, data);
}

void DispenserTile::dropContents(Level &level, int_t x, int_t y, int_t z) const
{
	auto dispenser = std::dynamic_pointer_cast<DispenserTileEntity>(level.getTileEntity(x, y, z));
	if (dispenser == nullptr)
		return;
	for (int_t slot = 0; slot < dispenser->getContainerSize(); ++slot)
	{
		ItemInstance &stack = dispenser->getItem(slot);
		if (stack.isEmpty())
			continue;
		float xo = level.random.nextFloat() * 0.8f + 0.1f;
		float yo = level.random.nextFloat() * 0.8f + 0.1f;
		float zo = level.random.nextFloat() * 0.8f + 0.1f;
		while (!stack.isEmpty())
		{
			int_t dropCount = level.random.nextInt(21) + 10;
			if (dropCount > stack.stackSize)
				dropCount = stack.stackSize;
			ItemInstance dropped = stack.remove(dropCount);
			auto entity = std::make_shared<EntityItem>(level, static_cast<double>(x) + xo, static_cast<double>(y) + yo, static_cast<double>(z) + zo, dropped);
			float spread = 0.05f;
			entity->xd = (level.random.nextFloat() * 2.0f - 1.0f) * spread;
			entity->yd = (level.random.nextFloat() * 2.0f - 1.0f) * spread + 0.2f;
			entity->zd = (level.random.nextFloat() * 2.0f - 1.0f) * spread;
			level.addEntity(entity);
		}
	}
}