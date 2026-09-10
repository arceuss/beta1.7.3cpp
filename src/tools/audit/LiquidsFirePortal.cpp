// Liquid flow, fire updates and portal regressions.

#include "world/level/tile/IceTile.h"
#include "world/level/tile/StoneTile.h"
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/Region.h"
#include "world/level/TilePos.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/FireTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/PortalTile.h"
#include "world/level/tile/Tile.h"

namespace AuditLiquidsFirePortal
{
namespace Detail
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "audit-liquidsfireportal FAILED: " << message << '\n';
	return condition;
}

bool near(double a, double b)
{
	return (a - b) < 1.0e-9 && (b - a) < 1.0e-9;
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
	jstring gatherStats() override { return u"audit-liquidsfireportal"; }
};

class Recorder : public LevelListener
{
public:
	std::vector<TilePos> changed;
	std::vector<jstring> particleNames;
	std::vector<jstring> sounds;
	std::vector<std::array<double, 3>> portalAt;

	void tileChanged(int_t x, int_t y, int_t z) override { changed.emplace_back(x, y, z); }
	void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
	void allChanged() override {}
	void playSound(const jstring &name, double, double, double, float, float) override { sounds.push_back(name); }
	void addParticle(const jstring &name, double x, double y, double z, double, double, double) override
	{
		particleNames.push_back(name);
		if (name == u"portal")
			portalAt.push_back({x, y, z});
	}
	void playMusic(const jstring &, double, double, double, float) override {}
	void entityAdded(std::shared_ptr<Entity>) override {}
	void entityRemoved(std::shared_ptr<Entity>) override {}
	void skyColorChanged() override {}
	void playStreamingMusic(const jstring &, int_t, int_t, int_t) override {}
	void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
	void levelEvent(Player *, int_t, int_t, int_t, int_t, int_t) override {}

	int_t countParticles(const jstring &name) const
	{
		int_t n = 0;
		for (const auto &p : particleNames)
			if (p == name)
				++n;
		return n;
	}

	int_t countSounds(const jstring &name) const
	{
		int_t n = 0;
		for (const auto &s : sounds)
			if (s == name)
				++n;
		return n;
	}

	bool sawChanged(int_t x, int_t y, int_t z) const
	{
		for (const TilePos &pos : changed)
			if (pos.x == x && pos.y == y && pos.z == z)
				return true;
		return false;
	}
};

class World : public Level
{
public:
	std::shared_ptr<Source> source = std::make_shared<Source>();
	bool raining = false;

