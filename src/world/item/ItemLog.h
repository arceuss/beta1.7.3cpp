#pragma once

#include "world/item/TileItem.h"

// Java ItemLog: the trunk species survives placement.
class ItemLog : public TileItem
{
public:
	explicit ItemLog(int_t baseId);

	int_t getIcon(const ItemInstance &stack) const override;
	int_t getLevelDataForAuxValue(int_t auxValue) const override;
};
