#include "world/item/ItemCloth.h"

#include "world/item/ItemDye.h"
#include "world/item/ItemInstance.h"
#include "world/level/tile/ClothTile.h"
#include "world/level/tile/Tile.h"

ItemCloth::ItemCloth(int_t baseId) : TileItem(baseId)
{
	setMaxDamage(0);
	setHasSubtypes(true);
}

int_t ItemCloth::getIcon(const ItemInstance &stack) const
{
	return Tile::wool.getTexture(Facing::NORTH, ~stack.itemDamage & 15);
}

int_t ItemCloth::getLevelDataForAuxValue(int_t auxValue) const
{
	return auxValue;
}

jstring ItemCloth::getDescriptionId(const ItemInstance &stack) const
{
	return TileItem::getDescriptionId() + u"." + ItemDye::getDyeColorName(~stack.itemDamage & 15);
}
