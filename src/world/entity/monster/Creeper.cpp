#include "world/entity/monster/Creeper.h"

#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/item/Item.h"
#include "world/level/Explosion.h"

Creeper::Creeper(Level &level) : Monster(level)
{
	textureName = u"/mob/creeper.png";
}

void Creeper::tick()
{
	oldSwell = swell;
	Monster::tick();
	if (attackTarget == nullptr && swell > 0)
	{
		swellDir = -1;
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
	return powered;
}


void Creeper::onStruckByLightning(Entity &lightning)
{
	Entity::onStruckByLightning(lightning);
	powered = true;
}
void Creeper::addAdditionalSaveData(CompoundTag &tag)
{
	Monster::addAdditionalSaveData(tag);
	if (powered)
		tag.putBoolean(u"powered", true);
}

void Creeper::readAdditionalSaveData(CompoundTag &tag)
{
	Monster::readAdditionalSaveData(tag);
	powered = tag.getBoolean(u"powered");
}

void Creeper::attackBlockedEntity(Entity &entity, float distance)
{
	(void)entity;
	(void)distance;
	if (!level.isOnline && swell > 0)
	{
		swellDir = -1;
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
	if ((swellDir <= 0 && distance < 3.0f) || (swellDir > 0 && distance < 7.0f))
	{
		if (swell == 0)
			level.playSoundAtEntity(*this, u"random.fuse", 1.0f, 0.5f);
		swellDir = 1;
		swell++;
		if (swell >= 30)
		{
			level.createExplosion(this, x, y, z, powered ? 6.0f : 3.0f);
			remove();
		}
		holdGround = true;
	}
	else
	{
		swellDir = -1;
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
