#include "world/entity/animal/Pig.h"

#include "world/entity/monster/PigZombie.h"
#include "world/entity/player/Player.h"
#include "world/item/Items.h"
#include "world/item/Item.h"
#include "world/level/Level.h"

Pig::Pig(Level &level) : Animal(level)
{
	textureName = u"/mob/pig.png";
	setSize(0.9f, 0.9f);
}

bool Pig::interact(Player &player)
{
	if (!saddled || level.isOnline || (rider != nullptr && rider.get() != &player))
		return false;
	auto self = level.getEntityRef(*this);
	if (self == nullptr)
		return false;
	player.ride(self);
	return true;
}

void Pig::onStruckByLightning(Entity &lightning)
{
	Entity::onStruckByLightning(lightning);
	if (level.isOnline)
		return;
	auto pigZombie = std::make_shared<PigZombie>(level);
	pigZombie->moveTo(x, y, z, yRot, xRot);
	level.addEntity(pigZombie);
	remove();
}

void Pig::addAdditionalSaveData(CompoundTag &tag)
{
	Animal::addAdditionalSaveData(tag);
	tag.putBoolean(u"Saddle", saddled);
}

void Pig::readAdditionalSaveData(CompoundTag &tag)
{
	Animal::readAdditionalSaveData(tag);
	saddled = tag.getBoolean(u"Saddle");
}

jstring Pig::getAmbientSound()
{
	return u"mob.pig";
}

jstring Pig::getHurtSound()
{
	return u"mob.pig";
}

jstring Pig::getDeathSound()
{
	return u"mob.pigdeath";
}

int_t Pig::getDeathLoot()
{
	return onFire > 0 ? Items::porkchopCooked->getShiftedIndex() : Items::porkchopRaw->getShiftedIndex();
}

bool Pig::isSaddled() const
{
	return saddled;
}

void Pig::setSaddled(bool saddled)
{
	this->saddled = saddled;
}
