#pragma once

#include <array>

#include "world/level/tile/entity/TileEntity.h"
#include "world/item/ItemInstance.h"

class Player;

class DispenserTileEntity : public TileEntity
{
private:
	std::array<ItemInstance, 9> items = {};

public:
	jstring getEncodeId() const override { return u"Trap"; }
	void load(CompoundTag &tag) override;
	void save(CompoundTag &tag) override;

	ItemInstance &getItem(int_t slot) { return items[slot]; }
	const ItemInstance &getItem(int_t slot) const { return items[slot]; }
	void setItem(int_t slot, const ItemInstance &item);
	ItemInstance removeItem(int_t slot, int_t count);
	int_t getContainerSize() const { return static_cast<int_t>(items.size()); }
	bool canUse(Player &player) const;
};