#include "world/level/tile/IceTile.h"

#include "world/level/material/Material.h"

IceTile::IceTile(int_t id, int_t tex) : TransparentTile(id, tex, Material::ice(), false)
{
	friction = 0.98f;
	updateCachedProperties();
}

int_t IceTile::getRenderLayer()
{
	return 1;
}

bool IceTile::isTranslucent()
{
	return true;
}
