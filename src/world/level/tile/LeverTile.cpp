#include "world/level/tile/LeverTile.h"

#include "world/level/Level.h"
#include "world/level/material/Material.h"

// b173: BlockLever - Material.circuits, render type 12 (SHAPE_LEVER)

LeverTile::LeverTile(int_t id, int_t tex) : Tile(id, tex, Material::circuits())
{
	setDestroyTime(0.5f);
	updateCachedProperties();
}

bool LeverTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt - any of the four walls or the floor may hold a lever
	if (level.isBlockNormalCube(x - 1, y, z))
		return true;
	if (level.isBlockNormalCube(x + 1, y, z))
		return true;
	if (level.isBlockNormalCube(x, y, z - 1))
		return true;
	if (level.isBlockNormalCube(x, y, z + 1))
		return true;
	return level.isBlockNormalCube(x, y - 1, z);
}

bool LeverTile::mayPlaceOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	// b173: canPlaceBlockOnSide - only the clicked face's own support counts
	if (face == Facing::UP && level.isBlockNormalCube(x, y - 1, z))
		return true;
	if (face == Facing::NORTH && level.isBlockNormalCube(x, y, z + 1))
		return true;
	if (face == Facing::SOUTH && level.isBlockNormalCube(x, y, z - 1))
		return true;
	if (face == Facing::WEST && level.isBlockNormalCube(x + 1, y, z))
		return true;
	return face == Facing::EAST && level.isBlockNormalCube(x - 1, y, z);
}

void LeverTile::setPlacedOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	// b173: onBlockPlaced - the clicked face alone decides the orientation, and
	// only the floor orientation consumes a random draw
	int_t powered = level.getData(x, y, z) & 8;
	int_t orient = -1;

	if (face == Facing::UP && level.isBlockNormalCube(x, y - 1, z))
		orient = 5 + level.random.nextInt(2);
	if (face == Facing::NORTH && level.isBlockNormalCube(x, y, z + 1))
		orient = 4;
	if (face == Facing::SOUTH && level.isBlockNormalCube(x, y, z - 1))
		orient = 3;
	if (face == Facing::WEST && level.isBlockNormalCube(x + 1, y, z))
		orient = 2;
	if (face == Facing::EAST && level.isBlockNormalCube(x - 1, y, z))
		orient = 1;

	if (orient == -1)
	{
		spawnResources(level, x, y, z, level.getData(x, y, z));
		level.setTile(x, y, z, 0);
		return;
	}

	level.setData(x, y, z, orient + powered);
}

bool LeverTile::checkCanSurvive(Level &level, int_t x, int_t y, int_t z)
{
	if (!mayPlace(level, x, y, z))
	{
		spawnResources(level, x, y, z, level.getData(x, y, z));
		level.setTile(x, y, z, 0);
		return false;
	}
	return true;
}

void LeverTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	(void)tile;

	if (checkCanSurvive(level, x, y, z))
	{
		int_t orient = level.getData(x, y, z) & 7;
		bool drop = false;

		if (!level.isBlockNormalCube(x - 1, y, z) && orient == 1)
			drop = true;
		if (!level.isBlockNormalCube(x + 1, y, z) && orient == 2)
			drop = true;
		if (!level.isBlockNormalCube(x, y, z - 1) && orient == 3)
			drop = true;
		if (!level.isBlockNormalCube(x, y, z + 1) && orient == 4)
			drop = true;
		if (!level.isBlockNormalCube(x, y - 1, z) && orient == 5)
			drop = true;
		if (!level.isBlockNormalCube(x, y - 1, z) && orient == 6)
			drop = true;

		if (drop)
		{
			spawnResources(level, x, y, z, level.getData(x, y, z));
			level.setTile(x, y, z, 0);
		}
	}
}

bool LeverTile::use(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	(void)player;
	if (level.isOnline)
		return true;

	int_t data = level.getData(x, y, z);
	int_t orient = data & 7;
	int_t powered = 8 - (data & 8);
	level.setData(x, y, z, orient | powered);
	level.setTilesDirty(x, y, z, x, y, z);
	level.playSoundEffect(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, u"random.click", 0.3f, powered > 0 ? 0.6f : 0.5f);
	level.notifyBlocksOfNeighborChange(x, y, z, id);
	if (orient == 1) level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
	else if (orient == 2) level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
	else if (orient == 3) level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
	else if (orient == 4) level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
	else level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
	return true;
}

void LeverTile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	// b173: onBlockClicked - same as blockActivated
	use(level, x, y, z, player);
}

bool LeverTile::getSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	(void)dir;
	// b173: isPoweringTo - powered bit set
	return (level.getData(x, y, z) & 8) > 0;
}

bool LeverTile::getDirectSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	// b173: isIndirectlyPoweringTo - powered and direction matches orientation.
	// Both floor orientations (5 and 6) power downward through direction 1.
	int_t data = level.getData(x, y, z);
	if ((data & 8) == 0)
		return false;

	int_t orient = data & 7;
	if (orient == 6 && dir == 1) return true;
	if (orient == 5 && dir == 1) return true;
	if (orient == 4 && dir == 2) return true;
	if (orient == 3 && dir == 3) return true;
	if (orient == 2 && dir == 4) return true;
	return orient == 1 && dir == 5;
}

void LeverTile::onRemove(Level &level, int_t x, int_t y, int_t z)
{
	// b173: onBlockRemoval - notify neighbors if was powered
	if ((level.getData(x, y, z) & 8) > 0)
	{
		level.notifyBlocksOfNeighborChange(x, y, z, id);
		int_t orient = level.getData(x, y, z) & 7;
		if (orient == 1) level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
		else if (orient == 2) level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
		else if (orient == 3) level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
		else if (orient == 4) level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
		else level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
	}
}

void LeverTile::updateShape(LevelSource &level, int_t x, int_t y, int_t z)
{
	// b173: setBlockBoundsBasedOnState - per-orientation AABBs; the floor
	// orientations use a wider half-width than the wall ones
	int_t orient = level.getData(x, y, z) & 7;
	float f = 0.1875f;

	if (orient == 1)
		setShape(0.0f, 0.2f, 0.5f - f, f * 2.0f, 0.8f, 0.5f + f);
	else if (orient == 2)
		setShape(1.0f - f * 2.0f, 0.2f, 0.5f - f, 1.0f, 0.8f, 0.5f + f);
	else if (orient == 3)
		setShape(0.5f - f, 0.2f, 0.0f, 0.5f + f, 0.8f, f * 2.0f);
	else if (orient == 4)
		setShape(0.5f - f, 0.2f, 1.0f - f * 2.0f, 0.5f + f, 0.8f, 1.0f);
	else
	{
		f = 0.25f;
		setShape(0.5f - f, 0.0f, 0.5f - f, 0.5f + f, 0.6f, 0.5f + f);
	}
}
