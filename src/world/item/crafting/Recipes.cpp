#include "world/item/crafting/Recipes.h"

#include <algorithm>
#include <unordered_map>

#include "world/item/crafting/CraftingContainer.h"
#include "world/item/Item.h"
#include "world/item/ItemPickaxe.h"
#include "world/item/Items.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/TreeTile.h"
#include "world/level/tile/WoodTile.h"
#include "world/level/tile/WorkbenchTile.h"
#include "world/level/tile/FurnaceTile.h"
#include "world/level/tile/SlabTile.h"
#include "world/level/tile/StoneTile.h"
#include "world/level/tile/TNTTile.h"
#include "world/level/tile/SandTile.h"
#include "world/item/ItemSign.h"
#include "world/level/tile/RailTile.h"
#include "world/level/tile/DetectorRailTile.h"
#include "world/level/tile/ChestTile.h"
#include "world/level/tile/ButtonTile.h"
#include "world/level/tile/LeverTile.h"
#include "world/level/tile/PressurePlateTile.h"
#include "world/level/tile/NotGateTile.h"
#include "world/level/tile/PistonBaseTile.h"
#include "world/level/tile/MushroomTile.h"
#include "world/level/tile/ClothTile.h"

namespace
{
	bool matchesAt(const Recipes::ShapedRecipe &recipe, const CraftingContainer &container, int_t offsetX, int_t offsetY, bool mirror)
	{
		for (int_t x = 0; x < 3; ++x)
		{
			for (int_t y = 0; y < 3; ++y)
			{
				int_t recipeX = x - offsetX;
				int_t recipeY = y - offsetY;
				ItemInstance recipeItem;
				if (recipeX >= 0 && recipeY >= 0 && recipeX < recipe.width && recipeY < recipe.height)
				{
					int_t index = mirror ? (recipe.width - recipeX - 1) + recipeY * recipe.width : recipeX + recipeY * recipe.width;
					recipeItem = recipe.items[index];
				}

				ItemInstance containerItem = container.getItem(x, y);
				if (recipeItem.isEmpty() && containerItem.isEmpty())
					continue;
				if (recipeItem.isEmpty() != containerItem.isEmpty())
					return false;
				if (recipeItem.itemID != containerItem.itemID)
					return false;
				if (recipeItem.itemDamage != -1 && recipeItem.itemDamage != containerItem.itemDamage)
					return false;
			}
		}
		return true;
	}

	bool matches(const Recipes::ShapedRecipe &recipe, const CraftingContainer &container)
	{
		for (int_t offsetX = 0; offsetX <= 3 - recipe.width; ++offsetX)
		{
			for (int_t offsetY = 0; offsetY <= 3 - recipe.height; ++offsetY)
			{
				if (matchesAt(recipe, container, offsetX, offsetY, false))
					return true;
				if (matchesAt(recipe, container, offsetX, offsetY, true))
					return true;
			}
		}
		return false;
	}
}

Recipes &Recipes::getInstance()
{
	static Recipes instance;
	return instance;
}

