#include "world/entity/monster/Creeper.h"

#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/item/Item.h"
#include "world/level/Explosion.h"

Creeper::Creeper(Level &level) : Monster(level)
{
	dataWatcher.addObject(16, static_cast<byte_t>(-1));
	dataWatcher.addObject(17, static_cast<byte_t>(0));
	textureName = u"/mob/creeper.png";
}

void Creeper::tick()
{
	oldSwell = swell;
	if (level.isOnline)
	{
		int_t state = getCreeperState();
		if (state > 0 && swell == 0)
			level.playSoundAtEntity(*this, u"random.fuse", 1.0f, 0.5f);
		swell += state;
		if (swell < 0)
			swell = 0;
		if (swell >= 30)
			swell = 30;
	}
	Monster::tick();
	if (attackTarget == nullptr && swell > 0)
	{
		setCreeperState(-1);
		swell--;
		if (swell < 0)
			swell = 0;
	}
}

float Creeper::getSwelling(float a) const
{
	return (oldSwell + (swell - oldSwell) * a) / 28.0f;
}

bool Creeper::isPowered() const
{
	return dataWatcher.getWatchableObjectByte(17) == 1;
}

int_t Creeper::getCreeperState() const
{
	return dataWatcher.getWatchableObjectByte(16);
}

void Creeper::setCreeperState(int_t state)
{
	dataWatcher.updateObject(16, static_cast<byte_t>(state));
}

void Creeper::onStruckByLightning(Entity &lightning)
{
	Entity::onStruckByLightning(lightning);
	dataWatcher.updateObject(17, static_cast<byte_t>(1));
}
void Creeper::addAdditionalSaveData(CompoundTag &tag)
{
	Monster::addAdditionalSaveData(tag);
	if (isPowered())
		tag.putBoolean(u"powered", true);
}

void Creeper::readAdditionalSaveData(CompoundTag &tag)
{
	Monster::readAdditionalSaveData(tag);
	dataWatcher.updateObject(17, static_cast<byte_t>(tag.getBoolean(u"powered") ? 1 : 0));
}

void Creeper::attackBlockedEntity(Entity &entity, float distance)
{
	(void)entity;
	(void)distance;
	if (!level.isOnline && swell > 0)
	{
		setCreeperState(-1);
		swell--;
		if (swell < 0)
			swell = 0;
	}
}

void Creeper::checkHurtTarget(Entity &entity, float distance)
{
	(void)entity;
	if (level.isOnline)
		return;
	int_t state = getCreeperState();
	if ((state <= 0 && distance < 3.0f) || (state > 0 && distance < 7.0f))
	{
		if (swell == 0)
			level.playSoundAtEntity(*this, u"random.fuse", 1.0f, 0.5f);
		setCreeperState(1);
		swell++;
		if (swell >= 30)
		{
			level.createExplosion(this, x, y, z, isPowered() ? 6.0f : 3.0f);
			remove();
		}
		holdGround = true;
	}
	else
	{
		setCreeperState(-1);
		swell--;
		if (swell < 0)
			swell = 0;
	}
}

jstring Creeper::getHurtSound()
{
	return u"mob.creeper";
}

jstring Creeper::getDeathSound()
{
	return u"mob.creeperdeath";
}

int_t Creeper::getDeathLoot()
{
	return Items::gunpowder->getShiftedIndex();
}
