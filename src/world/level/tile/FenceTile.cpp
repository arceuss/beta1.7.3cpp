#include "world/level/tile/FenceTile.h"

FenceTile::FenceTile(int_t id, int_t tex, const Material &material) : Tile(id, tex, material)
{
	setShape(0.0f, 0.0f, 0.0f, 1.0f, 1.5f, 1.0f);
	updateCachedProperties();
}

bool FenceTile::isCubeShaped()
{
	return false;
}

bool FenceTile::isSolidRender()
{
	return false;
}

Tile::Shape FenceTile::getRenderShape()
{
	return SHAPE_FENCE;
}