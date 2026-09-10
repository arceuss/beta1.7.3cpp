// Dropped item, falling block and primed TNT regressions.

#include "world/level/tile/FireTile.h"
#include "world/level/tile/IceTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/TNTTile.h"
#include "world/level/LevelListener.h"
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "nbt/CompoundTag.h"
#include "world/entity/PrimedTNT.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/item/FallingTile.h"
#include "world/entity/player/Player.h"
#include "world/item/ItemInstance.h"
#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/SandTile.h"
#include "world/level/tile/Tile.h"
#include "world/phys/AABB.h"
#include "world/phys/Vec3.h"

namespace AuditLooseEntities
{
namespace Detail
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "audit-looseentities FAILED: " << message << '\n';
	return condition;
}

class Source : public ChunkSource
{
public:
	std::map<std::pair<int_t, int_t>, std::shared_ptr<LevelChunk>> chunks;
	bool hasChunk(int_t x, int_t z) override { return chunks.count({x, z}) != 0; }
	std::shared_ptr<LevelChunk> getChunk(int_t x, int_t z) override { return chunks.at({x, z}); }
	void postProcess(ChunkSource &, int_t, int_t) override {}
	bool save(bool, std::shared_ptr<ProgressListener>) override { return true; }
	bool tick() override { return false; }
	bool shouldSave() override { return false; }
	jstring gatherStats() override { return u"audit-looseentities"; }
};

class World : public Level
{
	std::shared_ptr<Source> source = std::make_shared<Source>();

public:
	World() : Level(u"audit-looseentities", Dimension::Id_Normal, 987654321LL, false)
	{
		setChunkSource(source);
		for (int_t cx = -2; cx <= 2; ++cx)
			for (int_t cz = -2; cz <= 2; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
		AABB::resetPool();
		Vec3::resetPool();
	}

	// Raw placement: no onPlace/onRemove, no neighbor notification. Used where
	// a case needs an exact block layout under the entity under test.
	void raw(int_t x, int_t y, int_t z, int_t id, int_t data = 0)
	{
		source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(id);
		setDataNoUpdate(x, y, z, data);
	}
};

class Recorder : public LevelListener
{
public:
	struct Sound
	{
		jstring name;
		float volume = 0.0f, pitch = 0.0f;
	};
	struct Particle
	{
		jstring name;
	};
	std::vector<Sound> sounds;
	std::vector<Particle> particles;