Recipes::Recipes()
{
	auto addToolRecipes = [&](const ItemInstance &material, Item &sword, Item &shovel, Item &pickaxe, Item &axe, Item &hoe) {
		addShapedRecipe(ItemInstance(sword.getShiftedIndex(), 1, 0), {"#", "#", "X"}, {
			{'#', material},
			{'X', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
		});
		addShapedRecipe(ItemInstance(shovel.getShiftedIndex(), 1, 0), {"#", "X", "X"}, {
			{'#', material},
			{'X', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
		});
		addShapedRecipe(ItemInstance(pickaxe.getShiftedIndex(), 1, 0), {"###", " X ", " X "}, {
			{'#', material},
			{'X', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
		});
		addShapedRecipe(ItemInstance(axe.getShiftedIndex(), 1, 0), {"##", "#X", " X"}, {
			{'#', material},
			{'X', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
		});
		addShapedRecipe(ItemInstance(hoe.getShiftedIndex(), 1, 0), {"##", " X", " X"}, {
			{'#', material},
			{'X', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
		});
	};

	addShapedRecipe(ItemInstance(Tile::wood.id, 4, 0), {"#"}, {{'#', ItemInstance(Tile::treeTrunk.id, 1, -1)}});
	addShapedRecipe(ItemInstance(Items::stick->getShiftedIndex(), 4, 0), {"#", "#"}, {{'#', ItemInstance(Tile::wood.id, 1, -1)}});
	addShapedRecipe(ItemInstance(Tile::workBench.id, 1, 0), {"##", "##"}, {{'#', ItemInstance(Tile::wood.id, 1, -1)}});
	addShapedRecipe(ItemInstance(Tile::furnace.id, 1, 0), {"###", "# #", "###"}, {{'#', ItemInstance(Tile::cobblestone.id, 1, -1)}});

	addShapedRecipe(ItemInstance(Tile::slabSingle.id, 3, 3), {"###"}, {{'#', ItemInstance(Tile::cobblestone.id, 1, -1)}});
	addShapedRecipe(ItemInstance(Tile::slabSingle.id, 3, 0), {"###"}, {{'#', ItemInstance(Tile::rock.id, 1, -1)}});
	addShapedRecipe(ItemInstance(Tile::slabSingle.id, 3, 1), {"###"}, {{'#', ItemInstance(Tile::sandstone.id, 1, -1)}});
	addShapedRecipe(ItemInstance(Tile::slabSingle.id, 3, 2), {"###"}, {{'#', ItemInstance(Tile::wood.id, 1, -1)}});

	addToolRecipes(ItemInstance(Tile::wood.id, 1, -1), *Items::swordWood, *Items::shovelWood, *Items::pickaxeWood, *Items::axeWood, *Items::hoeWood);
	addToolRecipes(ItemInstance(Tile::cobblestone.id, 1, -1), *Items::swordStone, *Items::shovelStone, *Items::pickaxeStone, *Items::axeStone, *Items::hoeStone);

	addShapedRecipe(ItemInstance(Items::bowlSoup->getShiftedIndex(), 1, 0), {"#", "X", "B"}, {
		{'#', ItemInstance(Tile::brownMushroom.id, 1, 0)},
		{'X', ItemInstance(Tile::redMushroom.id, 1, 0)},
		{'B', ItemInstance(Items::bowlEmpty->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::bread->getShiftedIndex(), 1, 0), {"###"}, {
		{'#', ItemInstance(Items::wheat->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Items::cookie->getShiftedIndex(), 8, 0), {"#X#"}, {
		{'#', ItemInstance(Items::wheat->getShiftedIndex(), 1, 0)},
		{'X', ItemInstance(Items::dyePowder->getShiftedIndex(), 1, 3)}
	});
	addShapedRecipe(ItemInstance(Items::appleGold->getShiftedIndex(), 1, 0), {"###", "#X#", "###"}, {
		{'#', ItemInstance(Tile::goldBlock.id, 1, -1)},
		{'X', ItemInstance(Items::apple->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::bucketEmpty->getShiftedIndex(), 1, 0), {"# #", " # "}, {
		{'#', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::flintAndSteel->getShiftedIndex(), 1, 0), {"A ", " B"}, {
		{'A', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)},
		{'B', ItemInstance(Items::flint->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Items::sign->getShiftedIndex(), 1, 0), {"###", "###", " X "}, {
		{'#', ItemInstance(Tile::wood.id, 1, -1)},
		{'X', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Tile::chest.id, 1, 0), {"###", "# #", "###"}, {
		{'#', ItemInstance(Tile::wood.id, 1, -1)}
	});
	addShapedRecipe(ItemInstance(Tile::rail.id, 16, 0), {"X X", "X#X", "X X"}, {
		{'X', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)},
		{'#', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Tile::railPowered.id, 6, 0), {"X X", "X#X", "XRX"}, {
		{'X', ItemInstance(Items::ingotGold->getShiftedIndex(), 1, 0)},
		{'#', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)},
		{'R', ItemInstance(Items::redstone->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Tile::railDetector.id, 6, 0), {"X X", "X#X", "XRX"}, {
		{'X', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)},
		{'#', ItemInstance(Tile::pressurePlateStone.id, 1, 0)},
		{'R', ItemInstance(Items::redstone->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Items::minecart->getShiftedIndex(), 1, 0), {"# #", "###"}, {
		{'#', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Items::minecartPowered->getShiftedIndex(), 1, 0), {"A", "B"}, {
		{'A', ItemInstance(Tile::furnace.id, 1, 0)},
		{'B', ItemInstance(Items::minecart->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::minecartChest->getShiftedIndex(), 1, 0), {"A", "B"}, {
		{'A', ItemInstance(Tile::chest.id, 1, 0)},
		{'B', ItemInstance(Items::minecart->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::boat->getShiftedIndex(), 1, 0), {"# #", "###"}, {
		{'#', ItemInstance(Tile::wood.id, 1, -1)}
	});

	addShapedRecipe(ItemInstance(Tile::torchRedstoneActive.id, 1, 0), {"X", "#"}, {
		{'#', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)},
		{'X', ItemInstance(Items::redstone->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::redstoneRepeater->getShiftedIndex(), 1, 0), {"#X#", "III"}, {
		{'#', ItemInstance(Tile::torchRedstoneActive.id, 1, 0)},
		{'X', ItemInstance(Items::redstone->getShiftedIndex(), 1, 0)},
		{'I', ItemInstance(Tile::rock.id, 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::clock->getShiftedIndex(), 1, 0), {" # ", "#X#", " # "}, {
		{'#', ItemInstance(Items::ingotGold->getShiftedIndex(), 1, 0)},
		{'X', ItemInstance(Items::redstone->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Items::compass->getShiftedIndex(), 1, 0), {" # ", "#X#", " # "}, {
		{'#', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)},
		{'X', ItemInstance(Items::redstone->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::shears->getShiftedIndex(), 1, 0), {" #", "# "}, {
		{'#', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::bed->getShiftedIndex(), 1, 0), {"###", "XXX"}, {
		{'#', ItemInstance(Tile::wool.id, 1, -1)},
		{'X', ItemInstance(Tile::wood.id, 1, -1)}
	});

	addShapedRecipe(ItemInstance(Tile::pistonBase.id, 1, 0), {"TTT", "#X#", "#R#"}, {
		{'T', ItemInstance(Tile::wood.id, 1, -1)},
		{'#', ItemInstance(Tile::cobblestone.id, 1, -1)},
		{'X', ItemInstance(Items::ingotIron->getShiftedIndex(), 1, 0)},
		{'R', ItemInstance(Items::redstone->getShiftedIndex(), 1, 0)}
	});
	addShapedRecipe(ItemInstance(Tile::pistonStickyBase.id, 1, 0), {"S", "P"}, {
		{'S', ItemInstance(Items::slimeball->getShiftedIndex(), 1, 0)},
		{'P', ItemInstance(Tile::pistonBase.id, 1, 0)}
	});
	
	addShapedRecipe(ItemInstance(Tile::buttonStone.id, 1, 0), {"#"}, {
		{'#', ItemInstance(Tile::rock.id, 1, 0)}
	});

	addShapedRecipe(ItemInstance(Tile::pressurePlateStone.id, 1, 0), {"##"}, {
		{'#', ItemInstance(Tile::rock.id, 1, 0)}
	});

	addShapedRecipe(ItemInstance(Tile::pressurePlateWood.id, 1, 0), {"##"}, {
		{'#', ItemInstance(Tile::wood.id, 1, -1)}
	});

	addShapedRecipe(ItemInstance(Tile::lever.id, 1, 0), {"#", "X"}, {
		{'#', ItemInstance(Tile::cobblestone.id, 1, -1)},
		{'X', ItemInstance(Items::stick->getShiftedIndex(), 1, 0)}
	});

	addShapedRecipe(ItemInstance(Tile::tnt.id, 1, 0), {"X#X", "#X#", "X#X"}, {
		{'X', ItemInstance(Items::gunpowder->getShiftedIndex(), 1, 0)},
		{'#', ItemInstance(Tile::sand.id, 1, 0)}
	});

	addShapedRecipe(ItemInstance(Items::map->getShiftedIndex(), 1, 0), {"###", "#X#", "###"}, {
		{'#', ItemInstance(Items::paper->getShiftedIndex(), 1, 0)},
		{'X', ItemInstance(Items::compass->getShiftedIndex(), 1, 0)}
	});

	std::sort(shapedRecipes.begin(), shapedRecipes.end(), [](const ShapedRecipe &a, const ShapedRecipe &b) {
		return a.size() > b.size();
	});
}

ItemInstance Recipes::getItemFor(const CraftingContainer &container) const
{
	for (const ShapedRecipe &recipe : shapedRecipes)
		if (matches(recipe, container))
			return ItemInstance(recipe.result.itemID, recipe.result.stackSize, recipe.result.itemDamage);
	return ItemInstance();
}

void Recipes::addShapedRecipe(const ItemInstance &result, const std::vector<std::string> &pattern,
	const std::vector<std::pair<char, ItemInstance>> &mapping)
{
	if (pattern.empty())
		return;

	int_t width = static_cast<int_t>(pattern[0].size());
	int_t height = static_cast<int_t>(pattern.size());
	std::unordered_map<char, ItemInstance> itemMap;
	for (const auto &entry : mapping)
		itemMap[entry.first] = entry.second;

	ShapedRecipe recipe;
	recipe.width = width;
	recipe.height = height;
	recipe.result = result;
	recipe.items.resize(width * height);

	for (int_t y = 0; y < height; ++y)
	{
		for (int_t x = 0; x < width; ++x)
		{
			char key = pattern[y][x];
			auto it = itemMap.find(key);
			if (it != itemMap.end())
				recipe.items[x + y * width] = it->second;
			else
				recipe.items[x + y * width] = ItemInstance();
		}
	}

	shapedRecipes.push_back(std::move(recipe));
}
