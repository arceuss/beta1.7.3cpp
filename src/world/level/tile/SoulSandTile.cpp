#include "world/level/tile/SoulSandTile.h"

#include "world/entity/Entity.h"

SoulSandTile::SoulSandTile(int_t id, int_t tex, const Material &material) : Tile(id, tex, material)
{
	setShape(0.0f, 0.0f, 0.0f, 1.0f, 0.875f, 1.0f);
}

void SoulSandTile::entityInside(Level &level, int_t x, int_t y, int_t z, Entity &entity)
{
	entity.xd *= 0.4;
	entity.zd *= 0.4;
}