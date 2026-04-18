#include "world/level/tile/DeadBushTile.h"

#include "world/level/tile/SandTile.h"

DeadBushTile::DeadBushTile(int_t id, int_t tex) : FlowerTile(id, tex)
{
	updateDefaultShape();
}

int_t DeadBushTile::getResourceCount(Random &random)
{
	return 0;
}

void DeadBushTile::updateDefaultShape()
{
	float radius = 0.4f;
	setShape(0.5f - radius, 0.0f, 0.5f - radius, 0.5f + radius, 0.8f, 0.5f + radius);
}

bool DeadBushTile::canSurviveOn(int_t belowTile) const
{
	return belowTile == Tile::sand.id;
}