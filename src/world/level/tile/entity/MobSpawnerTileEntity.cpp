#include "world/level/tile/entity/MobSpawnerTileEntity.h"

#include <memory>
#include <typeinfo>
#include <vector>

#include "world/entity/EntityIO.h"
#include "world/entity/Mob.h"
#include "world/entity/monster/Giant.h"
#include "world/entity/monster/Monster.h"
#include "world/entity/monster/Zombie.h"
#include "world/level/Level.h"
#include "world/phys/AABB.h"

static int_t countInstancesOfClass(const std::vector<std::shared_ptr<Entity>> &entities, const Entity &prototype)
{
	const std::type_info &spawnClass = typeid(prototype);
	int_t count = 0;
	if (spawnClass == typeid(Mob))
	{
		for (const auto &other : entities)
			count += dynamic_cast<Mob *>(other.get()) != nullptr;
	}
	else if (spawnClass == typeid(Monster))
	{
		for (const auto &other : entities)
			count += dynamic_cast<Monster *>(other.get()) != nullptr;
	}
	else if (spawnClass == typeid(Zombie))
	{
		// Java's Giant extends EntityMob, despite the native Giant's Zombie base.
		for (const auto &other : entities)
			count += dynamic_cast<Zombie *>(other.get()) != nullptr && dynamic_cast<Giant *>(other.get()) == nullptr;
	}
	else
	{
		for (const auto &other : entities)
			count += typeid(*other) == spawnClass;
	}
	return count;
}

MobSpawnerTileEntity::MobSpawnerTileEntity()
{
	spawnDelay = 20;
}

bool MobSpawnerTileEntity::isNearPlayer() const
{
	if (level == nullptr)
		return false;
	for (const auto &player : level->players)
	{
		double dx = player->x - (x + 0.5);
		double dy = player->y - (y + 0.5);
		double dz = player->z - (z + 0.5);
		if (dx * dx + dy * dy + dz * dz < 16.0 * 16.0)
			return true;
	}
	return false;
}

void MobSpawnerTileEntity::tick()
{
	if (level == nullptr)
		return;

	oSpin = spin;
	if (!isNearPlayer())
		return;

	double px = static_cast<float>(x) + level->random.nextFloat();
	double py = static_cast<float>(y) + level->random.nextFloat();
	double pz = static_cast<float>(z) + level->random.nextFloat();
	level->addParticle(u"smoke", px, py, pz, 0.0, 0.0, 0.0);
	level->addParticle(u"flame", px, py, pz, 0.0, 0.0, 0.0);

	spin += static_cast<double>(1000.0f / (static_cast<float>(spawnDelay) + 200.0f));
	while (spin > 360.0)
	{
		spin -= 360.0;
		oSpin -= 360.0;
	}

	// MobSpawnerTileEntity.java:37 - the countdown and the spawn attempts are
	// server-authoritative; the spin and particles above are not
	if (level->isOnline)
		return;

	if (spawnDelay == -1)
		resetDelay();

	if (spawnDelay > 0)
	{
		spawnDelay--;
		return;
	}

	for (int_t attempt = 0; attempt < 4; attempt++)
	{
		std::shared_ptr<Entity> entity = EntityIO::newEntity(entityId, *level);
		Mob *mob = dynamic_cast<Mob *>(entity.get());
		if (mob == nullptr)
			return;

		// spawn cap: at most 6 of the spawned class within an 8/4/8 box
		AABB box(x, y, z, x + 1, y + 1, z + 1);
		AABB *grown = box.grow(8.0, 4.0, 8.0);
		if (countInstancesOfClass(level->getEntities(nullptr, *grown), *mob) >= 6)
		{
			resetDelay();
			return;
		}

		const double sxa = level->random.nextDouble();
		const double sxb = level->random.nextDouble();
		double sx = x + (sxa - sxb) * 4.0;
		double sy = y + level->random.nextInt(3) - 1;
		const double sza = level->random.nextDouble();
		const double szb = level->random.nextDouble();
		double sz = z + (sza - szb) * 4.0;
		mob->moveTo(sx, sy, sz, level->random.nextFloat() * 360.0f, 0.0f);
		if (mob->canSpawn())
		{
			level->addEntity(entity);

			for (int_t i = 0; i < 20; i++)
			{
				double sparkX = x + 0.5 + (level->random.nextFloat() - 0.5) * 2.0;
				double sparkY = y + 0.5 + (level->random.nextFloat() - 0.5) * 2.0;
				double sparkZ = z + 0.5 + (level->random.nextFloat() - 0.5) * 2.0;
				level->addParticle(u"smoke", sparkX, sparkY, sparkZ, 0.0, 0.0, 0.0);
				level->addParticle(u"flame", sparkX, sparkY, sparkZ, 0.0, 0.0, 0.0);
			}

			mob->spawnAnim();
			resetDelay();
		}
	}
}

void MobSpawnerTileEntity::resetDelay()
{
	if (level == nullptr)
		return;
	spawnDelay = 200 + level->random.nextInt(600);
}

void MobSpawnerTileEntity::load(CompoundTag &tag)
{
	TileEntity::load(tag);
	entityId = tag.getString(u"EntityId");
	spawnDelay = tag.getShort(u"Delay");
}

void MobSpawnerTileEntity::save(CompoundTag &tag)
{
	TileEntity::save(tag);
	tag.putString(u"EntityId", entityId);
	tag.putShort(u"Delay", static_cast<short_t>(spawnDelay));
}
