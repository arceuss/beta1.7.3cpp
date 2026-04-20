#pragma once

#include <memory>

#include "java/String.h"
#include "world/item/ItemInstance.h"

class Player;
class ChestTileEntity;

class CompoundContainer
{
private:
	jstring name;
	std::shared_ptr<ChestTileEntity> first;
	std::shared_ptr<ChestTileEntity> second;

public:
	CompoundContainer(jstring name, std::shared_ptr<ChestTileEntity> first, std::shared_ptr<ChestTileEntity> second);
	int_t getContainerSize() const;
	ItemInstance &getItem(int_t slot);
	const ItemInstance &getItem(int_t slot) const;
	void setItem(int_t slot, const ItemInstance &item);
	ItemInstance removeItem(int_t slot, int_t count);
	bool canUse(Player &player) const;
	void setChanged();
	const jstring &getName() const;
};
