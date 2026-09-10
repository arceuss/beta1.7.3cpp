#include "world/level/tile/TrapDoorTile.h"

#include "world/level/Level.h"

TrapDoorTile::TrapDoorTile(int_t id, int_t tex, const Material &material) : Tile(id, tex, material)
{
	updateCachedProperties();
}

bool TrapDoorTile::isOpen(int_t data)
{
	return (data & 4) != 0;
}

void TrapDoorTile::setShapeForData(int_t data)
{
	float thickness = 3.0f / 16.0f;
	setShape(0.0f, 0.0f, 0.0f, 1.0f, thickness, 1.0f);
	if (isOpen(data))
	{
		if ((data & 3) == 0)
			setShape(0.0f, 0.0f, 1.0f - thickness, 1.0f, 1.0f, 1.0f);
		if ((data & 3) == 1)
			setShape(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, thickness);
		if ((data & 3) == 2)
			setShape(1.0f - thickness, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
		if ((data & 3) == 3)
			setShape(0.0f, 0.0f, 0.0f, thickness, 1.0f, 1.0f);
	}
}

bool TrapDoorTile::isCubeShaped()
{
	return false;
}

bool TrapDoorTile::isSolidRender()
{
	return false;
}

AABB *TrapDoorTile::getAABB(Level &level, int_t x, int_t y, int_t z)
{
	updateShape(level, x, y, z);
	return Tile::getAABB(level, x, y, z);
}

AABB *TrapDoorTile::getTileAABB(Level &level, int_t x, int_t y, int_t z)
{
	updateShape(level, x, y, z);
	return Tile::getTileAABB(level, x, y, z);
}

void TrapDoorTile::updateShape(LevelSource &level, int_t x, int_t y, int_t z)
{
	setShapeForData(level.getData(x, y, z));
}

void TrapDoorTile::updateDefaultShape()
{
	// b173: setBlockBoundsForItemRender - the held/inventory slab sits centred on
	// the block middle, unlike the closed in-world shape that rests on the floor
	float thickness = 3.0f / 16.0f;
	setShape(0.0f, 0.5f - thickness / 2.0f, 0.0f, 1.0f, 0.5f + thickness / 2.0f, 1.0f);
}

void TrapDoorTile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	// b173: onBlockClicked - a left click toggles the trapdoor like a use
	use(level, x, y, z, player);
}

bool TrapDoorTile::use(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	level.setData(x, y, z, level.getData(x, y, z) ^ 4);
	level.levelEvent(&player, 1003, x, y, z, 0);
	return true;
}

void TrapDoorTile::setOpen(Level &level, int_t x, int_t y, int_t z, bool open)
{
	// b173: onPoweredBlockChange
	int_t data = level.getData(x, y, z);
	bool wasOpen = (data & 4) > 0;
	if (wasOpen == open)
		return;

	level.setData(x, y, z, data ^ 4);
	level.levelEvent(nullptr, 1003, x, y, z, 0);
}

void TrapDoorTile::setPlacedOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	// b173: onBlockPlaced - the clicked wall names the hinge side; admission is
	// the caller's job through mayPlaceOnFace
	int_t data = 0;
	if (face == Facing::NORTH)
		data = 0;
	if (face == Facing::SOUTH)
		data = 1;
	if (face == Facing::WEST)
		data = 2;
	if (face == Facing::EAST)
		data = 3;
	level.setData(x, y, z, data);
}

bool TrapDoorTile::mayPlaceOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	// b173: canPlaceBlockOnSide - floor and ceiling are refused outright
	if (face == Facing::DOWN)
		return false;
	if (face == Facing::UP)
		return false;
	if (face == Facing::NORTH)
		z++;
	if (face == Facing::SOUTH)
		z--;
	if (face == Facing::WEST)
		x++;
	if (face == Facing::EAST)
		x--;
	return level.isBlockNormalCube(x, y, z);
}

void TrapDoorTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	// b173: onNeighborBlockChange - server authority only
	if (level.isOnline)
		return;

	int_t data = level.getData(x, y, z);
	int_t sx = x;
	int_t sz = z;
	if ((data & 3) == 0)
		sz++;
	if ((data & 3) == 1)
		sz--;
	if ((data & 3) == 2)
		sx++;
	if ((data & 3) == 3)
		sx--;

	if (!level.isBlockNormalCube(sx, y, sz))
	{
		level.setTile(x, y, z, 0);
		spawnResources(level, x, y, z, data);
	}

	if (tile > 0 && Tile::tiles[tile] != nullptr && Tile::tiles[tile]->isSignalSource())
		setOpen(level, x, y, z, level.hasNeighborSignal(x, y, z));
}
