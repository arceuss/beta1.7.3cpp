#pragma once

#include <array>

#include "java/String.h"
#include "java/Type.h"
#include "Facing.h"

class ItemInstance;
class Tile;
class Entity;
class Player;
class Level;

class Item
{
public:
	static std::array<Item *, 32000> items;

private:
	int_t baseId = 0;
	int_t shiftedIndex = 0;
	int_t maxStackSize = 64;
	int_t maxDamage = 0;
	int_t iconIndex = 0;
	jstring descriptionId;

public:
	explicit Item(int_t baseId);
	virtual ~Item() {}

	int_t getShiftedIndex() const { return shiftedIndex; }
	int_t getMaxStackSize() const { return maxStackSize; }
	int_t getMaxDamage() const { return maxDamage; }
	const jstring &getDescriptionId() const { return descriptionId; }

	Item &setMaxStackSize(int_t size);
	Item &setMaxDamage(int_t damage);
	Item &setIconIndex(int_t icon);
	Item &setDescriptionId(const jstring &id);

	virtual int_t getIcon(const ItemInstance &stack) const;
	virtual float getDestroySpeed(const ItemInstance &stack, Tile &tile) const;
	virtual bool canDestroySpecial(const ItemInstance &stack, Tile &tile) const;
	virtual int_t getAttackDamage(const ItemInstance &stack, Entity &entity) const;
	virtual bool useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const;
	virtual bool hurtEnemy(ItemInstance &stack, Entity &target, Entity &attacker) const;
	virtual bool mineBlock(ItemInstance &stack, int_t tile, int_t x, int_t y, int_t z, Entity &miner) const;
	virtual bool isFull3D() const { return false; }
	virtual bool shouldRotateAroundWhenRendering() const { return false; }
};