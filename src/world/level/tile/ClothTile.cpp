#include "world/level/tile/ClothTile.h"
#include "ClientTarget.h"

ClothTile::ClothTile(int_t id, int_t tex, const Material &material) : Tile(id, tex, material)
{
}

int_t ClothTile::getTexture(Facing face, int_t data)
{
	if (ClientTarget::isAlphaPlace() || data == 0)
		return tex;

	int_t inv = ~(data & 15);
	return 113 + ((inv & 8) >> 3) + (inv & 7) * 16;
}

int_t ClothTile::getSpawnResourcesAuxValue(int_t data)
{
	return data;
}
