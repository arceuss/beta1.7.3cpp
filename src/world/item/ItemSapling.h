#pragma once

#include "world/item/TileItem.h"

// Java ItemSapling: the sapling species survives placement.
class ItemSapling : public TileItem
{
public:
	explicit ItemSapling(int_t baseId);

	int_t getIcon(const ItemInstance &stack) const override;
	int_t getLevelDataForAuxValue(int_t auxValue) const override;
};
