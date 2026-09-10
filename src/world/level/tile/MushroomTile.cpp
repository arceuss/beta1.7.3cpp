#include "world/level/tile/MushroomTile.h"

#include "world/level/Level.h"
#include "world/level/tile/Tile.h"

MushroomTile::MushroomTile(int_t id, int_t tex) : FlowerTile(id, tex)
{
	updateDefaultShape();
	setTicking(true);
}

void MushroomTile::updateDefaultShape()
{
	float radius = 0.2f;
	setShape(0.5f - radius, 0.0f, 0.5f - radius, 0.5f + radius, radius * 2.0f, 0.5f + radius);
}

// BlockMushroom.updateTick: spread only. Unlike the other bushes it never calls the
// FlowerTile survival check, so a dark mushroom is only removed by a neighbour update.
void MushroomTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	if (random.nextInt(100) != 0)
		return;

	// Java evaluates the three isAirBlock arguments left to right, so the draw order is
	// x (bound 3), y (bound 2 twice), z (bound 3). Each draw needs its own statement.
	int_t targetX = x + random.nextInt(3) - 1;
	int_t yUp = random.nextInt(2);
	int_t yDown = random.nextInt(2);
	int_t targetY = y + yUp - yDown;
	int_t targetZ = z + random.nextInt(3) - 1;
	if (!level.isEmptyTile(targetX, targetY, targetZ) || !canStay(level, targetX, targetY, targetZ))
		return;

	// Beta draws these two and discards them; they only shift the local copies of x and z.
	(void)random.nextInt(3);
	(void)random.nextInt(3);

	if (level.isEmptyTile(targetX, targetY, targetZ) && canStay(level, targetX, targetY, targetZ))
		level.setTile(targetX, targetY, targetZ, id);
}

bool MushroomTile::canSurviveOn(int_t belowTile) const
{
	return Tile::solid[belowTile];
}

bool MushroomTile::canStay(Level &level, int_t x, int_t y, int_t z)
{
	if (y < 0 || y >= Level::DEPTH)
		return false;
	return level.getFullBrightness(x, y, z) < 13 && canSurviveOn(level.getTile(x, y - 1, z));
}
