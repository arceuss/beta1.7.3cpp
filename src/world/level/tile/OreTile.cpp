#include "world/level/tile/OreTile.h"
#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/tile/Tile.h"

OreTile::OreTile(int_t id, int_t tex) : Tile(id, tex, Material::stone)
{
}

int_t OreTile::getResource(int_t data, Random &random)
{
	if (id == Tile::coalOre.id)
		return Items::coal->getShiftedIndex();
	if (id == Tile::diamondOre.id)
		return Items::diamond->getShiftedIndex();
	if (id == Tile::lapisOre.id)
		return Items::dyePowder->getShiftedIndex();
	return id; // iron ore and gold ore drop themselves
}

int_t OreTile::getResourceCount(Random &random)
{
	if (id == Tile::lapisOre.id)
		return 4 + random.nextInt(5);
	return 1;
}

int_t OreTile::getSpawnResourcesAuxValue(int_t data)
{
	if (id == Tile::lapisOre.id)
		return 4; // blue dye metadata
	return 0;
}