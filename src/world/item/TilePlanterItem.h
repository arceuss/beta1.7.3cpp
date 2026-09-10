#pragma once

#include "world/item/Item.h"

class Tile;

// Java TilePlanterItem (CFR ItemReed): plants a fixed tile with data 0 and no
// build-height rule, and reports success even when the target refuses it.
class TilePlanterItem : public Item
{
public:
	TilePlanterItem(int_t baseId, Tile &tile);

	bool useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const override;

private:
	int_t tileId;
};
