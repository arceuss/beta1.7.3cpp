#pragma once

#include <array>

#include "world/level/tile/entity/TileEntity.h"
#include "world/item/ItemInstance.h"

class Player;

class FurnaceTileEntity : public TileEntity
{
private:
	std::array<ItemInstance, 3> items = {};

	bool canSmelt() const;
	void smeltItem();
	int_t getBurnDuration(const ItemInstance &stack) const;

public:
	int_t burnTime = 0;
	int_t currentItemBurnTime = 0;
	int_t cookTime = 0;

	jstring getEncodeId() const override { return u"Furnace"; }
	void load(CompoundTag &tag) override;
	void save(CompoundTag &tag) override;
	void tick() override;

	ItemInstance &getItem(int_t slot) { return items[slot]; }
	const ItemInstance &getItem(int_t slot) const { return items[slot]; }
	int_t getCookProgressScaled(int_t scale) const;
	int_t getBurnTimeRemainingScaled(int_t scale) const;
	bool isBurning() const;
	bool canUse(Player &player) const;
};
