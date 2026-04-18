#pragma once

#include "world/entity/Mob.h"
#include "world/entity/player/InventoryPlayer.h"
#include "world/level/tile/Tile.h"

#include "java/Type.h"
#include "java/String.h"

class EntityItem;

class Player : public Mob
{
public:
	static constexpr int_t MAX_HEALTH = 20;
	static constexpr int_t SWING_DURATION = 8;

	byte_t userType = 0;
	int_t score = 0;

	float oBob = 0.0f;
	float bob = 0.0f;

	bool swinging = false;
	int_t swingTime = 0;

	jstring name;
	int_t dimension = 0;

	jstring cloakTexture;
	double xCloakO = 0.0, yCloakO = 0.0, zCloakO = 0.0;
	double xCloak = 0.0, yCloak = 0.0, zCloak = 0.0;

	int_t dmgSpill = 0;
	InventoryPlayer inventory = InventoryPlayer(this);

	Player(Level &level);

	void tick() override;

protected:
	virtual void closeContainer();

public:
	virtual void rideTick() override;
	virtual void resetPos() override;

protected:
	virtual void updateAi() override;

public:
	virtual void aiStep() override;

private:
	void touch(Entity &e);

public:
	virtual void swing();
	virtual void attack(const std::shared_ptr<Entity> &entity);
	virtual void respawn();

	float getDestroySpeed(Tile &tile);
	bool canDestroy(Tile &tile);

	void readAdditionalSaveData(CompoundTag &tag) override;
	void addAdditionalSaveData(CompoundTag &tag) override;

	float getHeadHeight() override;
	void die(Entity *source) override;
	bool hurt(Entity *source, int_t dmg) override;

protected:
	void actuallyHurt(int_t dmg) override;

public:
	void interact(const std::shared_ptr<Entity> &entity);
	virtual void take(Entity &entity, int_t count);
	ItemInstance *getSelectedItem();
	void removeSelectedItem();
	void drop();
	void drop(ItemInstance &stack);
	void drop(ItemInstance &stack, bool randomSpread);

protected:
	void reallyDrop(std::shared_ptr<EntityItem> itemEntity);

public:
	bool isPlayer() override { return true; }
};