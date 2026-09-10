#include "world/item/ItemSlab.h"

#include "world/item/ItemInstance.h"
#include "world/level/tile/SlabTile.h"
#include "world/level/tile/Tile.h"

ItemSlab::ItemSlab(int_t baseId) : TileItem(baseId)
{
	setMaxDamage(0);
	setHasSubtypes(true);
}

int_t ItemSlab::getIcon(const ItemInstance &stack) const
{
	return Tile::slabSingle.getTexture(Facing::NORTH, stack.itemDamage);
}

int_t ItemSlab::getLevelDataForAuxValue(int_t auxValue) const
{
	return auxValue;
}

jstring ItemSlab::getDescriptionId(const ItemInstance &stack) const
{
	// StoneSlabTile.names, indexed by the aux value. Java would throw above 3;
	// masking keeps stored worlds with junk aux values loadable and leaves every
	// valid variant untouched.
	static const jstring names[] = {u"stone", u"sand", u"wood", u"cobble"};
	return TileItem::getDescriptionId() + u"." + names[stack.itemDamage & 3];
}
