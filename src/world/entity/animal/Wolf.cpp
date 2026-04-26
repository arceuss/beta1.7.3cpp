#include "world/entity/animal/Wolf.h"

#include "nbt/CompoundTag.h"
#include "world/entity/animal/Sheep.h"
#include "world/entity/player/Player.h"
#include "world/entity/projectile/EntityArrow.h"
#include "world/item/Item.h"
#include "world/item/ItemFood.h"
#include "world/item/ItemInstance.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "util/Mth.h"

namespace
{
	constexpr byte_t WOLF_SITTING = 1;
	constexpr byte_t WOLF_ANGRY = 2;
	constexpr byte_t WOLF_TAMED = 4;
}

Wolf::Wolf(Level &level) : Animal(level)
{
	textureName = u"/mob/wolf.png";
	setSize(0.8f, 0.8f);
	runSpeed = 1.1f;
	health = 8;
}

jstring Wolf::getTexture()
{
	if (isWolfTamed())
		return u"/mob/wolf_tame.png";
	return isWolfAngry() ? u"/mob/wolf_angry.png" : Animal::getTexture();
}

bool Wolf::hurt(Entity *source, int_t damage)
{
	setWolfSitting(false);
	if (source != nullptr && dynamic_cast<Player *>(source) == nullptr && dynamic_cast<EntityArrow *>(source) == nullptr)
		damage = (damage + 1) / 2;
	if (!Animal::hurt(source, damage))
		return false;
	if (source != nullptr && source != this)
	{
		if (!isWolfTamed() && !isWolfAngry() && dynamic_cast<Player *>(source) != nullptr)
			setWolfAngry(true);
		setTarget(level.getEntityRef(*source));
	}
	return true;
}

bool Wolf::interact(Player &player)
{
	ItemInstance *selected = player.getSelectedItem();
	if (!isWolfTamed())
	{
		if (selected != nullptr && selected->itemID == Items::bone->getShiftedIndex() && !isWolfAngry())
		{
			selected->remove(1);
			if (!level.isOnline && random.nextInt(3) == 0)
			{
				setWolfTamed(true);
				setWolfSitting(true);
				health = 20;
				setWolfOwner(player.name);
			}
			return true;
		}
		return false;
	}

	if (selected != nullptr)
	{
		Item *item = selected->getItem();
		if (auto food = dynamic_cast<ItemFood *>(item))
		{
			if (food->isWolfsFavoriteMeat() && health < 20)
			{
				selected->remove(1);
				health += food->getHealAmount();
				if (health > 20)
					health = 20;
				return true;
			}
		}
	}
	if (player.name == owner)
	{
		setWolfSitting(!isWolfSitting());
		setTarget(nullptr);
		return true;
	}
	return false;
}

float Wolf::getHeadHeight()
{
	return bbHeight * 0.8f;
}

int_t Wolf::getMaxSpawnClusterSize()
{
	return 8;
}

bool Wolf::isWolfSitting() const { return (wolfFlags & WOLF_SITTING) != 0; }
void Wolf::setWolfSitting(bool sitting) { wolfFlags = sitting ? (wolfFlags | WOLF_SITTING) : (wolfFlags & ~WOLF_SITTING); }
bool Wolf::isWolfAngry() const { return (wolfFlags & WOLF_ANGRY) != 0; }
void Wolf::setWolfAngry(bool angry) { wolfFlags = angry ? (wolfFlags | WOLF_ANGRY) : (wolfFlags & ~WOLF_ANGRY); }
bool Wolf::isWolfTamed() const { return (wolfFlags & WOLF_TAMED) != 0; }
void Wolf::setWolfTamed(bool tamed) { wolfFlags = tamed ? (wolfFlags | WOLF_TAMED) : (wolfFlags & ~WOLF_TAMED); }
const jstring &Wolf::getWolfOwner() const { return owner; }
void Wolf::setWolfOwner(const jstring &newOwner) { owner = newOwner; }

float Wolf::getTailRotation() const
{
	if (isWolfAngry())
		return Mth::PI * 0.49f;
	return isWolfTamed() ? (0.55f - (20 - health) * 0.02f) * Mth::PI : Mth::PI * 0.2f;
}

float Wolf::getInterestedAngle(float a) const
{
	return (interestedAngleOld + (interestedAngle - interestedAngleOld) * a) * 0.15f * Mth::PI;
}

void Wolf::addAdditionalSaveData(CompoundTag &tag)
{
	Animal::addAdditionalSaveData(tag);
	tag.putBoolean(u"Angry", isWolfAngry());
	tag.putBoolean(u"Sitting", isWolfSitting());
	tag.putString(u"Owner", owner);
}

