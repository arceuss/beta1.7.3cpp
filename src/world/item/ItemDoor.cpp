#include "world/item/ItemDoor.h"

#include "util/Mth.h"
#include "world/item/ItemInstance.h"
#include "world/entity/player/Player.h"
#include "world/level/Level.h"
#include "world/level/tile/DoorTile.h"

ItemDoor::ItemDoor(int_t baseId, DoorTile &doorTile) : Item(baseId), doorTile(doorTile)
{
	setMaxStackSize(1);
}

bool ItemDoor::useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const
{
	if (face != Facing::UP)
		return false;

	// DoorItem defers admission to DoorTile.mayPlace, which owns the build-height
	// guard, the normal-cube support test and both replaceable halves.
	y++;
	if (!doorTile.mayPlace(level, x, y, z))
		return false;

	int_t rotation = Mth::floor(static_cast<double>((player.yRot + 180.0f) * 4.0f / 360.0f) - 0.5) & 3;
	int_t xd = 0;
	int_t zd = 0;
	if (rotation == 0) zd = 1;
	if (rotation == 1) xd = -1;
	if (rotation == 2) zd = -1;
	if (rotation == 3) xd = 1;

	int_t solidA = (level.isBlockNormalCube(x - xd, y, z - zd) ? 1 : 0) + (level.isBlockNormalCube(x - xd, y + 1, z - zd) ? 1 : 0);
	int_t solidB = (level.isBlockNormalCube(x + xd, y, z + zd) ? 1 : 0) + (level.isBlockNormalCube(x + xd, y + 1, z + zd) ? 1 : 0);
	bool doorA = level.getTile(x - xd, y, z - zd) == doorTile.id || level.getTile(x - xd, y + 1, z - zd) == doorTile.id;
	bool doorB = level.getTile(x + xd, y, z + zd) == doorTile.id || level.getTile(x + xd, y + 1, z + zd) == doorTile.id;

	bool hingeRight = false;
	if (doorA && !doorB)
		hingeRight = true;
	else if (solidB > solidA)
		hingeRight = true;

	if (hingeRight)
	{
		rotation = (rotation - 1) & 3;
		rotation += 4;
	}

	level.noNeighborUpdate = true;
	level.setTileAndData(x, y, z, doorTile.id, rotation);
	level.setTileAndData(x, y + 1, z, doorTile.id, rotation + 8);
	level.noNeighborUpdate = false;
	level.notifyBlocksOfNeighborChange(x, y, z, doorTile.id);
	level.notifyBlocksOfNeighborChange(x, y + 1, z, doorTile.id);
	stack.stackSize--;
	return true;
}
