// Projectile ray origin, collision filtering, authority and impact-tick regressions.

#include "world/level/tile/FlowerTile.h"
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "world/entity/projectile/EntityArrow.h"
#include "world/entity/projectile/EntitySnowball.h"
#include "world/entity/projectile/EntityThrownEgg.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/PistonBaseTile.h"
#include "world/level/tile/Tile.h"
#include "world/phys/AABB.h"
#include "world/phys/HitResult.h"
#include "world/phys/Vec3.h"

namespace ProjectilesAuditCases
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "projectiles-cases FAILED: " << message << '\n';
	return condition;
}

bool near(double a, double b)
{
	return std::abs(a - b) < 1.0e-9;
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
	jstring gatherStats() override { return u"projectiles-cases"; }
};

class World : public Level
{
public:
	std::shared_ptr<Source> source = std::make_shared<Source>();
	std::map<std::tuple<int_t, int_t, int_t>, int_t> tileData;

	World() : Level(u"projectiles-cases", Dimension::Id_Normal, 987654321LL, false)
	{
		setChunkSource(source);
		for (int_t cx = -1; cx <= 1; ++cx)
			for (int_t cz = -1; cz <= 1; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
		AABB::resetPool();
		Vec3::resetPool();
	}

	void tile(int_t x, int_t y, int_t z, int_t id)
	{
		source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(id);
	}

	int_t getData(int_t x, int_t y, int_t z) override
	{
		const auto it = tileData.find(std::make_tuple(x, y, z));
		return it == tileData.end() ? 0 : it->second;
	}
};

// Something the projectiles are allowed to hit. hurt() reports false so the
// arrow takes its rebound branch instead of dying, which keeps the post-impact
// state readable.
class Target : public Entity
{
public:
	int_t hurtCount = 0;

	Target(Level &level, double px, double py, double pz) : Entity(level)
	{
		setSize(0.1f, 0.2f);
		heightOffset = 0.0f;
		setPos(px, py, pz);
	}

