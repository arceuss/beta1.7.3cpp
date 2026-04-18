#include "world/entity/player/InventoryPlayer.h"

#include <utility>

#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"
#include "world/entity/Entity.h"
#include "world/entity/player/Player.h"
#include "world/level/tile/Tile.h"

InventoryPlayer::InventoryPlayer(Player *player) : player(player)
{
}

ItemInstance *InventoryPlayer::getCurrentItem()
{
	if (currentItem >= 0 && currentItem < 9)
	{
		ItemInstance &item = mainInventory[currentItem];
		if (!item.isEmpty())
			return &item;
	}
	return nullptr;
}

ItemInstance *InventoryPlayer::getSelected()
{
	return getCurrentItem();
}

ItemInstance *InventoryPlayer::getItem(int_t slot)
{
	if (slot >= 0 && slot < static_cast<int_t>(mainInventory.size()))
	{
		ItemInstance &item = mainInventory[slot];
		if (!item.isEmpty())
			return &item;
	}
	return nullptr;
}

std::unique_ptr<ItemInstance> InventoryPlayer::removeItem(int_t slot, int_t count)
{
	if (slot < 0 || slot >= static_cast<int_t>(mainInventory.size()) || mainInventory[slot].isEmpty())
		return nullptr;

	ItemInstance removed = mainInventory[slot].remove(count);
	if (mainInventory[slot].stackSize <= 0)
		mainInventory[slot] = ItemInstance();
	return std::make_unique<ItemInstance>(removed);
}

ItemInstance *InventoryPlayer::getCarried()
{
	if (!carried.isEmpty())
		return &carried;
	return nullptr;
}

void InventoryPlayer::setCarried(const ItemInstance &item)
{
	carried = item;
}

void InventoryPlayer::setCarried(ItemInstance &&item)
{
	carried = std::move(item);
}

void InventoryPlayer::setCarriedNull()
{
	carried = ItemInstance();
}


void InventoryPlayer::setItem(int_t slot, const ItemInstance &item)
{
	if (slot >= 0 && slot < static_cast<int_t>(mainInventory.size()))
		mainInventory[slot] = item;
}

void InventoryPlayer::changeCurrentItem(int_t direction)
{
	if (direction > 0)
		direction = 1;
	else if (direction < 0)
		direction = -1;

	for (currentItem -= direction; currentItem < 0; currentItem += 9)
	{
	}
	while (currentItem >= 9)
		currentItem -= 9;
}

void InventoryPlayer::tick()
{
	for (ItemInstance &item : mainInventory)
		if (!item.isEmpty() && item.popTime > 0)
			item.popTime--;
}

int_t InventoryPlayer::getFreeSlot() const
{
	for (int_t i = 0; i < static_cast<int_t>(mainInventory.size()); ++i)
		if (mainInventory[i].isEmpty())
			return i;
	return -1;
}

int_t InventoryPlayer::getSlotWithRemainingSpace(const ItemInstance &item) const
{
	for (int_t i = 0; i < static_cast<int_t>(mainInventory.size()); ++i)
	{
		const ItemInstance &existing = mainInventory[i];
		if (!existing.isEmpty() && existing.sameItem(item) && existing.isStackable() && existing.stackSize < existing.getMaxStackSize())
			return i;
	}
	return -1;
}

bool InventoryPlayer::add(ItemInstance &item)
{
	while (!item.isEmpty())
	{
		int_t slot = getSlotWithRemainingSpace(item);
		if (slot < 0)
			slot = getFreeSlot();
		if (slot < 0)
			return false;

		ItemInstance &target = mainInventory[slot];
		if (target.isEmpty())
		{
			int_t toMove = item.stackSize;
			if (toMove > item.getMaxStackSize())
				toMove = item.getMaxStackSize();
			target = ItemInstance(item.itemID, toMove, item.itemDamage);
			target.popTime = 5;
			item.stackSize -= toMove;
		}
		else
		{
			int_t space = target.getMaxStackSize() - target.stackSize;
			if (space <= 0)
				return false;
			int_t toMove = item.stackSize;
			if (toMove > space)
				toMove = space;
			target.stackSize += toMove;
			target.popTime = 5;
			item.stackSize -= toMove;
		}
	}
	return true;
}

void InventoryPlayer::dropAll()
{
	if (player == nullptr)
		return;

	for (ItemInstance &item : mainInventory)
	{
		if (!item.isEmpty())
		{
			player->drop(item, true);
			item = ItemInstance();
		}
	}
}

float InventoryPlayer::getDestroySpeed(Tile &tile)
{
	ItemInstance *item = getCurrentItem();
	if (item != nullptr)
		return item->getDestroySpeed(tile);
	return 1.0f;
}

bool InventoryPlayer::canDestroySpecial(Tile &tile)
{
	ItemInstance *item = getCurrentItem();
	if (item != nullptr)
		return item->canDestroySpecial(tile);
	return true;
}

int_t InventoryPlayer::getAttackDamage(Entity &entity)
{
	ItemInstance *item = getCurrentItem();
	if (item != nullptr)
		return item->getAttackDamage(entity);
	return 1;
}

int_t InventoryPlayer::getArmorValue() const
{
	return 0;
}

void InventoryPlayer::hurtArmor(int_t damage)
{
}

void InventoryPlayer::save(ListTag &tag) const
{
	for (int_t i = 0; i < static_cast<int_t>(mainInventory.size()); ++i)
	{
		const ItemInstance &item = mainInventory[i];
		if (item.isEmpty())
			continue;
		auto itemTag = std::make_shared<CompoundTag>();
		itemTag->putByte(u"Slot", static_cast<byte_t>(i));
		item.save(*itemTag);
		tag.add(itemTag);
	}
}

void InventoryPlayer::load(ListTag &tag)
{
	mainInventory.fill(ItemInstance());
	setCarriedNull();
	for (int_t i = 0; i < tag.size(); ++i)
	{
		auto itemTag = std::dynamic_pointer_cast<CompoundTag>(tag.get(i));
		if (!itemTag)
			continue;
		int_t slot = itemTag->getByte(u"Slot") & 255;
		if (slot < 0 || slot >= static_cast<int_t>(mainInventory.size()))
			continue;
		mainInventory[slot] = ItemInstance(*itemTag);
	}
}
