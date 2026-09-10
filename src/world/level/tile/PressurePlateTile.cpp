#include "world/level/tile/PressurePlateTile.h"

#include "world/level/Level.h"
#include "world/level/material/Material.h"
#include "world/entity/Mob.h"
#include "world/entity/player/Player.h"
#include "world/phys/AABB.h"
#include "java/Random.h"

// b173: BlockPressurePlate — Material circuits (stone) or wood, ticking, sensitivity by subtype

namespace PressurePlateDetail
{
	static bool isMobEntity(Entity &entity)
	{
		return dynamic_cast<Mob *>(&entity) != nullptr;
	}
	static bool isPlayerEntity(Entity &entity)
	{
		return dynamic_cast<Player *>(&entity) != nullptr;
	}
}

PressurePlateTile::PressurePlateTile(int_t id, int_t tex, Sensitivity sensitivity, const Material &material)
	: Tile(id, tex, material), sensitivity(sensitivity)
{
	setTicking(true);
	setShape(1.0f / 16.0f, 0.0f, 1.0f / 16.0f, 15.0f / 16.0f, 0.03125f, 15.0f / 16.0f);
	updateCachedProperties();
}

bool PressurePlateTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt — a normal cube below is required
	return level.isBlockNormalCube(x, y - 1, z);
}

void PressurePlateTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	(void)tile;
	// b173: onNeighborBlockChange — drop if support gone
	if (!level.isBlockNormalCube(x, y - 1, z))
	{
		spawnResources(level, x, y, z, level.getData(x, y, z));
		level.setTile(x, y, z, 0);
	}
}

void PressurePlateTile::entityInside(Level &level, int_t x, int_t y, int_t z, Entity &entity)
{
	(void)entity;
	// b173: onEntityCollidedWithBlock — the trigger belongs to the server, and a
	// plate that already reads pressed needs no re-evaluation here
	if (level.isOnline)
		return;
	if (level.getData(x, y, z) == 1)
		return;
	checkPressed(level, x, y, z);
}

void PressurePlateTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	(void)random;
	// b173: updateTick — re-evaluate state if still pressed
	if (level.isOnline)
		return;
	if (level.getData(x, y, z) == 0)
		return;
	checkPressed(level, x, y, z);
}

void PressurePlateTile::checkPressed(Level &level, int_t x, int_t y, int_t z)
{
	// b173: checkPressed — the entity query itself is the filter, so entities in
	// their death animation still hold a plate down
	bool wasPressed = level.getData(x, y, z) == 1;
	bool pressed = false;
	float f = 0.125f;

	if (sensitivity == Sensitivity::EVERYTHING)
	{
		AABB *plateAABB = AABB::newTemp(static_cast<float>(x) + f, y, static_cast<float>(z) + f, static_cast<float>(x + 1) - f, static_cast<double>(y) + 0.25, static_cast<float>(z + 1) - f);
		pressed = !level.getEntities(nullptr, *plateAABB).empty();
	}
	if (sensitivity == Sensitivity::MOBS)
	{
		AABB *plateAABB = AABB::newTemp(static_cast<float>(x) + f, y, static_cast<float>(z) + f, static_cast<float>(x + 1) - f, static_cast<double>(y) + 0.25, static_cast<float>(z + 1) - f);
		pressed = !level.getEntitiesOfCondition(PressurePlateDetail::isMobEntity, *plateAABB).empty();
	}
	if (sensitivity == Sensitivity::PLAYERS)
	{
		AABB *plateAABB = AABB::newTemp(static_cast<float>(x) + f, y, static_cast<float>(z) + f, static_cast<float>(x + 1) - f, static_cast<double>(y) + 0.25, static_cast<float>(z + 1) - f);
		pressed = !level.getEntitiesOfCondition(PressurePlateDetail::isPlayerEntity, *plateAABB).empty();
	}

	if (pressed && !wasPressed)
	{
		level.setData(x, y, z, 1);
		level.notifyBlocksOfNeighborChange(x, y, z, id);
		level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
		level.setTilesDirty(x, y, z, x, y, z);
		level.playSoundEffect(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.1, static_cast<double>(z) + 0.5, u"random.click", 0.3f, 0.6f);
	}

	if (!pressed && wasPressed)
	{
		level.setData(x, y, z, 0);
		level.notifyBlocksOfNeighborChange(x, y, z, id);
		level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
		level.setTilesDirty(x, y, z, x, y, z);
		level.playSoundEffect(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.1, static_cast<double>(z) + 0.5, u"random.click", 0.3f, 0.5f);
	}

	if (pressed)
		level.scheduleBlockUpdate(x, y, z, id, getTickDelay());
}

bool PressurePlateTile::getSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	(void)dir;
	return level.getData(x, y, z) > 0;
}

bool PressurePlateTile::getDirectSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	return level.getData(x, y, z) > 0 && dir == 1;
}

void PressurePlateTile::onRemove(Level &level, int_t x, int_t y, int_t z)
{
	int_t data = level.getData(x, y, z);
	if (data > 0)
	{
		level.notifyBlocksOfNeighborChange(x, y, z, id);
		level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
	}
}

void PressurePlateTile::updateShape(LevelSource &level, int_t x, int_t y, int_t z)
{
	int_t data = level.getData(x, y, z);
	if (data == 1)
	{
		// Depressed
		setShape(1.0f / 16.0f, 0.0f, 1.0f / 16.0f, 15.0f / 16.0f, 0.03125f, 15.0f / 16.0f);
	}
	else
	{
		// Raised
		setShape(1.0f / 16.0f, 0.0f, 1.0f / 16.0f, 15.0f / 16.0f, 1.0f / 16.0f, 15.0f / 16.0f);
	}
}

void PressurePlateTile::updateDefaultShape()
{
	float width = 0.5f;
	float height = 2.0f / 16.0f;
	float depth = 0.5f;
	setShape(0.5f - width, 0.5f - height, 0.5f - depth, 0.5f + width, 0.5f + height, 0.5f + depth);
}