	bool isPickable() override { return true; }
	bool hurt(Entity *, int_t) override
	{
		++hurtCount;
		return false;
	}
};

struct ProbeArrow : public EntityArrow
{
	using EntityArrow::EntityArrow;
	using EntityArrow::inData;
	using EntityArrow::inGround;
	using EntityArrow::inTile;
	using EntityArrow::ticksInAir;
	using EntityArrow::ticksInGround;
	using EntityArrow::xTile;
	using EntityArrow::yTile;
	using EntityArrow::zTile;
};

struct ProbeSnowball : public EntitySnowball
{
	using EntitySnowball::EntitySnowball;
	using EntitySnowball::inGround;
	using EntitySnowball::ticksInAir;
};

struct ProbeEgg : public EntityThrownEgg
{
	using EntityThrownEgg::EntityThrownEgg;
	using EntityThrownEgg::inGround;
	using EntityThrownEgg::ticksInAir;
};

// F058. Level::clip walks its `from` vector to the last cell boundary it
// crossed. Both targets below sit entirely inside the projectile's starting
// cell, so a reused start vector cannot reach them at all.
bool entityRayStartsAtTheProjectile()
{
	bool ok = true;

	{
		World world;
		auto target = std::make_shared<Target>(world, 0.6, 65.0, 0.5);
		ok &= expect(world.addEntity(target), "F058: the arrow target must join the level");

		ProbeArrow arrow(world, 0.2, 65.0, 0.5);
		arrow.xd = 1.0;
		arrow.yd = 0.0;
		arrow.zd = 0.0;
		arrow.tick();

		// Grown target box spans x 0.25..0.95; the cell boundary the DDA stops
		// on is x = 1.0.
		ok &= expect(target->hurtCount == 1, "F058 arrow: entity ray must start at the arrow, not the crossed cell boundary");
		ok &= expect(arrow.ticksInAir == 0, "F058 arrow: a survived hit resets flight time");
		ok &= expect(arrow.xd < 0.0, "F058 arrow: a survived hit reverses the arrow");
		ok &= expect(!arrow.removed, "F058 arrow: a survived hit keeps the arrow alive");
	}

	{
		World world;
		auto target = std::make_shared<Target>(world, 0.6, 65.0, 0.5);
		ok &= expect(world.addEntity(target), "F058: the snowball target must join the level");

		ProbeSnowball snowball(world, 0.2, 65.0, 0.5);
		snowball.xd = 1.0;
		snowball.yd = 0.0;
		snowball.zd = 0.0;
		snowball.tick();

		ok &= expect(target->hurtCount == 1, "F058 snowball: entity ray must start at the snowball");
		ok &= expect(snowball.removed, "F058 snowball: an entity hit consumes the snowball");
	}

	std::cout << "projectiles-cases F058 reset ray endpoints: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F059, first half. Java's Arrow asks for raycast(from, to, false, true); the
// second flag drops tiles that have no collision box. Flowers are the cheapest
// such tile that still reports mayPick() == true, so without the flag the arrow
// embeds itself in one.
bool arrowIgnoresTilesWithoutCollisionBox()
{
	bool ok = true;

	{
		World world;
		world.tile(1, 65, 0, Tile::flower.id);

		ProbeArrow arrow(world, 0.1, 65.3, 0.5);
		arrow.xd = 1.5;
		arrow.yd = 0.0;
		arrow.zd = 0.0;
		arrow.tick();

		// The flower's pick shape is x/z 0.3..0.7, y 0.0..0.6, so the ray does
		// cross it: only the ignore-no-AABB flag keeps the arrow flying.
		ok &= expect(!arrow.inGround, "F059 arrow: a tile without a collision box must not stop the arrow");
		ok &= expect(near(arrow.x, 1.6), "F059 arrow: the arrow keeps its whole step through a flower");
	}

	{
		World world;
		world.tile(1, 65, 0, Tile::cobblestone.id);

		ProbeArrow arrow(world, 0.1, 65.3, 0.5);
		arrow.xd = 1.5;
		arrow.yd = 0.0;
		arrow.zd = 0.0;
		arrow.tick();

		ok &= expect(arrow.inGround, "F059 arrow: ordinary solids must still stop the arrow");
		ok &= expect(arrow.xTile == 1 && arrow.yTile == 65 && arrow.zTile == 0, "F059 arrow: the stopped arrow records the struck tile");
		ok &= expect(arrow.arrowShake == 7, "F059 arrow: a tile impact arms the shake counter");
	}

	std::cout << "projectiles-cases F059 ignore-no-AABB: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F059, second half. Tile shape state is shared and mutable. Java refreshes it
// with updateShape before reading the embedded-arrow AABB; without that the
// test runs against whatever orientation some earlier lookup left behind.
// The piston base is the reference case here: its shape is data-dependent and,
// unlike DoorTile/TrapDoorTile, its getAABB does not refresh on its own, so the
// arrow is the only thing that can do it.
bool arrowRefreshesEmbeddedTileShape()
{
	bool ok = true;
	World world;
	world.tile(3, 65, 3, Tile::pistonBase.id);

	// Leave the shared piston bounds describing data 9 (powered, facing up:
	// y 65.0..65.75), the way any unrelated piston lookup during the tick would.
	world.tileData[std::make_tuple(3, 65, 3)] = 9;
	Tile::pistonBase.updateShape(world, 3, 65, 3);
	world.tileData[std::make_tuple(3, 65, 3)] = 8;

	// data 8 is powered facing down: y 65.25..66.0, so only a refreshed shape
	// contains the arrow.
	ProbeArrow arrow(world, 3.5, 65.9, 3.5);
	arrow.xTile = 3;
	arrow.yTile = 65;
	arrow.zTile = 3;
	arrow.inTile = Tile::pistonBase.id;
	arrow.inData = 8;
	arrow.tick();

	ok &= expect(arrow.inGround, "F059 arrow: the embedded test must refresh the tile shape before reading its AABB");
	ok &= expect(arrow.ticksInGround == 1, "F059 arrow: a refreshed match keeps the arrow embedded and ages it");
	ok &= expect(arrow.ticksInAir == 0, "F059 arrow: an embedded arrow does not run the flight path");

	std::cout << "projectiles-cases F059 shape refresh: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F062. Java encloses the snowball/egg entity search in !isOnline. The geometry
// here stays inside one cell so Level::clip returns without touching `from`,
// which keeps this case independent of F058.
bool thrownProjectilesGateEntityHitsOnAuthority()
{
	bool ok = true;

	{
		World world;
		auto target = std::make_shared<Target>(world, 0.62, 65.0, 0.5);
		ok &= expect(world.addEntity(target), "F062: the offline target must join the level");

		ProbeSnowball snowball(world, 0.2, 65.0, 0.5);
		snowball.xd = 0.5;
		snowball.yd = 0.0;
		snowball.zd = 0.0;
		snowball.tick();

		ok &= expect(target->hurtCount == 1, "F062 snowball: an offline throw resolves entity hits");
		ok &= expect(snowball.removed, "F062 snowball: an entity hit consumes the snowball");
	}

	{
		World world;
		world.isOnline = true;
		auto target = std::make_shared<Target>(world, 0.62, 65.0, 0.5);
		ok &= expect(world.addEntity(target), "F062: the online target must join the level");

		ProbeSnowball snowball(world, 0.2, 65.0, 0.5);
		snowball.xd = 0.5;
		snowball.yd = 0.0;
		snowball.zd = 0.0;
		snowball.tick();

		ok &= expect(target->hurtCount == 0, "F062 snowball: an online client must not resolve entity hits");
		ok &= expect(!snowball.removed, "F062 snowball: an online client must not consume the snowball on an entity");
		ok &= expect(near(snowball.x, 0.7), "F062 snowball: an unimpeded online throw completes its step");
	}

	std::cout << "projectiles-cases F062 online gate: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F062, continued. remove() only sets a flag; Java runs the rest of the tick
// after an impact. The egg additionally keeps hatching server-side only.
bool impactTickContinuesAfterRemoval()
{
	bool ok = true;

	{
		World world;
		auto target = std::make_shared<Target>(world, 0.62, 65.0, 0.5);
		ok &= expect(world.addEntity(target), "F062: the impact-tick target must join the level");

		ProbeSnowball snowball(world, 0.2, 65.0, 0.5);
		snowball.xd = 0.5;
		snowball.yd = 0.0;
		snowball.zd = 0.0;
		snowball.tick();

		ok &= expect(snowball.removed, "F062 snowball: the impact removes the snowball");
		ok &= expect(near(snowball.x, 0.7), "F062 snowball: the impact tick still applies the remaining movement");
		ok &= expect(snowball.yd < 0.0, "F062 snowball: the impact tick still applies drag and gravity");
		ok &= expect(near(snowball.bb.x0, 0.7 - 0.125), "F062 snowball: the impact tick still ends in setPos, so the box follows the new position");
	}

	{
		World world;
		world.isOnline = true;
		world.tile(1, 65, 0, Tile::cobblestone.id);
		const std::size_t before = world.entities.size();

		ProbeEgg egg(world, 0.1, 65.3, 0.5);
		egg.xd = 1.5;
		egg.yd = 0.0;
		egg.zd = 0.0;
		egg.tick();

		ok &= expect(egg.removed, "F062 egg: a tile impact still consumes the egg online");
		ok &= expect(world.entities.size() == before, "F062 egg: hatching stays authoritative and never runs on a client");
		ok &= expect(near(egg.x, 1.6), "F062 egg: the impact tick still applies the remaining movement");
		ok &= expect(egg.yd < 0.0, "F062 egg: the impact tick still applies drag and gravity");
	}

	std::cout << "projectiles-cases F062 impact continuation: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F066. Java narrows the three impact deltas through float before storing them
// back into the double velocity. 1.0 - 0.1 is not representable in a float, so
// the two spellings are distinguishable at the very first tick.
bool arrowImpactDeltasNarrowToFloat()
{
	bool ok = true;
	World world;
	world.tile(1, 65, 0, Tile::cobblestone.id);

	ProbeArrow arrow(world, 0.1, 65.5, 0.5);
	arrow.xd = 1.5;
	arrow.yd = 0.0;
	arrow.zd = 0.0;
	arrow.tick();

	// The clip lands on the tile's west face, exactly x = 1.0.
	const double narrowed = static_cast<double>(static_cast<float>(1.0 - 0.1));
	const double full = 1.0 - 0.1;

	ok &= expect(narrowed != full, "F066 arrow: the probe delta must actually differ between float and double");
	ok &= expect(arrow.inGround && arrow.xTile == 1, "F066 arrow: the arrow embeds in the cobblestone");
	ok &= expect(arrow.xd == narrowed * 0.99f, "F066 arrow: impact deltas narrow through float exactly as Java does");
	ok &= expect(arrow.xd != full * 0.99f, "F066 arrow: impact deltas must not stay at double precision");
	ok &= expect(arrow.zd == 0.0, "F066 arrow: a square-on impact leaves no lateral delta");

	std::cout << "projectiles-cases F066 impact precision: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}


bool run()
{
	bool ok = true;
	ok &= entityRayStartsAtTheProjectile();
	ok &= arrowIgnoresTilesWithoutCollisionBox();
	ok &= arrowRefreshesEmbeddedTileShape();
	ok &= thrownProjectilesGateEntityHitsOnAuthority();
	ok &= impactTickContinuesAfterRemoval();
	ok &= arrowImpactDeltasNarrowToFloat();
	std::cout << "projectiles-cases: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}


bool runAuditProjectilesCases()
{
	return ProjectilesAuditCases::run();
}
