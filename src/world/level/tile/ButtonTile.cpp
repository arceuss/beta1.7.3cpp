#include "world/level/tile/ButtonTile.h"

#include "world/level/Level.h"
#include "world/level/material/Material.h"

// b173: BlockButton - Material.circuits, ticking, 4 wall orientations, powered state in bit 3

ButtonTile::ButtonTile(int_t id, int_t tex) : Tile(id, tex, Material::circuits())
{
	setTicking(true);
	updateCachedProperties();
}

// Scan 4 wall faces for a normal cube to attach to; returns orientation 1-4 or -1
int_t ButtonTile::getOrientation(Level &level, int_t x, int_t y, int_t z)
{
	if (level.isBlockNormalCube(x - 1, y, z)) return 1; // attach east wall → orient 1
	if (level.isBlockNormalCube(x + 1, y, z)) return 2; // attach west wall → orient 2
	if (level.isBlockNormalCube(x, y, z - 1)) return 3; // attach south wall → orient 3
	if (level.isBlockNormalCube(x, y, z + 1)) return 4; // attach north wall → orient 4
	return -1;
}

bool ButtonTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt — at least one wall face must be a normal cube
	return getOrientation(level, x, y, z) != -1;
}

void ButtonTile::setPlacedOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	// b173: onBlockPlaced — determine orientation from face clicked
	int_t orient = -1;
	if (face == Facing::NORTH) orient = 4; // attached to north face (+z neighbor)
	else if (face == Facing::SOUTH) orient = 3; // attached to south face (-z neighbor)
	else if (face == Facing::WEST) orient = 2; // attached to west face (+x neighbor)
	else if (face == Facing::EAST) orient = 1; // attached to east face (-x neighbor)

	// Floor/ceiling faces: try auto-detect
	if (orient == -1)
		orient = getOrientation(level, x, y, z);

	if (orient == -1)
	{
		// No valid face — drop the button
		spawnResources(level, x, y, z, 0);
		level.setTile(x, y, z, 0);
		return;
	}

	// Preserve powered bit (bit 3) if any, set orientation in low 3 bits
	int_t data = level.getData(x, y, z);
	level.setData(x, y, z, (data & 8) | orient);
}

void ButtonTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	(void)tile;
	int_t data = level.getData(x, y, z);
	int_t orient = data & 7;

	// Check if the block this button is attached to is still a normal cube
	bool supported = false;
	if (orient == 1) supported = level.isBlockNormalCube(x - 1, y, z);
	else if (orient == 2) supported = level.isBlockNormalCube(x + 1, y, z);
	else if (orient == 3) supported = level.isBlockNormalCube(x, y, z - 1);
	else if (orient == 4) supported = level.isBlockNormalCube(x, y, z + 1);

	if (!supported)
	{
		spawnResources(level, x, y, z, data);
		level.setTile(x, y, z, 0);
	}
}

bool ButtonTile::use(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	(void)player;
	int_t data = level.getData(x, y, z);

	// Already powered — do nothing
	if ((data & 8) != 0)
		return true;

	// Set powered bit
	level.setData(x, y, z, data | 8);

	// Notify neighbors including the block we're attached to
	level.notifyBlocksOfNeighborChange(x, y, z, id);
	int_t orient = data & 7;
	if (orient == 1) level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
	else if (orient == 2) level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
	else if (orient == 3) level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
	else if (orient == 4) level.notifyBlocksOfNeighborChange(x, y, z + 1, id);

	// Schedule deactivation tick
	level.scheduleBlockUpdate(x, y, z, id, getTickDelay());

	// Click on sound
	level.playSoundEffect(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, u"random.click", 0.3f, 0.6f);

	return true;
}

void ButtonTile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	use(level, x, y, z, player);
}

void ButtonTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	(void)random;
	int_t data = level.getData(x, y, z);

	// Only act if still powered
	if ((data & 8) == 0)
		return;

	// Clear powered bit
	level.setData(x, y, z, data & ~8);

	// Notify neighbors
	level.notifyBlocksOfNeighborChange(x, y, z, id);
	int_t orient = data & 7;
	if (orient == 1) level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
	else if (orient == 2) level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
	else if (orient == 3) level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
	else if (orient == 4) level.notifyBlocksOfNeighborChange(x, y, z + 1, id);

	// Click off sound
	level.playSoundEffect(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, u"random.click", 0.3f, 0.5f);
}

bool ButtonTile::isDirectSignalTo(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	(void)dir;
	return (level.getData(x, y, z) & 8) != 0;
}

bool ButtonTile::isIndirectSignalTo(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	int_t data = level.getData(x, y, z);
	if ((data & 8) == 0)
		return false;

	int_t orient = data & 7;
	// Signal goes toward the wall the button is attached to
	// orient 1 → east wall (dir 5 = EAST), orient 2 → west wall (dir 4 = WEST)
	// orient 3 → south wall (dir 3 = SOUTH), orient 4 → north wall (dir 2 = NORTH)
	if (orient == 1 && dir == 5) return true;
	if (orient == 2 && dir == 4) return true;
	if (orient == 3 && dir == 3) return true;
	if (orient == 4 && dir == 2) return true;
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