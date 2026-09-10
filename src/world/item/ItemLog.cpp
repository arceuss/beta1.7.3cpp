#include "world/item/ItemLog.h"

#include "world/item/ItemInstance.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/TreeTile.h"

ItemLog::ItemLog(int_t baseId) : TileItem(baseId)
{
	setMaxDamage(0);
	setHasSubtypes(true);
}

int_t ItemLog::getIcon(const ItemInstance &stack) const
{
	return Tile::treeTrunk.getTexture(Facing::NORTH, stack.itemDamage);
}

int_t ItemLog::getLevelDataForAuxValue(int_t auxValue) const
{
	return auxValue;
}
