#include "world/entity/item/EntityItem.h"

#include "java/Math.h"
#include "nbt/CompoundTag.h"
#include "util/Mth.h"
#include "world/entity/player/InventoryPlayer.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/TreeTile.h"
#include "world/stats/AchievementList.h"
#include "world/stats/Achievement.h"

EntityItem::EntityItem(Level &level) : Entity(level)
{
	setSize(0.25f, 0.25f);
	heightOffset = bbHeight / 2.0f;
	// ItemEntity.bobOffs is a field initializer over Math.random and the double Math.PI,
	// so it is drawn from the shared global source, not from the entity RNG.
	bobOffs = static_cast<float>(Math::random() * 3.141592653589793 * 2.0);
	makeStepSound = false;
}

EntityItem::EntityItem(Level &level, double x, double y, double z, const ItemInstance &stack) : EntityItem(level)
{
	setPos(x, y, z);
	item = stack;
	// Spawn spread comes from Math.random as well, and each component is narrowed to
	// float before it lands in the double velocity field.
	yRot = static_cast<float>(Math::random() * 360.0);
	xd = static_cast<float>(Math::random() * static_cast<double>(0.2f) - static_cast<double>(0.1f));
	yd = 0.2f;
	zd = static_cast<float>(Math::random() * static_cast<double>(0.2f) - static_cast<double>(0.1f));
}

void EntityItem::tick()
{
	Entity::tick();
	if (throwTime > 0)
		throwTime--;

	xo = x;
	yo = y;
	zo = z;
	yd -= 0.04f;

	if (&level.getMaterial(Mth::floor(x), Mth::floor(y), Mth::floor(z)) == static_cast<const Material *>(&Material::lava))
	{
		yd = 0.2f;
		xd = (random.nextFloat() - random.nextFloat()) * 0.2f;
		zd = (random.nextFloat() - random.nextFloat()) * 0.2f;
		level.playSoundAtEntity(*this, u"random.fizz", 0.4f, 2.0f + random.nextFloat() * 0.4f);
	}

	pushOutOfBlocks(x, (bb.y0 + bb.y1) / 2.0, z);
	move(xd, yd, zd);

	float friction = 0.98f;
	if (onGround)
	{
		// 0.1f * 0.1f * 58.8f == 0.58800006f, only used when there is no tile below the box.
		friction = 0.1f * 0.1f * 58.8f;
		int_t support = level.getTile(Mth::floor(x), Mth::floor(bb.y0) - 1, Mth::floor(z));
		if (support > 0)
		{
			Tile *supportTile = Tile::tiles[support];
			if (supportTile != nullptr)
				friction = supportTile->friction * 0.98f;
		}
	}

	xd *= friction;
	yd *= 0.98f;
	zd *= friction;
	if (onGround)
		yd *= -0.5;

	// ItemEntity's own tick counter shadows Entity.tickCount in Java and is never read,
	// so nothing increments here.
	age++;
	if (age >= 6000)
		remove();
}

void EntityItem::playerTouch(Player &player)
{
	if (level.isOnline)
		return;

	int_t originalCount = item.stackSize;
	int_t originalId = item.itemID;
	if (throwTime == 0 && player.inventory.add(item))
	{
		if (originalId == Tile::treeTrunk.id)
			player.triggerAchievement(*AchievementList::mineWood);
		if (originalId == Items::leather->getShiftedIndex())
			player.triggerAchievement(*AchievementList::killCow);
		const float popA = random.nextFloat();
		const float popB = random.nextFloat();
		level.playSoundAtEntity(*this, u"random.pop", 0.2f, ((popA - popB) * 0.7f + 1.0f) * 2.0f);
		player.take(*this, originalCount);
		if (item.stackSize <= 0)
			remove();
	}
}

void EntityItem::burn(int_t dmg)
{
	hurt(nullptr, dmg);
}

bool EntityItem::hurt(Entity *source, int_t dmg)
{
	markHurt();
	health -= dmg;
	if (health <= 0)
		remove();
	return false;
}

bool EntityItem::handleWaterMovement()
{
	return level.handleMaterialAcceleration(bb, Material::water, *this);
}

void EntityItem::addAdditionalSaveData(CompoundTag &tag)
{
	tag.putShort(u"Health", static_cast<short_t>(static_cast<byte_t>(health)));
	tag.putShort(u"Age", static_cast<short_t>(age));
	auto itemTag = std::make_shared<CompoundTag>();
	item.save(*itemTag);
	tag.put(u"Item", itemTag);
}

void EntityItem::readAdditionalSaveData(CompoundTag &tag)
{
	health = tag.getShort(u"Health") & 0xFF;
	age = tag.getShort(u"Age");
	auto itemTag = tag.getCompound(u"Item");
	if (itemTag)
		item.load(*itemTag);
}
