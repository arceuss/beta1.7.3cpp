#include "world/item/crafting/FurnaceRecipes.h"

#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/tile/Tile.h"

FurnaceRecipes &FurnaceRecipes::getInstance()
{
	static FurnaceRecipes instance;
	return instance;
}

FurnaceRecipes::FurnaceRecipes()
{
	recipes.emplace(Tile::ironOre.id, ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0));
	recipes.emplace(Tile::cobblestone.id, ItemInstance(1, 1, 0));
}

ItemInstance FurnaceRecipes::getResult(const ItemInstance &input) const
{
	auto it = recipes.find(input.itemID);
	if (it == recipes.end())
		return ItemInstance();
	return it->second;
}
