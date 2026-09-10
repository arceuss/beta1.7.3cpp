#include "world/item/ItemLeaves.h"

#include "world/item/ItemInstance.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/tile/Tile.h"

ItemLeaves::ItemLeaves(int_t baseId) : TileItem(baseId)
{
	setMaxDamage(0);
	setHasSubtypes(true);
}

int_t ItemLeaves::getIcon(const ItemInstance &stack) const
{
	return Tile::leaves.getTexture(Facing::DOWN, stack.itemDamage);
}

int_t ItemLeaves::getLevelDataForAuxValue(int_t auxValue) const
{
	return auxValue | 8;
}
