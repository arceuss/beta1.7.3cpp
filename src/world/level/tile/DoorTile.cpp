#include "world/level/tile/DoorTile.h"

#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/Level.h"

DoorTile::DoorTile(int_t id, int_t tex, const Material &material, bool iron) : Tile(id, tex, material), iron(iron)
{
	updateCachedProperties();
}

int_t DoorTile::getState(int_t data) const
{
	return (data & 4) == 0 ? ((data - 1) & 3) : (data & 3);
}

void DoorTile::setDoorRotation(int_t state)
{
	float thickness = 3.0f / 16.0f;
	setShape(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, thickness);
	if (state == 1)
		setShape(1.0f - thickness, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	if (state == 2)
		setShape(0.0f, 0.0f, 1.0f - thickness, 1.0f, 1.0f, 1.0f);
	if (state == 3)
		setShape(0.0f, 0.0f, 0.0f, thickness, 1.0f, 1.0f);
}

bool DoorTile::isCubeShaped()
{
	return false;
}

bool DoorTile::isSolidRender()
{
	return false;
}

Tile::Shape DoorTile::getRenderShape()
{
	return SHAPE_DOOR;
}

AABB *DoorTile::getAABB(Level &level, int_t x, int_t y, int_t z)
{
	updateShape(level, x, y, z);
	return Tile::getAABB(level, x, y, z);
}

AABB *DoorTile::getTileAABB(Level &level, int_t x, int_t y, int_t z)
{
	updateShape(level, x, y, z);
	return Tile::getTileAABB(level, x, y, z);
}

void DoorTile::updateShape(LevelSource &level, int_t x, int_t y, int_t z)
{
	setDoorRotation(getState(level.getData(x, y, z)));
}

void DoorTile::updateDefaultShape()
{
	setDoorRotation(0);
}

int_t DoorTile::getTexture(Facing face, int_t data)
{
	if (face == Facing::DOWN || face == Facing::UP)
		return tex;

	int_t state = getState(data);
	bool northSouthFace = face == Facing::NORTH || face == Facing::SOUTH;
	if (((state == 0 || state == 2) ^ northSouthFace))
	{
		return tex;
	}

	int_t variant = state / 2 + ((static_cast<int_t>(face) & 1) ^ state);
	variant += (data & 4) / 4;
	int_t result = tex - (data & 8) * 2;
	if ((variant & 1) != 0)
		result = -result;
	return result;
}

void DoorTile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	// b173: onBlockClicked - a left click runs the same toggle as a right click,
	// and the iron refusal stays inside use
	use(level, x, y, z, player);
}

bool DoorTile::use(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	if (iron)
		return true;

	int_t data = level.getData(x, y, z);
	if ((data & 8) != 0)
	{
		if (level.getTile(x, y - 1, z) == id)
			use(level, x, y - 1, z, player);
		return true;
	}

	// b173: blockActivated - the top half is written first and every write
	// notifies, so neighbours observe the halves in the reference order
	if (level.getTile(x, y + 1, z) == id)
		level.setData(x, y + 1, z, (data ^ 4) + 8);
	level.setData(x, y, z, data ^ 4);
	level.setTilesDirty(x, y - 1, z, x, y, z);
	level.levelEvent(&player, 1003, x, y, z, 0);
	return true;
}

void DoorTile::setOpen(Level &level, int_t x, int_t y, int_t z, bool open)
{
	// b173: onPoweredBlockChange
	int_t data = level.getData(x, y, z);
	if ((data & 8) != 0)
	{
		if (level.getTile(x, y - 1, z) == id)
			setOpen(level, x, y - 1, z, open);
		return;
	}

	bool wasOpen = (level.getData(x, y, z) & 4) > 0;
	if (wasOpen == open)
		return;

	if (level.getTile(x, y + 1, z) == id)
		level.setData(x, y + 1, z, (data ^ 4) + 8);
	level.setData(x, y, z, data ^ 4);
	level.setTilesDirty(x, y - 1, z, x, y, z);
	level.levelEvent(nullptr, 1003, x, y, z, 0);
}

void DoorTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	int_t data = level.getData(x, y, z);
	if ((data & 8) != 0)
	{
		if (level.getTile(x, y - 1, z) != id)
			level.setTile(x, y, z, 0);
		if (tile > 0 && Tile::tiles[tile] != nullptr && Tile::tiles[tile]->isSignalSource() && level.getTile(x, y - 1, z) == id)
			neighborChanged(level, x, y - 1, z, tile);
		return;
	}

	// b173: onNeighborBlockChange - both halves are removed with notifying
	// writes, and only the server drops the door item
	bool removed = false;
	if (level.getTile(x, y + 1, z) != id)
	{
		level.setTile(x, y, z, 0);
		removed = true;
	}

	if (!level.isBlockNormalCube(x, y - 1, z))
	{
		level.setTile(x, y, z, 0);
		removed = true;
		if (level.getTile(x, y + 1, z) == id)
			level.setTile(x, y + 1, z, 0);
	}

	if (removed)
	{
		if (!level.isOnline)
			spawnResources(level, x, y, z, data);
	}
	else if (tile > 0 && Tile::tiles[tile] != nullptr && Tile::tiles[tile]->isSignalSource())
	{
		bool powered = level.hasNeighborSignal(x, y, z) || level.hasNeighborSignal(x, y + 1, z);
		setOpen(level, x, y, z, powered);
	}
}

bool DoorTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt - needs a normal cube below and two free cells,
	// and the top half may not reach the build ceiling
	if (y >= 127)
		return false;
	return level.isBlockNormalCube(x, y - 1, z) && Tile::mayPlace(level, x, y, z) && Tile::mayPlace(level, x, y + 1, z);
}

int_t DoorTile::getResource(int_t data, Random &random)
{
	(void)random;
	if ((data & 8) != 0)
		return 0;
	return iron ? Items::doorIron->getShiftedIndex() : Items::doorWood->getShiftedIndex();
}