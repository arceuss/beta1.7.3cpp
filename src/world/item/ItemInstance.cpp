#include "world/item/ItemInstance.h"

#include "Facing.h"
#include "nbt/CompoundTag.h"
#include "world/entity/Entity.h"
#include "world/entity/Mob.h"
#include "world/item/Item.h"
#include "world/level/tile/Tile.h"

ItemInstance::ItemInstance(int_t itemID) : ItemInstance(itemID, 1, 0)
{
}

ItemInstance::ItemInstance(int_t itemID, int_t count) : ItemInstance(itemID, count, 0)
{
}

ItemInstance::ItemInstance(int_t itemID, int_t count, int_t damage)
	: stackSize(count), itemID(itemID), itemDamage(damage)
{
}

ItemInstance::ItemInstance(CompoundTag &tag)
{
	load(tag);
}

Item *ItemInstance::getItem() const
{
	if (itemID < 0 || itemID >= static_cast<int_t>(Item::items.size()))
		return nullptr;
	return Item::items[itemID];
}

int_t ItemInstance::getMaxStackSize() const
{
	Item *item = getItem();
	if (item != nullptr)
		return item->getMaxStackSize();
	return 64;
}

int_t ItemInstance::getMaxDamage() const
{
	Item *item = getItem();
	if (item != nullptr)
		return item->getMaxDamage();
	return 0;
}

bool ItemInstance::isStackable() const
{
	return !isEmpty() && getMaxStackSize() > 1 && !isItemDamaged();
}

bool ItemInstance::isItemDamaged() const
{
	return getMaxDamage() > 0 && itemDamage > 0;
}

int_t ItemInstance::getIcon() const
{
	if (itemID >= 0 && itemID < static_cast<int_t>(Tile::tiles.size()) && Tile::tiles[itemID] != nullptr)
		return Tile::tiles[itemID]->getTexture(Facing::NORTH, itemDamage);
	Item *item = getItem();
	if (item != nullptr)
		return item->getIcon(*this);
	return 0;
}

float ItemInstance::getDestroySpeed(Tile &tile) const
{
	Item *item = getItem();
	if (item != nullptr)
		return item->getDestroySpeed(*this, tile);
	return 1.0f;
}

bool ItemInstance::canDestroySpecial(Tile &tile) const
{
	Item *item = getItem();
	if (item != nullptr)
		return item->canDestroySpecial(*this, tile);
	return true;
}

int_t ItemInstance::getAttackDamage(Entity &entity) const
{
	Item *item = getItem();
	if (item != nullptr)
		return item->getAttackDamage(*this, entity);
	return 1;
}

void ItemInstance::use(Level &level, Player &player)
{
	Item *item = getItem();
	if (item == nullptr)
		return;
	item->use(*this, level, player);
}

bool ItemInstance::useOn(Player &player, Level &level, int_t x, int_t y, int_t z, Facing face)
{
	Item *item = getItem();
	if (item == nullptr)
		return false;
	return item->useOn(*this, player, level, x, y, z, face);
}

void ItemInstance::damageItem(int_t amount)
{
	if (amount <= 0 || isEmpty())
		return;
	int_t maxDamage = getMaxDamage();
	if (maxDamage <= 0)
		return;
	itemDamage += amount;
	if (itemDamage > maxDamage)
	{
		itemDamage = 0;
		stackSize--;
		if (stackSize < 0)
			stackSize = 0;
	}
}

bool ItemInstance::hurtEnemy(Entity &target, Entity &attacker)
{
	Item *item = getItem();
	if (item == nullptr)
		return false;
	return item->hurtEnemy(*this, target, attacker);
}

void ItemInstance::saddleEntity(Mob &target)
{
	Item *item = getItem();
	if (item == nullptr)
		return;
	item->saddleEntity(*this, target);
}

bool ItemInstance::mineBlock(int_t tile, int_t x, int_t y, int_t z, Entity &miner)
{
	Item *item = getItem();
	if (item == nullptr)
		return false;
	return item->mineBlock(*this, tile, x, y, z, miner);
}

void ItemInstance::save(CompoundTag &tag) const
{
	tag.putShort(u"id", static_cast<short_t>(itemID));
	tag.putByte(u"Count", static_cast<byte_t>(stackSize));
	tag.putShort(u"Damage", static_cast<short_t>(itemDamage));
}

void ItemInstance::load(CompoundTag &tag)
{
	itemID = tag.getShort(u"id");
	stackSize = tag.getByte(u"Count");
	itemDamage = tag.getShort(u"Damage");
}

ItemInstance ItemInstance::remove(int_t count)
{
	if (count > stackSize)
		count = stackSize;
	stackSize -= count;
	return ItemInstance(itemID, count, itemDamage);
}

bool ItemInstance::sameItem(const ItemInstance &other) const
{
	return itemID == other.itemID && itemDamage == other.itemDamage;
}