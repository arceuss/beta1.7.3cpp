#include "world/level/tile/entity/MobSpawnerTileEntity.h"

#include "world/level/Level.h"

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

	double px = x + level->random.nextFloat();
	double py = y + level->random.nextFloat();
	double pz = z + level->random.nextFloat();
	level->addParticle(u"smoke", px, py, pz, 0.0, 0.0, 0.0);
	level->addParticle(u"flame", px, py, pz, 0.0, 0.0, 0.0);

	spin += 1000.0 / (spawnDelay + 200.0);
	while (spin > 360.0)
	{
		spin -= 360.0;
		oSpin -= 360.0;
	}

	// TODO: Mob spawning is not fully implemented yet.
	// The code below follows Beta 1.7.3 parity but requires:
	// - Entity factory system (EntityIO::newEntity equivalent)
	// - Mob class with canSpawn(), moveTo(), spawnAnim()
	// - Level::getEntitiesOfClass or equivalent for spawn cap checking
	// - Proper entity addition to the world
	//
	// if (spawnDelay == -1)
	//     resetDelay();
	//
	// if (spawnDelay > 0)
	// {
	//     spawnDelay--;
	//     return;
	// }
	//
	// constexpr int_t attempts = 4;
	// for (int_t i = 0; i < attempts; i++)
	// {
	//     // TODO: create entity from entityId
	//     // TODO: check spawn cap (max 6 of same type within 8/4/8 box)
	//     // TODO: random position near spawner
	//     // TODO: if entity->canSpawn(), add to world + particles + resetDelay()
	// }
	//
	// TileEntity::tick();
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