	World() : Level(u"audit-liquidsfireportal", Dimension::Id_Normal, 1234567, false)
	{
		setChunkSource(source);
		for (int_t cx = -1; cx <= 1; ++cx)
			for (int_t cz = -1; cz <= 1; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
	}

	bool isRaining() override { return raining; }

	void put(int_t x, int_t y, int_t z, int_t tile, int_t data)
	{
		source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(tile);
		setDataNoUpdate(x, y, z, data);
	}

	void floor(int_t x0, int_t z0, int_t x1, int_t z1, int_t y, int_t tile)
	{
		for (int_t x = x0; x <= x1; ++x)
			for (int_t z = z0; z <= z1; ++z)
				put(x, y, z, tile, 0);
	}
};

// F050/B29. A falling water cell beside open air gets no downward pull, while
// the same cell beside a stone wall does. Face visibility answers the opposite
// way (air is visible, stone is not), so the old predicate pulled in the open
// pit and this fails before the fix.
bool fallingFlowNeedsSolid()
{
	World open;
	open.floor(-2, -2, 2, 2, 63, Tile::rock.id);
	open.put(0, 64, 0, Tile::water.id, 8);
	Vec3 openFlow = Tile::water.getFlowVector(open, 0, 64, 0);
	bool ok = expect(near(openFlow.x, 0.0) && near(openFlow.y, 0.0) && near(openFlow.z, 0.0),
		"falling water in an open pit has a zero flow vector");

	World walled;
	walled.floor(-2, -2, 2, 2, 63, Tile::rock.id);
	walled.put(0, 64, 0, Tile::water.id, 8);
	walled.put(1, 64, 0, Tile::rock.id, 0);
	Vec3 wallFlow = Tile::water.getFlowVector(walled, 0, 64, 0);
	ok &= expect(near(wallFlow.x, 0.0) && near(wallFlow.y, -1.0) && near(wallFlow.z, 0.0),
		"falling water beside a stone wall pulls straight down");

	World icy;
	icy.floor(-2, -2, 2, 2, 63, Tile::rock.id);
	icy.put(0, 64, 0, Tile::water.id, 8);
	icy.put(1, 64, 0, Tile::ice.id, 0);
	Vec3 iceFlow = Tile::water.getFlowVector(icy, 0, 64, 0);
	ok &= expect(near(iceFlow.x, 0.0) && near(iceFlow.y, 0.0) && near(iceFlow.z, 0.0),
		"ice never counts as solid for the falling-flow check");
	return ok;
}

// F051/B30. The slope lookup only needs the read-only source, so a Region
// over the same cells must agree with the Level instead of being downcast.
bool slopeLevelRegionParity()
{
	World level;
	level.floor(-2, -2, 2, 2, 63, Tile::rock.id);
	level.put(0, 64, 0, Tile::water.id, 0);
	level.put(1, 64, 0, Tile::water.id, 2);

	double direct = LiquidTile::getSlopeAngle(level, 0, 64, 0, Material::water);
	Region region(level, -2, 60, -2, 2, 68, 2);
	double throughRegion = LiquidTile::getSlopeAngle(region, 0, 64, 0, Material::water);

	bool ok = expect(direct > -999.0, "the graded water has a defined slope angle");
	ok &= expect(near(direct, throughRegion), "Level and Region slope angles agree");
	ok &= expect(near(direct, std::atan2(0.0, 2.0) - 1.5707963267948966), "the slope points down the depth gradient");
	return ok;
}

// F052/B31, fizz half. Lava touching water hardens and the mix effect plays
// its sound plus exactly eight large-smoke particles.
bool fizzSmokeAndHarden()
{
	World level;
	level.put(0, 64, 0, Tile::lava.id, 0);
	level.put(1, 64, 0, Tile::water.id, 0);

	Recorder events;
	level.addListener(events);
	Tile::lava.onPlace(level, 0, 64, 0);
	level.removeListener(events);

	bool ok = expect(level.getTile(0, 64, 0) == Tile::obsidian.id, "still lava touching water hardens to obsidian");
	ok &= expect(events.countSounds(u"random.fizz") == 1, "the mix plays one fizz sound");
	ok &= expect(events.countParticles(u"largesmoke") == 8, "the mix spawns eight large-smoke particles");
	return ok;
}

// F053/B28, aging half. An eternal fire ages without notifying neighbours:
// the metadata write is silent and the captured age still drives the tick.
bool fireSilentAging()
{
	World level;
	level.put(2, 63, 2, Tile::netherrack.id, 0);
	level.put(2, 64, 2, Tile::fire.id, 14);
	level.random.setSeed(11);

	Recorder events;
	level.addListener(events);
	Tile::fire.tick(level, 2, 64, 2, level.random);
	level.removeListener(events);

	bool ok = expect(level.getTile(2, 64, 2) == Tile::fire.id, "eternal fire survives its tick");
	int_t age = level.getData(2, 64, 2);
	ok &= expect(age == 14 || age == 15, "age advances by at most one");
	ok &= expect(!events.sawChanged(2, 64, 2), "the aging write does not notify neighbours");
	return ok;
}

// F053/B28, rain half. An exposed fire goes out before touching the RNG, so
// the random state is unchanged across the tick.
bool fireRainExtinguishFirst()
{
	World level;
	level.raining = true;
	int_t fx = -1;
	for (int_t x = 0; x <= 15; ++x)
	{
		if (level.canBlockBeRainedOn(x, 64, 8))
		{
			fx = x;
			break;
		}
	}
	bool ok = expect(fx >= 0, "the test world has a rain-exposed column");
	if (fx < 0)
		return false;

	level.put(fx, 63, 8, Tile::rock.id, 0);
	level.put(fx, 64, 8, Tile::fire.id, 5);
	level.random.setSeed(5);
	ulong_t before = level.random.rawState();
	Tile::fire.tick(level, fx, 64, 8, level.random);
	ok &= expect(level.getTile(fx, 64, 8) == 0, "exposed fire goes out in rain");
	ok &= expect(level.random.rawState() == before, "the rain exit draws nothing from the RNG");
	return ok;
}

// Puts a full obsidian frame for one portal axis. With corners=false the four
// corner cells stay air, matching the usual ten-obsidian Beta frame.
void putPortalFrame(World &level, int_t bx, int_t by, int_t bz, bool alongX, bool corners)
{
	for (int_t fo = -1; fo <= 2; ++fo)
	{
		for (int_t yo = -1; yo <= 3; ++yo)
		{
			bool horizontalEdge = fo == -1 || fo == 2;
			bool verticalEdge = yo == -1 || yo == 3;
			if (horizontalEdge && verticalEdge)
			{
				if (corners)
					level.put(alongX ? bx + fo : bx, by + yo, alongX ? bz : bz + fo, Tile::obsidian.id, 0);
				continue;
			}
			if (horizontalEdge || verticalEdge)
				level.put(alongX ? bx + fo : bx, by + yo, alongX ? bz : bz + fo, Tile::obsidian.id, 0);
		}
	}
}

bool portalInteriorIsPortal(World &level, int_t bx, int_t by, int_t bz, bool alongX)
{
	for (int_t fo = 0; fo < 2; ++fo)
		for (int_t yo = 0; yo < 3; ++yo)
			if (level.getTile(alongX ? bx + fo : bx, by + yo, alongX ? bz : bz + fo) != Tile::portal.id)
				return false;
	return true;
}

// F039/B32. Cornerless frames light in both axes, a missing edge refuses,
// and the full frame still works.
bool portalCornerlessFrames()
{
	World xLevel;
	putPortalFrame(xLevel, 0, 64, 0, true, false);
	bool ok = expect(Tile::portal.trySpawnPortal(xLevel, 0, 64, 0), "cornerless X frame lights from its interior");
	ok &= expect(portalInteriorIsPortal(xLevel, 0, 64, 0, true), "cornerless X frame fills the 2x3 interior");

	World zLevel;
	putPortalFrame(zLevel, 0, 64, 4, false, false);
	ok &= expect(Tile::portal.trySpawnPortal(zLevel, 0, 64, 4), "cornerless Z frame lights from its interior");
	ok &= expect(portalInteriorIsPortal(zLevel, 0, 64, 4, false), "cornerless Z frame fills the 2x3 interior");

	World broken;
	putPortalFrame(broken, 0, 64, 8, true, false);
	broken.put(0, 67, 8, 0, 0);
	ok &= expect(!Tile::portal.trySpawnPortal(broken, 0, 64, 8), "a missing top edge refuses the portal");

	World full;
	putPortalFrame(full, 0, 64, 12, true, true);
	ok &= expect(Tile::portal.trySpawnPortal(full, 0, 64, 12), "the full fourteen-obsidian frame still lights");
	ok &= expect(portalInteriorIsPortal(full, 0, 64, 12, true), "the full frame fills the 2x3 interior");
	return ok;
}

// F039/B33, face half. Only the open sides at a portal column edge draw;
// every other face, including all faces of a far cell, stays culled.
bool portalFaceCulling()
{
	World level;
	level.put(0, 64, 0, Tile::portal.id, 0);

	bool ok = expect(Tile::portal.shouldRenderFace(level, -1, 64, 0, Facing::EAST),
		"the open side at a portal column edge draws");
	ok &= expect(!Tile::portal.shouldRenderFace(level, -1, 64, 0, Facing::UP),
		"the top face at a portal column edge stays culled");
	ok &= expect(Tile::portal.shouldRenderFace(level, 0, 64, 1, Facing::NORTH),
		"the other axis edge draws its open side");
	ok &= expect(!Tile::portal.shouldRenderFace(level, 0, 64, 0, Facing::EAST),
		"a portal cell never draws its own face");
	ok &= expect(!Tile::portal.shouldRenderFace(level, 9, 64, 9, Facing::EAST),
		"a far cell draws nothing for the portal");
	return ok;
}

// F039/B33, particle half. An X-axis portal concentrates its display
// particles on the Z plane; the swapped axis branch spreads them instead.
bool portalParticlePlane()
{
	World level;
	level.put(0, 64, 0, Tile::portal.id, 0);
	level.put(1, 64, 0, Tile::portal.id, 0);
	level.random.setSeed(7);

	Recorder events;
	level.addListener(events);
	Tile::portal.animateTick(level, 0, 64, 0, level.random);
	level.removeListener(events);

	bool ok = expect(events.portalAt.size() == 4, "a portal tick spawns four display particles");
	for (const auto &at : events.portalAt)
		ok &= expect(near(std::abs(at[2] - 0.5), 0.25), "X-axis portal particles sit on the Z plane");
	return ok;
}

// F067, owned part. Both liquid states carry their material description key.
bool liquidDescriptionKeys()
{
	bool ok = expect(Tile::water.descriptionId == u"tile.water", "flowing water is tile.water");
	ok &= expect(Tile::calmWater.descriptionId == u"tile.water", "still water is tile.water");
	ok &= expect(Tile::lava.descriptionId == u"tile.lava", "flowing lava is tile.lava");
	ok &= expect(Tile::calmLava.descriptionId == u"tile.lava", "still lava is tile.lava");
	return ok;
}

bool run()
{
	Tile::initTiles();
	bool ok = fallingFlowNeedsSolid();
	ok &= slopeLevelRegionParity();
	ok &= fizzSmokeAndHarden();
	ok &= fireSilentAging();
	ok &= fireRainExtinguishFirst();
	ok &= portalCornerlessFrames();
	ok &= portalFaceCulling();
	ok &= portalParticlePlane();
	ok &= liquidDescriptionKeys();
	std::cout << "audit-liquidsfireportal cases: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}
}

bool runAuditLiquidsFirePortalCases()
{
	return AuditLiquidsFirePortal::Detail::run();
}
