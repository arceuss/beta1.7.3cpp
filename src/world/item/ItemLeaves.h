#pragma once

#include "world/item/TileItem.h"

// Java ItemLeaves: placed leaves get bit 3 set so decay skips them.
class ItemLeaves : public TileItem
{
public:
	explicit ItemLeaves(int_t baseId);

	int_t getIcon(const ItemInstance &stack) const override;
	int_t getLevelDataForAuxValue(int_t auxValue) const override;
};
