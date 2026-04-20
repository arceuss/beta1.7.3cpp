#pragma once

#include "java/Type.h"

class Entity;
class Tile;
class Item;
class Player;
class Level;
class CompoundTag;

enum class Facing;
class ItemInstance
{
public:
	int_t stackSize = 0;
	int_t itemID = 0;
	int_t itemDamage = 0;
	int_t popTime = 0;

	ItemInstance() = default;
	ItemInstance(int_t itemID);
	ItemInstance(int_t itemID, int_t count);
	ItemInstance(int_t itemID, int_t count, int_t damage);
	ItemInstance(CompoundTag &tag);

	bool isEmpty() const { return stackSize <= 0 || itemID == 0; }
	Item *getItem() const;
	int_t getMaxStackSize() const;
	int_t getMaxDamage() const;
	bool isStackable() const;
	bool isItemDamaged() const;
	int_t getIcon() const;
	int_t getAuxValue() const { return itemDamage; }
	float getDestroySpeed(Tile &tile) const;
	bool canDestroySpecial(Tile &tile) const;
	int_t getAttackDamage(Entity &entity) const;
	void use(Level &level, Player &player);
	bool useOn(Player &player, Level &level, int_t x, int_t y, int_t z, Facing face);
	void damageItem(int_t amount);
	bool hurtEnemy(Entity &target, Entity &attacker);
	bool mineBlock(int_t tile, int_t x, int_t y, int_t z, Entity &miner);
	void save(CompoundTag &tag) const;
	void load(CompoundTag &tag);
	ItemInstance remove(int_t count);
	bool sameItem(const ItemInstance &other) const;
};