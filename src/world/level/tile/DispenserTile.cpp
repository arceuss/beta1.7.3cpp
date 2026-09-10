#include "world/level/tile/DispenserTile.h"

#include "client/player/LocalPlayer.h"
#include "util/Mth.h"
#include "util/Memory.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/player/Player.h"
#include "world/entity/projectile/EntityArrow.h"
#include "world/entity/projectile/EntitySnowball.h"
#include "world/entity/projectile/EntityThrownEgg.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/tile/entity/DispenserTileEntity.h"
#include "java/Random.h"

	// DispenserTile.java:88-137 (BlockDispenser.dispenseItem)
	static void fireItem(Level &level, int_t x, int_t y, int_t z, Random &random)
	{
		int_t data = level.getData(x, y, z);
		int_t dirX = 0;
		int_t dirZ = 0;
		if (data == 3)
			dirZ = 1;
		else if (data == 2)
			dirZ = -1;
		else
			dirX = data == 5 ? 1 : -1;

		auto dispenser = std::dynamic_pointer_cast<DispenserTileEntity>(level.getTileEntity(x, y, z));
		if (dispenser == nullptr)
			return;

		ItemInstance stack = dispenser->removeRandomItem();
		double px = static_cast<double>(x) + static_cast<double>(dirX) * 0.6 + 0.5;
		double py = static_cast<double>(y) + 0.5;
		double pz = static_cast<double>(z) + static_cast<double>(dirZ) * 0.6 + 0.5;
		if (stack.isEmpty())
		{
			level.levelEvent(1001, x, y, z, 0);
			return;
		}

		if (stack.itemID == Items::arrow->getShiftedIndex())
		{
			auto arrow = std::make_shared<EntityArrow>(level, px, py, pz);
			arrow->setArrowHeading(dirX, 0.1f, dirZ, 1.1f, 6.0f);
			arrow->doesArrowBelongToPlayer = true;
			level.addEntity(arrow);
			level.levelEvent(1002, x, y, z, 0);
		}
		else if (stack.itemID == Items::egg->getShiftedIndex())
		{
			auto egg = std::make_shared<EntityThrownEgg>(level, px, py, pz);
			egg->shoot(dirX, 0.1f, dirZ, 1.1f, 6.0f);
			level.addEntity(egg);
			level.levelEvent(1002, x, y, z, 0);
		}
		else if (stack.itemID == Items::snowball->getShiftedIndex())
		{
			auto snowball = std::make_shared<EntitySnowball>(level, px, py, pz);
			snowball->shoot(dirX, 0.1f, dirZ, 1.1f, 6.0f);
			level.addEntity(snowball);
			level.levelEvent(1002, x, y, z, 0);
		}
		else
		{
			auto entity = std::make_shared<EntityItem>(level, px, py - 0.3, pz, stack);
			double speed = random.nextDouble() * 0.1 + 0.2;
			entity->xd = static_cast<double>(dirX) * speed;
			entity->yd = 0.2f;
			entity->zd = static_cast<double>(dirZ) * speed;
			entity->xd += random.nextGaussian() * static_cast<double>(0.0075f) * 6.0;
			entity->yd += random.nextGaussian() * static_cast<double>(0.0075f) * 6.0;
			entity->zd += random.nextGaussian() * static_cast<double>(0.0075f) * 6.0;
			level.addEntity(entity);
			level.levelEvent(1000, x, y, z, 0);
		}
		level.levelEvent(2000, x, y, z, dirX + 1 + (dirZ + 1) * 3);
	}

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
	if (level.isOnline)
		return true;
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

void DispenserTile::dropContents(Level &level, int_t x, int_t y, int_t z)
{
	auto dispenser = std::dynamic_pointer_cast<DispenserTileEntity>(level.getTileEntity(x, y, z));
	if (dispenser == nullptr)
		return;
	for (int_t slot = 0; slot < dispenser->getContainerSize(); ++slot)
	{
		ItemInstance &stack = dispenser->getItem(slot);
		if (stack.isEmpty())
			continue;
		float xo = random.nextFloat() * 0.8f + 0.1f;
		float yo = random.nextFloat() * 0.8f + 0.1f;
		float zo = random.nextFloat() * 0.8f + 0.1f;
		while (stack.stackSize > 0)
		{
			int_t dropCount = random.nextInt(21) + 10;
			if (dropCount > stack.stackSize)
				dropCount = stack.stackSize;
			stack.stackSize -= dropCount;
			ItemInstance dropped(stack.itemID, dropCount, stack.itemDamage);
			auto entity = std::make_shared<EntityItem>(level, static_cast<float>(x) + xo, static_cast<float>(y) + yo, static_cast<float>(z) + zo, dropped);
			float velocity = 0.05f;
			entity->xd = static_cast<float>(random.nextGaussian()) * velocity;
			entity->yd = static_cast<float>(random.nextGaussian()) * velocity + 0.2f;
			entity->zd = static_cast<float>(random.nextGaussian()) * velocity;
			level.addEntity(entity);
		}
	}
}

void DispenserTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	if (tile > 0 && Tile::tiles[tile] != nullptr && Tile::tiles[tile]->isSignalSource())
	{
		bool powered = level.hasNeighborSignal(x, y, z) || level.hasNeighborSignal(x, y + 1, z);
		if (powered)
			level.scheduleBlockUpdate(x, y, z, id, getTickDelay());
	}
}

void DispenserTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	if (level.hasNeighborSignal(x, y, z) || level.hasNeighborSignal(x, y + 1, z))
		fireItem(level, x, y, z, random);
}