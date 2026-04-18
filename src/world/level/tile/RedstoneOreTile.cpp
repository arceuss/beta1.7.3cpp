#include "world/level/tile/RedstoneOreTile.h"
#include "world/item/Item.h"
#include "world/item/Items.h"

RedstoneOreTile::RedstoneOreTile(int_t id, int_t tex) : Tile(id, tex, Material::stone)
{
}

int_t RedstoneOreTile::getResource(int_t data, Random &random)
{
	(void)data;
	(void)random;
	return Items::redstone->getShiftedIndex();
}

int_t RedstoneOreTile::getResourceCount(Random &random)
{
	return 4 + random.nextInt(2);
}