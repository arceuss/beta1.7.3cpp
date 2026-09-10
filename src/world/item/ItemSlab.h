#pragma once

#include "world/item/TileItem.h"

// Java ItemSlab: only the variant travels in the aux value. The downward merge
// belongs to StoneSlabTile.onPlace, not to the item.
class ItemSlab : public TileItem
{
public:
	explicit ItemSlab(int_t baseId);

	int_t getIcon(const ItemInstance &stack) const override;
	int_t getLevelDataForAuxValue(int_t auxValue) const override;
	jstring getDescriptionId(const ItemInstance &stack) const override;
};
