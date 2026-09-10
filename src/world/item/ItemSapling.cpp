#include "world/item/ItemSapling.h"

#include "world/item/ItemInstance.h"
#include "world/level/tile/SaplingTile.h"
#include "world/level/tile/Tile.h"

ItemSapling::ItemSapling(int_t baseId) : TileItem(baseId)
{
	setMaxDamage(0);
	setHasSubtypes(true);
}

int_t ItemSapling::getIcon(const ItemInstance &stack) const
{
	return Tile::sapling.getTexture(Facing::DOWN, stack.itemDamage);
}

int_t ItemSapling::getLevelDataForAuxValue(int_t auxValue) const
{
	return auxValue;
}
