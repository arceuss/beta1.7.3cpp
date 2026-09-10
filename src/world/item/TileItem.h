#pragma once

#include "world/item/Item.h"

// Java TileItem (CFR ItemBlock): the ordinary inventory adapter every registered
// tile gets. Subclasses only vary the icon, the description suffix and the world
// data written by getLevelDataForAuxValue.
class TileItem : public Item
{
public:
	explicit TileItem(int_t baseId);

	int_t getTileId() const { return tileId; }

	bool useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const override;

	jstring getDescriptionId(const ItemInstance &stack) const override;
	jstring getDescriptionId() const override;

private:
	int_t tileId;
};