	void tileChanged(int_t, int_t, int_t) override {}
	void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
	void allChanged() override {}
	void playSound(const jstring &name, double, double, double, float volume, float pitch) override
	{
		sounds.push_back({name, volume, pitch});
	}
	void addParticle(const jstring &name, double, double, double, double, double, double) override
	{
		particles.push_back({name});
	}
	void playMusic(const jstring &, double, double, double, float) override {}
	void entityAdded(std::shared_ptr<Entity>) override {}
	void entityRemoved(std::shared_ptr<Entity>) override {}
	void skyColorChanged() override {}
	void playStreamingMusic(const jstring &, int_t, int_t, int_t) override {}
	void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
	void levelEvent(Player *, int_t, int_t, int_t, int_t, int_t) override {}
};

int_t countItems(World &world)
{
	int_t n = 0;
	for (const auto &entity : world.entities)
		if (std::dynamic_pointer_cast<EntityItem>(entity) != nullptr)
			n++;
	return n;
}

int_t countPrimed(World &world)
{
	int_t n = 0;
	for (const auto &entity : world.entities)
		if (std::dynamic_pointer_cast<PrimedTNT>(entity) != nullptr)
			n++;
	return n;
}

// F055. Java uses life-- <= 0 with life = 80, so the 80 fuse burns for 81
// ticks. A --fuse <= 0 spelling would detonate one tick early.
bool primedFuseTiming()
{
	bool ok = true;
	World world;
	world.isOnline = true;
	PrimedTNT tnt(world, 0.5, 100.5, 0.5);
	tnt.noPhysics = true;
	for (int_t i = 0; i < 80; ++i)
		tnt.tick();
	ok &= expect(!tnt.removed, "F055 online: fuse 80 must still burn after 80 ticks");
	ok &= expect(tnt.fuse == 0, "F055 online: 80 ticks must count the fuse down to 0");
	tnt.tick();
	ok &= expect(tnt.removed, "F055 online: fuse 80 must detonate on tick 81");

	PrimedTNT quick(world, 0.5, 100.5, 0.5);
	quick.noPhysics = true;
	quick.fuse = 1;
	quick.tick();
	ok &= expect(!quick.removed, "F055 online: fuse 1 must survive its first tick");
	quick.tick();
	ok &= expect(quick.removed, "F055 online: fuse 1 must detonate on its second tick");
	{
		World world;
		PrimedTNT boom(world, 0.5, 100.5, 0.5);
		boom.noPhysics = true;
		boom.fuse = 0;
		boom.tick();
		ok &= expect(boom.removed, "F055 offline: fuse 0 must detonate on its first tick");
	}

	std::cout << "audit-looseentities F055 fuse timing: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F055. Spawn contract: placement-blocking flag, pickability, zero shadow,
// Beta upward pop with the odd narrow launch arc, and fuse save round-trip.
bool primedSpawnState()
{
	bool ok = true;
	World world;
	PrimedTNT tnt(world, 0.5, 100.5, 0.5);
	ok &= expect(tnt.blocksBuilding, "F055: primed TNT must block building placement");
	ok &= expect(tnt.isPickable(), "F055: primed TNT must be pickable while alive");
	ok &= expect(tnt.getShadowHeightOffs() == 0.0f, "F055: primed TNT shadow offset must be 0");
	ok &= expect(tnt.fuse == 80, "F055: fresh primed TNT must carry fuse 80");
	ok &= expect(tnt.yd == 0.2f, "F055: primed TNT must pop upward at 0.2");
	const double speed = std::sqrt(tnt.xd * tnt.xd + tnt.zd * tnt.zd);
	ok &= expect(std::abs(speed - 0.02) < 1.0e-6, "F055: launch speed must be the Beta 0.02 ring");
	ok &= expect(std::abs(tnt.xd) <= 0.02 && std::abs(tnt.zd) <= 0.02, "F055: launch components must stay on the 0.02 ring");

	tnt.noPhysics = true;
	for (int_t i = 0; i < 3; ++i)
		tnt.tick();
	CompoundTag tag;
	ok &= expect(tnt.save(tag), "F055: a live primed entity must save");
	PrimedTNT reloaded(world);
	reloaded.load(tag);
	ok &= expect(reloaded.fuse == tnt.fuse && reloaded.fuse == 77, "F055: save/load must preserve a counting fuse");

	std::cout << "audit-looseentities F055 spawn state: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F056. throwTime gates pickup and counts down in tick; age 6000 despawns.
bool itemPickupDelayAndExpiry()
{
	bool ok = true;
	World world;
	EntityItem held(world, 0.5, 70.0, 0.5, ItemInstance(Tile::cobblestone.id, 3, 0));
	Player player(world);
	held.throwTime = 10;
	held.playerTouch(player);
	ok &= expect(!held.removed, "F056: a fresh drop must ignore pickup while throwTime runs");
	ok &= expect(held.item.stackSize == 3, "F056: a gated pickup must leave the stack whole");
	held.throwTime = 0;
	held.playerTouch(player);
	ok &= expect(held.removed, "F056: pickup at throwTime 0 must consume the entity");
	ok &= expect(held.item.stackSize == 0, "F056: a full pickup must empty the stack");

	EntityItem old(world, 0.5, 70.0, 0.5, ItemInstance(Tile::cobblestone.id, 1, 0));
	old.age = 5999;
	old.tick();
	ok &= expect(old.removed, "F056: age 6000 must despawn the drop");

	std::cout << "audit-looseentities F056 pickup and expiry: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F056. Lava resets the fall to an upward bounce and plays the fizz.
bool itemLavaBounce()
{
	bool ok = true;
	World world;
	world.raw(0, 70, 0, Tile::lava.id);
	EntityItem drop(world, 0.5, 70.5, 0.5, ItemInstance(Tile::cobblestone.id, 1, 0));
	drop.xd = 0.0;
	drop.yd = 0.0;
	drop.zd = 0.0;
	Recorder recorder;
	world.addListener(recorder);
	drop.tick();
	world.removeListener(recorder);
	ok &= expect(drop.yd > 0.15, "F056: lava must bounce the drop upward");
	ok &= expect(!recorder.sounds.empty() && recorder.sounds.back().name == u"random.fizz",
		"F056: lava bounce must play the fizz");
	ok &= expect(!drop.removed, "F056: lava bounce must not destroy the drop");

	std::cout << "audit-looseentities F056 lava bounce: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F056. Grounded drag comes from the support tile: ice keeps far more
// horizontal speed than stone. A hardcoded 0.588 would treat both the same.
bool itemGroundFriction()
{
	bool ok = true;
	double stoneXd = 0.0, iceXd = 0.0;
	for (int_t pass = 0; pass < 2; ++pass)
	{
		World world;
		const int_t floorId = pass == 0 ? Tile::cobblestone.id : Tile::ice.id;
		for (int_t dx = -1; dx <= 1; ++dx)
			for (int_t dz = -1; dz <= 1; ++dz)
				world.raw(dx, 64, dz, floorId);
		EntityItem drop(world, 0.5, 66.0, 0.5, ItemInstance(Tile::cobblestone.id, 1, 0));
		drop.xd = 0.0;
		drop.zd = 0.0;
		int_t settled = 0;
		while (!drop.onGround && settled < 100)
		{
			drop.tick();
			settled++;
		}
		ok &= expect(drop.onGround, "F056: the drop must settle on the floor");
		if (!drop.onGround)
			continue;
		drop.xd = 1.0;
		drop.yd = -0.1;
		drop.zd = 0.0;
		drop.tick();
		if (pass == 0)
			stoneXd = drop.xd;
		else
			iceXd = drop.xd;
	}
	ok &= expect(stoneXd > 0.55 && stoneXd < 0.62, "F056: stone drag must be near 0.6 * 0.98");
	ok &= expect(iceXd > 0.93 && iceXd < 0.99, "F056: ice drag must be near 0.98 * 0.98");
	ok &= expect(iceXd > stoneXd + 0.3, "F056: ice must preserve clearly more speed than stone");

	std::cout << "audit-looseentities F056 ground friction: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F057. A supported landing places exactly one block and drops nothing.
bool fallingLandingPlaces()
{
	bool ok = true;
	World world;
	for (int_t dx = -1; dx <= 1; ++dx)
		for (int_t dz = -1; dz <= 1; ++dz)
			world.raw(dx, 64, dz, Tile::cobblestone.id);
	FallingTile fall(world, 0.5, 70.5, 0.5, Tile::sand.id);
	int_t steps = 0;
	while (!fall.removed && steps < 300)
	{
		fall.tick();
		steps++;
	}
	ok &= expect(fall.removed, "F057: a landed fall must finish");
	ok &= expect(world.getTile(0, 65, 0) == Tile::sand.id, "F057: a supported landing must place the carried block");
	ok &= expect(countItems(world) == 0, "F057: a successful landing must drop no item");

	std::cout << "audit-looseentities F057 supported landing: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F057. When the landing write itself fails, Java drops exactly one carried
// item instead of deleting the block. Resting at y 128 makes setTile fail
// its height guard while mayPlace and support both pass.
bool fallingUnwritableLandingDrops()
{
	bool ok = true;
	World world;
	for (int_t dx = -1; dx <= 1; ++dx)
		for (int_t dz = -1; dz <= 1; ++dz)
			world.raw(dx, 127, dz, Tile::cobblestone.id);
	FallingTile fall(world, 0.5, 128.4, 0.5, Tile::sand.id);
	int_t steps = 0;
	while (!fall.removed && steps < 300)
	{
		fall.tick();
		steps++;
	}
	ok &= expect(fall.removed, "F057: an unwritable landing must still finish");
	ok &= expect(world.getTile(0, 128, 0) == 0, "F057: a failed landing must place nothing");
	ok &= expect(countItems(world) == 1, "F057: a failed landing must drop exactly one item");

	std::cout << "audit-looseentities F057 unwritable landing: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F057. The airborne timeout drops the carried block as an item.
bool fallingTimeoutDrops()
{
	bool ok = true;
	World world;
	FallingTile fall(world, 0.5, 100.5, 0.5, Tile::sand.id);
	fall.time = 100;
	fall.tick();
	ok &= expect(fall.removed, "F057: a timed-out fall must finish");
	ok &= expect(countItems(world) == 1, "F057: a timed-out fall must drop exactly one item");

	std::cout << "audit-looseentities F057 timeout: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F057. The source cell clears from the post-move position on the way down.
bool fallingClearsSource()
{
	bool ok = true;
	World world;
	world.raw(0, 70, 0, Tile::sand.id);
	FallingTile fall(world, 0.5, 70.5, 0.5, Tile::sand.id);
	fall.tick();
	ok &= expect(world.getTile(0, 70, 0) == 0, "F057: the fall must clear its source cell after moving");

	std::cout << "audit-looseentities F057 source clearing: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F057. SandTile: entity path over loaded chunks, instant teleport path,
// supported rest, and the free-cell rule.
bool sandSlidePaths()
{
	bool ok = true;
	{
		World world;
		world.raw(5, 66, 5, Tile::sand.id);
		Tile::sand.tick(world, 5, 66, 5, world.random);
		ok &= expect(world.getTile(5, 66, 5) == Tile::sand.id, "F057: the entity path must leave the source for the fall to clear");
		int_t falls = 0;
		for (const auto &entity : world.entities)
		{
			auto fall = std::dynamic_pointer_cast<FallingTile>(entity);
			if (fall != nullptr && fall->tile == Tile::sand.id)
				falls++;
		}
		ok &= expect(falls == 1, "F057: free space below with chunks loaded must spawn one fall");
	}
	{
		World world;
		world.raw(6, 65, 6, Tile::cobblestone.id);
		world.raw(6, 66, 6, Tile::sand.id);
		Tile::sand.tick(world, 6, 66, 6, world.random);
		ok &= expect(world.entities.empty(), "F057: supported sand must spawn nothing");
		ok &= expect(world.getTile(6, 66, 6) == Tile::sand.id, "F057: supported sand must stay put");
	}
	{
		World world;
		world.raw(7, 64, 7, Tile::cobblestone.id);
		world.raw(7, 66, 7, Tile::sand.id);
		SandTile::fallInstantly = true;
		Tile::sand.tick(world, 7, 66, 7, world.random);
		SandTile::fallInstantly = false;
		ok &= expect(world.getTile(7, 66, 7) == 0, "F057: instant fall must vacate the source");
		ok &= expect(world.getTile(7, 65, 7) == Tile::sand.id, "F057: instant fall must teleport onto the support");
		ok &= expect(world.entities.empty(), "F057: instant fall must spawn no entity");
	}
	{
		World world;
		world.raw(0, 70, 0, 0);
		world.raw(1, 70, 0, Tile::fire.id);
		world.raw(2, 70, 0, Tile::water.id);
		world.raw(3, 70, 0, Tile::lava.id);
		world.raw(4, 70, 0, Tile::cobblestone.id);
		ok &= expect(SandTile::isFree(world, 0, 70, 0), "F057: air must be free");
		ok &= expect(SandTile::isFree(world, 1, 70, 0), "F057: fire must be free");
		ok &= expect(SandTile::isFree(world, 2, 70, 0), "F057: water must be free");
		ok &= expect(SandTile::isFree(world, 3, 70, 0), "F057: lava must be free");
		ok &= expect(!SandTile::isFree(world, 4, 70, 0), "F057: stone must not be free");
	}

	std::cout << "audit-looseentities F057 sand slide: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F055/S06. destroy splits on the armed bit: unarmed drops the block item
// with pickup delay, armed primes with the fuse sound, online drops nothing.
bool tntDestroyPaths()
{
	bool ok = true;
	{
		World world;
		Tile::tnt.destroy(world, 3, 65, 3, 0);
		ok &= expect(countPrimed(world) == 0, "S06: unarmed TNT must prime nothing");
		ok &= expect(countItems(world) == 1, "S06: unarmed TNT must drop its block item");
		bool delayed = false;
		for (const auto &entity : world.entities)
		{
			auto item = std::dynamic_pointer_cast<EntityItem>(entity);
			if (item != nullptr && item->item.itemID == Tile::tnt.id)
				delayed = item->throwTime == 10;
		}
		ok &= expect(delayed, "S06: the TNT drop must carry the Beta pickup delay");
	}
	{
		World world;
		Recorder recorder;
		world.addListener(recorder);
		Tile::tnt.destroy(world, 3, 65, 3, 1);
		world.removeListener(recorder);
		ok &= expect(countItems(world) == 0, "S06: armed TNT must drop no item");
		ok &= expect(countPrimed(world) == 1, "S06: armed TNT must prime exactly once");
		bool fuse80 = false;
		for (const auto &entity : world.entities)
		{
			auto primed = std::dynamic_pointer_cast<PrimedTNT>(entity);
			if (primed != nullptr)
				fuse80 = primed->fuse == 80;
		}
		ok &= expect(fuse80, "S06: a fresh prime must carry fuse 80");
		bool fused = false;
		for (const auto &sound : recorder.sounds)
			if (sound.name == u"random.fuse")
				fused = true;
		ok &= expect(fused, "S06: priming must play the fuse sound");
	}
	{
		World world;
		world.isOnline = true;
		Tile::tnt.destroy(world, 3, 65, 3, 0);
		Tile::tnt.destroy(world, 3, 65, 3, 1);
		ok &= expect(world.entities.empty(), "S06: destroy must stay server-side");
	}
	{
		World world;
		Tile::tnt.onBlockDestroyedByExplosion(world, 3, 65, 3);
		ok &= expect(countPrimed(world) == 1, "F055: a blast must chain exactly one prime");
		bool ranged = false;
		for (const auto &entity : world.entities)
		{
			auto primed = std::dynamic_pointer_cast<PrimedTNT>(entity);
			if (primed != nullptr)
				ranged = primed->fuse >= 10 && primed->fuse <= 29;
		}
		ok &= expect(ranged, "F055: a chained fuse must land in nextInt(20) + 10");
	}
	{
		World world;
		ok &= expect(Tile::tnt.getResourceCount(world.random) == 0, "S06: TNT harvest must yield no resource die");
	}

	std::cout << "audit-looseentities TNT destroy: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// S06. Flint arms the block data; any other held item leaves it alone.
bool tntFlintAttack()
{
	bool ok = true;
	World world;
	world.raw(2, 65, 2, Tile::tnt.id, 0);
	Player player(world);
	player.inventory.setItem(player.inventory.currentItem, ItemInstance(Items::flintAndSteel->getShiftedIndex(), 1, 0));
	Tile::tnt.attack(world, 2, 65, 2, player);
	ok &= expect(world.getData(2, 65, 2) == 1, "S06: flint must arm the TNT data");
	world.setDataNoUpdate(2, 65, 2, 0);
	player.inventory.setItem(player.inventory.currentItem, ItemInstance(Tile::cobblestone.id, 1, 0));
	Tile::tnt.attack(world, 2, 65, 2, player);
	ok &= expect(world.getData(2, 65, 2) == 0, "S06: a bare hand must not arm TNT");

	std::cout << "audit-looseentities S06 flint attack: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// B06/F002. Neither sand nor TNT restricts admission in Java, so the shared
// base predicate must accept them in air, on any face.
bool baseAdmissionForSandAndTnt()
{
	bool ok = true;
	World world;
	ok &= expect(Tile::sand.mayPlace(world, 0, 70, 0), "B06: sand needs no admission override in air");
	ok &= expect(Tile::sand.mayPlaceOnFace(world, 0, 70, 0, Facing::UP), "B06: sand face admission must follow the base rule");
	ok &= expect(Tile::tnt.mayPlace(world, 0, 70, 0), "B06: TNT needs no admission override in air");
	ok &= expect(Tile::tnt.mayPlaceOnFace(world, 0, 70, 0, Facing::UP), "B06: TNT face admission must follow the base rule");

	std::cout << "audit-looseentities admission: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}
}

bool runAuditLooseEntitiesCases()
{
	using namespace AuditLooseEntities::Detail;
	bool ok = true;
	ok &= primedFuseTiming();
	ok &= primedSpawnState();
	ok &= itemPickupDelayAndExpiry();
	ok &= itemLavaBounce();
	ok &= itemGroundFriction();
	ok &= fallingLandingPlaces();
	ok &= fallingUnwritableLandingDrops();
	ok &= fallingTimeoutDrops();
	ok &= fallingClearsSource();
	ok &= sandSlidePaths();
	ok &= tntDestroyPaths();
	ok &= tntFlintAttack();
	ok &= baseAdmissionForSandAndTnt();
	std::cout << "audit-looseentities: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}
