#pragma once

#include "world/item/TileItem.h"

// Java ItemPiston: places the staging data 7 so onPlace cannot act on a
// direction before setPlacedBy installs the player's facing.
class ItemPiston : public TileItem
{
public:
	explicit ItemPiston(int_t baseId);

	int_t getLevelDataForAuxValue(int_t auxValue) const override;
};
