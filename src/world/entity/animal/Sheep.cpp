#include "world/entity/animal/Sheep.h"

#include "world/entity/item/EntityItem.h"
#include "world/entity/player/Player.h"
#include "world/item/Items.h"
#include "world/level/tile/Tile.h"
#include "world/item/Item.h"
#include "world/level/Level.h"
#include "world/level/tile/ClothTile.h"

Sheep::Sheep(Level &level) : Animal(level)
{
	textureName = u"/mob/sheep.png";
	setSize(0.9f, 1.3f);
}

bool Sheep::interact(Player &player)
{
	ItemInstance *selected = player.getSelectedItem();
	if (selected != nullptr && selected->itemID == Items::shears->getShiftedIndex() && !sheared)
	{
		if (!level.isOnline)
		{
			sheared = true;
			int_t count = 2 + random.nextInt(3);
			for (int_t i = 0; i < count; ++i)
			{
				auto dropped = std::make_shared<EntityItem>(level, x, y + 1.0, z, ItemInstance(Tile::wool.id, 1, fleeceColor));
				dropped->yd += random.nextFloat() * 0.05f;
				dropped->xd += (random.nextFloat() - random.nextFloat()) * 0.1f;
				dropped->zd += (random.nextFloat() - random.nextFloat()) * 0.1f;
				level.addEntity(dropped);
			}
		}
		selected->damageItem(1);
		if (selected->isEmpty())
			player.removeSelectedItem();
	}
	return false;
}

void Sheep::dropDeathLoot()
{
	if (!sheared)
		spawnAtLocation(ItemInstance(Tile::wool.id, 1, fleeceColor), 0.0f);
}

void Sheep::addAdditionalSaveData(CompoundTag &tag)
{
	Animal::addAdditionalSaveData(tag);
	tag.putBoolean(u"Sheared", sheared);
	tag.putByte(u"Color", static_cast<byte_t>(fleeceColor));
}

void Sheep::readAdditionalSaveData(CompoundTag &tag)
{
	Animal::readAdditionalSaveData(tag);
	sheared = tag.getBoolean(u"Sheared");
	fleeceColor = tag.getByte(u"Color");
}

jstring Sheep::getAmbientSound()
{
	return u"mob.sheep";
}

jstring Sheep::getHurtSound()
{
	return u"mob.sheep";
}

jstring Sheep::getDeathSound()
{
	return u"mob.sheep";
}

int_t Sheep::getFleeceColor() const
{
	return fleeceColor & 15;
}

void Sheep::setFleeceColor(int_t color)
{
	fleeceColor = color & 15;
}

bool Sheep::isSheared() const
{
	return sheared;
}

void Sheep::setSheared(bool sheared)
{
	this->sheared = sheared;
}

int_t Sheep::getRandomFleeceColor(Random &random)
{
	int_t value = random.nextInt(100);
	if (value < 5) return 15;
	if (value < 10) return 7;
	if (value < 15) return 8;
	if (value < 18) return 12;
	return random.nextInt(500) == 0 ? 6 : 0;
}
