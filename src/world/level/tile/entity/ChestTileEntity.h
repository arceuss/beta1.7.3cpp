#pragma once

#include <array>

#include "world/level/tile/entity/TileEntity.h"
#include "world/item/ItemInstance.h"

class Player;

class ChestTileEntity : public TileEntity
{
private:
	std::array<ItemInstance, 27> items = {};

public:
	jstring getEncodeId() const override { return u"Chest"; }
	void load(CompoundTag &tag) override;
	void save(CompoundTag &tag) override;
	ItemInstance &getItem(int_t slot);
	const ItemInstance &getItem(int_t slot) const;
	void setItem(int_t slot, const ItemInstance &item);
	ItemInstance removeItem(int_t slot, int_t count);
	int_t getContainerSize() const;
	bool canUse(Player &player) const;
	jstring getName() const;
};
