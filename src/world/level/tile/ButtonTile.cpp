#include "world/level/tile/ButtonTile.h"

#include "world/level/Level.h"
#include "world/level/material/Material.h"

// b173: BlockButton - Material.circuits, ticking, 4 wall orientations, powered state in bit 3

ButtonTile::ButtonTile(int_t id, int_t tex) : Tile(id, tex, Material::circuits())
{
	setTicking(true);
	updateCachedProperties();
}

// b173: getOrientation — scan the four walls, defaulting to orientation 1 like
// the reference does when nothing supports the button
int_t ButtonTile::getOrientation(Level &level, int_t x, int_t y, int_t z)
{
	if (level.isBlockNormalCube(x - 1, y, z)) return 1; // attach east wall → orient 1
	if (level.isBlockNormalCube(x + 1, y, z)) return 2; // attach west wall → orient 2
	if (level.isBlockNormalCube(x, y, z - 1)) return 3; // attach south wall → orient 3
	if (level.isBlockNormalCube(x, y, z + 1)) return 4; // attach north wall → orient 4
	return 1;
}

bool ButtonTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt — at least one wall face must be a normal cube
	if (level.isBlockNormalCube(x - 1, y, z))
		return true;
	if (level.isBlockNormalCube(x + 1, y, z))
		return true;
	if (level.isBlockNormalCube(x, y, z - 1))
		return true;
	return level.isBlockNormalCube(x, y, z + 1);
}

bool ButtonTile::mayPlaceOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	// b173: canPlaceBlockOnSide — the clicked wall itself must be a normal cube,
	// so floor and ceiling clicks are refused
	if (face == Facing::NORTH && level.isBlockNormalCube(x, y, z + 1))
		return true;
	if (face == Facing::SOUTH && level.isBlockNormalCube(x, y, z - 1))
		return true;
	if (face == Facing::WEST && level.isBlockNormalCube(x + 1, y, z))
		return true;
	return face == Facing::EAST && level.isBlockNormalCube(x - 1, y, z);
}

void ButtonTile::setPlacedOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	// b173: onBlockPlaced — the clicked wall decides the orientation only when it
	// actually supports the button; the powered bit survives
	int_t data = level.getData(x, y, z);
	int_t powered = data & 8;
	int_t orient;

	if (face == Facing::NORTH && level.isBlockNormalCube(x, y, z + 1))
		orient = 4;
	else if (face == Facing::SOUTH && level.isBlockNormalCube(x, y, z - 1))
		orient = 3;
	else if (face == Facing::WEST && level.isBlockNormalCube(x + 1, y, z))
		orient = 2;
	else if (face == Facing::EAST && level.isBlockNormalCube(x - 1, y, z))
		orient = 1;
	else
		orient = getOrientation(level, x, y, z);

	level.setData(x, y, z, orient + powered);
}

bool ButtonTile::checkCanSurvive(Level &level, int_t x, int_t y, int_t z)
{
	if (!mayPlace(level, x, y, z))
	{
		spawnResources(level, x, y, z, level.getData(x, y, z));
		level.setTile(x, y, z, 0);
		return false;
	}
	return true;
}

void ButtonTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
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

		if (drop)
		{
			spawnResources(level, x, y, z, level.getData(x, y, z));
			level.setTile(x, y, z, 0);
		}
	}
}

bool ButtonTile::use(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	(void)player;
	int_t data = level.getData(x, y, z);

	// Already powered — do nothing
	if ((data & 8) != 0)
		return true;

	level.setData(x, y, z, data | 8);
	level.setTilesDirty(x, y, z, x, y, z);
	level.playSoundEffect(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, u"random.click", 0.3f, 0.6f);
	level.notifyBlocksOfNeighborChange(x, y, z, id);
	int_t orient = data & 7;
	if (orient == 1) level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
	else if (orient == 2) level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
	else if (orient == 3) level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
	else if (orient == 4) level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
	else level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
	level.scheduleBlockUpdate(x, y, z, id, getTickDelay());

	return true;
}

void ButtonTile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	use(level, x, y, z, player);
}

void ButtonTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	(void)random;

	// b173: updateTick - the release is a server-side transition
	if (level.isOnline)
		return;

	int_t data = level.getData(x, y, z);

	// Only act if still powered
	if ((data & 8) == 0)
		return;

	// Clear powered bit
	level.setData(x, y, z, data & 7);

	// Notify neighbors
	level.notifyBlocksOfNeighborChange(x, y, z, id);
	int_t orient = data & 7;
	if (orient == 1) level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
	else if (orient == 2) level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
	else if (orient == 3) level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
	else if (orient == 4) level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
	else level.notifyBlocksOfNeighborChange(x, y - 1, z, id);

	// Click off sound
	level.playSoundEffect(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, u"random.click", 0.3f, 0.5f);
	level.setTilesDirty(x, y, z, x, y, z);
}

bool ButtonTile::getSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	(void)dir;
	return (level.getData(x, y, z) & 8) != 0;
}

bool ButtonTile::getDirectSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	int_t data = level.getData(x, y, z);
	if ((data & 8) == 0)
		return false;

	// b1.2 ButtonTile.getDirectSignal - strong signal toward the attached wall
	int_t orient = data & 7;
	if (orient == 5 && dir == 1) return true;
	if (orient == 4 && dir == 2) return true;
	if (orient == 3 && dir == 3) return true;
	if (orient == 2 && dir == 4) return true;
	if (orient == 1 && dir == 5) return true;
	return false;
}

void ButtonTile::onRemove(Level &level, int_t x, int_t y, int_t z)
{
	int_t data = level.getData(x, y, z);
	if ((data & 8) != 0)
	{
		level.notifyBlocksOfNeighborChange(x, y, z, id);
		int_t orient = data & 7;
		if (orient == 1) level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
		else if (orient == 2) level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
		else if (orient == 3) level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
		else if (orient == 4) level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
		else level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
	}
}

void ButtonTile::updateShape(LevelSource &level, int_t x, int_t y, int_t z)
{
	// b173: setBlockBoundsBasedOnState — thin box against the wall
	int_t data = level.getData(x, y, z);
	int_t orient = data & 7;
	bool powered = (data & 8) != 0;

	float y0 = 6.0f / 16.0f;
	float y1 = 10.0f / 16.0f;
	float halfWidth = 3.0f / 16.0f;
	float depth = powered ? 1.0f / 16.0f : 2.0f / 16.0f;
	if (orient == 1)
		setShape(0.0f, y0, 0.5f - halfWidth, depth, y1, 0.5f + halfWidth);
	else if (orient == 2)
		setShape(1.0f - depth, y0, 0.5f - halfWidth, 1.0f, y1, 0.5f + halfWidth);
	else if (orient == 3)
		setShape(0.5f - halfWidth, y0, 0.0f, 0.5f + halfWidth, y1, depth);
	else if (orient == 4)
		setShape(0.5f - halfWidth, y0, 1.0f - depth, 0.5f + halfWidth, y1, 1.0f);
}

void ButtonTile::updateDefaultShape()
{
	float width = 3.0f / 16.0f;
	float height = 2.0f / 16.0f;
	float depth = 2.0f / 16.0f;
	setShape(0.5f - width, 0.5f - height, 0.5f - depth, 0.5f + width, 0.5f + height, 0.5f + depth);
}