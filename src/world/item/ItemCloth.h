#pragma once

#include "world/item/TileItem.h"

// Java ItemCloth: wool keeps its colour in the stack aux value.
class ItemCloth : public TileItem
{
public:
	explicit ItemCloth(int_t baseId);

	int_t getIcon(const ItemInstance &stack) const override;
	int_t getLevelDataForAuxValue(int_t auxValue) const override;
	jstring getDescriptionId(const ItemInstance &stack) const override;
};
