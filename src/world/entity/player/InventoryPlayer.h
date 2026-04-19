#pragma once

#include <array>
#include <memory>

#include "java/Type.h"
#include "world/item/ItemInstance.h"

class Player;
class ListTag;
class Tile;
class Entity;

class InventoryPlayer
{
public:
	std::array<ItemInstance, 36> mainInventory = {};
	std::array<ItemInstance, 4> armorInventory = {};
	int_t currentItem = 0;


	InventoryPlayer(Player *player = nullptr);

	ItemInstance *getCurrentItem();
	ItemInstance *getSelected();
	ItemInstance *getItem(int_t slot);
	std::unique_ptr<ItemInstance> removeItem(int_t slot, int_t count);
	void setItem(int_t slot, const ItemInstance &item);
	void changeCurrentItem(int_t direction);
	void tick();
	bool add(ItemInstance &item);
	void dropAll();
	float getDestroySpeed(Tile &tile);
	bool canDestroySpecial(Tile &tile);
	int_t getAttackDamage(Entity &entity);
	int_t getArmorValue() const;
	void hurtArmor(int_t damage);
	void save(ListTag &tag) const;
	void load(ListTag &tag);

	ItemInstance *getCarried();
	void setCarried(const ItemInstance &item);
	void setCarried(ItemInstance &&item);
	void setCarriedNull();

private:
	Player *player = nullptr;
	ItemInstance carried = {};
	int_t getFreeSlot() const;
	int_t getSlotWithRemainingSpace(const ItemInstance &item) const;
};