void Wolf::readAdditionalSaveData(CompoundTag &tag)
{
	Animal::readAdditionalSaveData(tag);
	setWolfAngry(tag.getBoolean(u"Angry"));
	setWolfSitting(tag.getBoolean(u"Sitting"));
	owner = tag.getString(u"Owner");
	if (!owner.empty())
		setWolfTamed(true);
}

bool Wolf::canDespawn()
{
	return !isWolfTamed();
}

bool Wolf::isMovementCeased()
{
	return isWolfSitting();
}

std::shared_ptr<Entity> Wolf::findAttackTarget()
{
	if (isWolfAngry())
		return std::static_pointer_cast<Entity>(level.getNearestPlayer(*this, 16.0));
	return nullptr;
}

void Wolf::checkHurtTarget(Entity &entity, float distance)
{
	if (attackTime <= 0 && distance < 1.5f && entity.bb.y1 > bb.y0 && entity.bb.y0 < bb.y1)
	{
		attackTime = 20;
		entity.hurt(this, isWolfTamed() ? 4 : 2);
	}
}

void Wolf::updateAi()
{
	std::shared_ptr<Player> ownerPlayer;
	if (isWolfTamed() && !owner.empty())
	{
		for (const auto &player : level.players)
		{
			if (player != nullptr && player->name == owner)
			{
				ownerPlayer = player;
				break;
			}
		}
	}
	if (!getTarget() && !hasPath() && isWolfTamed() && riding == nullptr)
	{
		if (ownerPlayer != nullptr)
		{
			float ownerDistance = distanceTo(*ownerPlayer);
			if (ownerDistance > 5.0f)
				setPath(level.getPathToEntity(*this, *ownerPlayer, 16.0f));
			if (!hasPath() && ownerDistance > 12.0f)
			{
				int_t baseX = Mth::floor(ownerPlayer->x) - 2;
				int_t baseZ = Mth::floor(ownerPlayer->z) - 2;
				int_t baseY = Mth::floor(ownerPlayer->bb.y0);
				for (int_t x = 0; x <= 4; ++x)
				{
					for (int_t z = 0; z <= 4; ++z)
					{
						if ((x < 1 || z < 1 || x > 3 || z > 3)
							&& level.isSolidTile(baseX + x, baseY - 1, baseZ + z)
							&& !level.isSolidTile(baseX + x, baseY, baseZ + z)
							&& !level.isSolidTile(baseX + x, baseY + 1, baseZ + z))
						{
							moveTo(baseX + x + 0.5f, baseY, baseZ + z + 0.5f, yRot, xRot);
							goto wolf_follow_done;
						}
					}
				}
			}
		}
		else if (!isInWater())
		{
			setWolfSitting(true);
		}
	}
	else if (getTarget() == nullptr && !hasPath() && !isWolfTamed() && level.random.nextInt(100) == 0)
	{
		AABB *huntBox = bb.grow(16.0, 4.0, 16.0);
		const auto &nearby = level.getEntities(this, *huntBox);
		for (const auto &entity : nearby)
		{
			if (dynamic_cast<Sheep *>(entity.get()) != nullptr)
			{
				setTarget(entity);
				break;
			}
		}
	}

wolf_follow_done:
	Animal::updateAi();
	interestedAngleOld = interestedAngle;
	looksWithInterest = false;
	auto target = getTarget();
	if (target != nullptr && !hasPath() && !isWolfAngry())
	{
		if (auto player = std::dynamic_pointer_cast<Player>(target))
		{
			ItemInstance *selected = player->getSelectedItem();
			if (selected != nullptr)
			{
				if (!isWolfTamed() && selected->itemID == Items::bone->getShiftedIndex())
					looksWithInterest = true;
				else if (isWolfTamed())
					if (auto food = dynamic_cast<ItemFood *>(selected->getItem()))
						looksWithInterest = food->isWolfsFavoriteMeat();
			}
		}
	}
	interestedAngle += ((looksWithInterest ? 1.0f : 0.0f) - interestedAngle) * 0.4f;
	if (isInWater())
		setWolfSitting(false);
}

jstring Wolf::getAmbientSound()
{
	if (isWolfAngry())
		return u"mob.wolf.growl";
	if (random.nextInt(3) == 0)
		return isWolfTamed() && health < 10 ? u"mob.wolf.whine" : u"mob.wolf.panting";
	return u"mob.wolf.bark";
}

jstring Wolf::getHurtSound() { return u"mob.wolf.hurt"; }
jstring Wolf::getDeathSound() { return u"mob.wolf.death"; }
float Wolf::getSoundVolume() { return 0.4f; }
int_t Wolf::getDeathLoot() { return -1; }
