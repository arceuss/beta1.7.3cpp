#pragma once

#include "world/entity/animal/Animal.h"

class Wolf : public Animal
{
private:
	byte_t wolfFlags = 0;
	jstring owner;
	bool looksWithInterest = false;
	float interestedAngle = 0.0f;
	float interestedAngleOld = 0.0f;

public:
	Wolf(Level &level);
	jstring getEncodeId() const override { return u"Wolf"; }
	jstring getTexture() override;
	bool hurt(Entity *source, int_t damage) override;
	bool interact(Player &player) override;
	float getHeadHeight() override;
	int_t getMaxSpawnClusterSize() override;

	bool isWolfSitting() const;
	void setWolfSitting(bool sitting);
	bool isWolfAngry() const;
	void setWolfAngry(bool angry);
	bool isWolfTamed() const;
	void setWolfTamed(bool tamed);
	const jstring &getWolfOwner() const;
	void setWolfOwner(const jstring &owner);
	float getTailRotation() const;
	float getInterestedAngle(float a) const;

protected:
	void addAdditionalSaveData(CompoundTag &tag) override;
	void readAdditionalSaveData(CompoundTag &tag) override;
	bool canDespawn() override;
	bool isMovementCeased() override;
	std::shared_ptr<Entity> findAttackTarget() override;
	void checkHurtTarget(Entity &entity, float distance) override;
	void updateAi() override;
	jstring getAmbientSound() override;
	jstring getHurtSound() override;
	jstring getDeathSound() override;
	float getSoundVolume() override;
	int_t getDeathLoot() override;
};
