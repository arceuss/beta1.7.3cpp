#pragma once

#include "world/item/Item.h"

class ItemDye : public Item
{
public:
	ItemDye(int_t baseId);

	int_t getIcon(const ItemInstance &stack) const override;
	jstring getDescriptionId(const ItemInstance &stack) const override;
	// ItemDye.dyeColors[colour]
	static const jstring &getDyeColorName(int_t colour);
	bool useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const override;
	void saddleEntity(ItemInstance &stack, Mob &target) const override;
};
