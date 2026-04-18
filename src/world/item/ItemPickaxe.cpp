#include "world/item/ItemPickaxe.h"

#include "world/level/tile/StoneTile.h"
#include "world/level/tile/IceTile.h"
#include "world/level/tile/SlabTile.h"
#include "world/level/tile/Tile.h"

namespace
{
	struct EffectiveTiles
	{
		const int_t *tiles;
		std::size_t count;
	};

	EffectiveTiles getEffectiveTiles()
	{
		static int_t EFFECTIVE_TILES[] = {
			Tile::cobblestone.id,
			Tile::slabDouble.id,
			Tile::slabSingle.id,
			Tile::rock.id,
			Tile::sandstone.id,
			Tile::mossyCobblestone.id,
			Tile::ironOre.id,
			Tile::coalOre.id,
			Tile::goldOre.id,
			Tile::diamondOre.id,
			Tile::ice.id,
			Tile::lapisOre.id,
		};
		return EffectiveTiles{EFFECTIVE_TILES, sizeof(EFFECTIVE_TILES) / sizeof(EFFECTIVE_TILES[0])};
	}
}

ItemPickaxe::ItemPickaxe(int_t baseId, ToolMaterialType material)
	: ItemTool(baseId, 2, material, getEffectiveTiles().tiles, getEffectiveTiles().count)
{
}

bool ItemPickaxe::canDestroySpecial(const ItemInstance &stack, Tile &tile) const
{
	(void)stack;
	int_t harvestLevel = getToolMaterialData(toolMaterial).harvestLevel;
	if (tile.id == Tile::obsidian.id)
		return harvestLevel >= 3;
	if (tile.id == Tile::diamondOre.id || tile.id == Tile::goldOre.id || tile.id == Tile::redstoneOre.id)
		return harvestLevel >= 2;
	if (tile.id == Tile::ironOre.id || tile.id == Tile::lapisOre.id)
		return harvestLevel >= 1;
	return true;
}
