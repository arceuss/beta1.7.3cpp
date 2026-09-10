#include "ClientTarget.h"
// Plant placement, growth, drops, lighting and shape regressions.

#include "world/level/tile/DeadBushTile.h"
#include "world/level/tile/DirtTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/SandTile.h"
#include "world/level/tile/SnowBlockTile.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/StoneTile.h"
#include "world/level/tile/TreeTile.h"
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/TilePos.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/material/Material.h"
#include "world/level/tile/CactusTile.h"
#include "world/level/tile/CropsTile.h"
#include "world/level/tile/FarmlandTile.h"
#include "world/level/tile/FlowerTile.h"
#include "world/level/tile/GrassTile.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/tile/MushroomTile.h"
#include "world/level/tile/ReedTile.h"
#include "world/level/tile/SaplingTile.h"
#include "world/level/tile/TallGrassTile.h"
#include "world/level/tile/Tile.h"
#include "world/phys/AABB.h"

namespace AuditPlants
{
namespace Detail
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "audit-plants FAILED: " << message << '\n';
	return condition;
}

bool near(double a, double b)
{
	return (a - b) < 1.0e-7 && (b - a) < 1.0e-7;
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
	jstring gatherStats() override { return u"audit-plants"; }
};

class Recorder : public LevelListener
{
public:
	std::vector<TilePos> changed;
	std::vector<TilePos> dirty;

	void tileChanged(int_t x, int_t y, int_t z) override { changed.emplace_back(x, y, z); }
	void setTilesDirty(int_t x0, int_t y0, int_t z0, int_t, int_t, int_t) override { dirty.emplace_back(x0, y0, z0); }
	void allChanged() override {}
	void playSound(const jstring &, double, double, double, float, float) override {}
	void addParticle(const jstring &, double, double, double, double, double, double) override {}
	void playMusic(const jstring &, double, double, double, float) override {}
	void entityAdded(std::shared_ptr<Entity>) override {}
	void entityRemoved(std::shared_ptr<Entity>) override {}
	void skyColorChanged() override {}
	void playStreamingMusic(const jstring &, int_t, int_t, int_t) override {}
	void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
	void levelEvent(Player *, int_t, int_t, int_t, int_t, int_t) override {}

	bool sawChanged(int_t x, int_t y, int_t z) const { return contains(changed, x, y, z); }

private:
	static bool contains(const std::vector<TilePos> &list, int_t x, int_t y, int_t z)
	{
		for (const TilePos &pos : list)
		{
			if (pos.x == x && pos.y == y && pos.z == z)
				return true;
		}
		return false;
	}
};

class World : public Level
{
public:
	std::shared_ptr<Source> source = std::make_shared<Source>();

	World() : Level(u"audit-plants", Dimension::Id_Normal, 1234567, false)
	{
		setChunkSource(source);
		for (int_t cx = -2; cx <= 2; ++cx)
			for (int_t cz = -2; cz <= 2; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
	}

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

// F003/B04/B06. Each bush family admits only its own soils. Flower and tall
// grass share grass/dirt/farmland, crops need farmland, dead bush needs sand,
// mushrooms need an opaque (solid-render) block below.
bool bushAdmission()
{
	World level;
	bool ok = true;

	level.put(0, 63, 0, Tile::grass.id, 0);
	level.put(2, 63, 0, Tile::dirt.id, 0);
	level.put(4, 63, 0, Tile::farmland.id, 0);
	level.put(6, 63, 0, Tile::sand.id, 0);
	level.put(8, 63, 0, Tile::rock.id, 0);
	ok &= expect(Tile::flower.mayPlace(level, 0, 64, 0), "flower admits grass");
	ok &= expect(Tile::flower.mayPlace(level, 2, 64, 0), "flower admits dirt");
	ok &= expect(Tile::flower.mayPlace(level, 4, 64, 0), "flower admits farmland");
	ok &= expect(!Tile::flower.mayPlace(level, 6, 64, 0), "flower rejects sand");
	ok &= expect(!Tile::flower.mayPlace(level, 8, 64, 0), "flower rejects stone");
	ok &= expect(Tile::tallGrass.mayPlace(level, 0, 64, 0), "tall grass shares flower soils");
	ok &= expect(!Tile::tallGrass.mayPlace(level, 6, 64, 0), "tall grass rejects sand");
	ok &= expect(Tile::crops.mayPlace(level, 4, 64, 0), "crops admit farmland");
	ok &= expect(!Tile::crops.mayPlace(level, 0, 64, 0), "crops reject grass");
	ok &= expect(Tile::deadBush.mayPlace(level, 6, 64, 0), "dead bush admits sand");
	ok &= expect(!Tile::deadBush.mayPlace(level, 0, 64, 0), "dead bush rejects grass");
	ok &= expect(Tile::brownMushroom.mayPlace(level, 8, 64, 0), "mushroom admits solid stone");
	ok &= expect(Tile::brownMushroom.mayPlace(level, 6, 64, 0) == Tile::solid[Tile::sand.id],
		"mushroom on sand follows the opaque lookup exactly");

	// Rejected preflight must not mutate the world: the target cell keeps air.
	level.put(10, 64, 0, 0, 0);
	ok &= expect(!Tile::crops.mayPlace(level, 10, 64, 0) && level.getTile(10, 64, 0) == 0,
		"rejected placement leaves air alone instead of place-then-drop");
	return ok;
}

// F003/B04. Support loss drops through the normal path before the block is
// cleared, and the removal notifies. A flower always drops one item, so the
// entity count distinguishes drop-before-remove from a silent clear.
bool flowerSupportLossDrops()
{
	World level;
	level.put(0, 63, 0, Tile::grass.id, 0);
	level.put(0, 64, 0, Tile::flower.id, 0);
	size_t before = level.entities.size();

	Recorder events;
	level.addListener(events);
	level.put(0, 63, 0, 0, 0);
	Tile::flower.neighborChanged(level, 0, 64, 0, 0);
	level.removeListener(events);

	bool ok = expect(level.getTile(0, 64, 0) == 0, "unsupported flower is removed");
	ok &= expect(level.entities.size() == before + 1, "support loss drops the flower item first");
	ok &= expect(events.sawChanged(0, 64, 0), "the removal uses the notifying clear");
	return ok;
}

// F026/B11/B06. Cactus needs sand or cactus below with no solid side
// neighbour; reed needs itself or grass/dirt beside water. Cactus support loss
// drops, matching the reed path that already dropped.
bool cactusReedAdmissionAndDrops()
{
	World level;
	bool ok = true;

	level.floor(-4, -4, 4, 4, 62, Tile::sand.id);
	level.put(0, 63, 0, Tile::sand.id, 0);
	ok &= expect(Tile::cactus.mayPlace(level, 0, 64, 0), "cactus admits sand with clear sides");
	level.put(1, 64, 0, Tile::rock.id, 0);
	ok &= expect(!Tile::cactus.canStay(level, 0, 64, 0), "a solid side neighbour vetoes cactus");
	level.put(1, 64, 0, 0, 0);
	level.put(0, 63, 0, Tile::grass.id, 0);
	ok &= expect(!Tile::cactus.mayPlace(level, 0, 64, 0), "cactus rejects grass soil");

	level.put(0, 63, 0, Tile::sand.id, 0);
	level.put(0, 64, 0, Tile::cactus.id, 0);
	size_t before = level.entities.size();
	Recorder events;
	level.addListener(events);
	level.put(1, 64, 0, Tile::rock.id, 0);
	Tile::cactus.neighborChanged(level, 0, 64, 0, Tile::rock.id);
	level.removeListener(events);
	ok &= expect(level.getTile(0, 64, 0) == 0, "invalid cactus is removed");
	ok &= expect(level.entities.size() == before + 1, "cactus support loss drops its item");
	ok &= expect(events.sawChanged(0, 64, 0), "cactus removal notifies");

	World water;
	water.put(0, 62, 0, Tile::grass.id, 0);
	water.put(1, 62, 0, Tile::calmWater.id, 0);
	ok &= expect(Tile::reed.mayPlace(water, 0, 63, 0), "reed admits grass beside water");
	water.put(1, 62, 0, Tile::sand.id, 0);
	ok &= expect(!Tile::reed.mayPlace(water, 0, 63, 0), "reed without adjacent water is rejected");
	water.put(0, 62, 0, Tile::sand.id, 0);
	ok &= expect(!Tile::reed.mayPlace(water, 0, 63, 0), "reed rejects sand even beside water");
	water.put(0, 62, 0, Tile::reed.id, 0);
	ok &= expect(Tile::reed.mayPlace(water, 0, 63, 0), "reed stacks on itself");
	return ok;
}

// A04. The sapling keeps its own larger default bounds. Item rendering calls
// the virtual hook on the shared tile, so inheriting the flower box would
// shrink every sapling in the world.
bool saplingBounds()
{
	Tile::sapling.updateDefaultShape();
	bool ok = expect(near(Tile::sapling.xx0, 0.1) && near(Tile::sapling.xx1, 0.9)
			&& near(Tile::sapling.yy1, 0.8),
		"sapling default bounds stay at radius 0.4 and height 0.8");
	Tile::flower.updateDefaultShape();
	ok &= expect(near(Tile::flower.xx0, 0.3) && near(Tile::flower.yy1, 0.6),
		"flower default bounds stay at radius 0.2 and height 0.6");
	Tile::sapling.updateDefaultShape();
	ok &= expect(near(Tile::sapling.xx0, 0.1) && near(Tile::sapling.yy1, 0.8),
		"rebuilding sapling bounds after flower rendering still restores sapling size");
	return ok;
}

// F024/B05. A failed generator restores the sapling species silently instead
// of retrying another tree. A fully solid cube fails every feature, so the
// only observable difference is the write path: silent restoration sends no
// tileChanged, a notifying one does.
bool saplingGrowthSilentRestore()
{
	World level;
	level.floor(-6, -6, 6, 6, 63, Tile::rock.id);
	for (int_t y = 64; y < 72; ++y)
		for (int_t x = -6; x <= 6; ++x)
			for (int_t z = -6; z <= 6; ++z)
				if (x != 0 || y != 64 || z != 0)
					level.put(x, y, z, Tile::rock.id, 0);
	level.put(0, 63, 0, Tile::grass.id, 0);
	level.put(0, 64, 0, Tile::sapling.id, 1);

	Recorder events;
	level.addListener(events);
	Tile::sapling.growTree(level, 0, 64, 0, level.random);
	level.removeListener(events);

	bool ok = expect(level.getTile(0, 64, 0) == Tile::sapling.id, "failed growth keeps a sapling");
	ok &= expect((level.getData(0, 64, 0) & 3) == 1, "failed growth restores the spruce species bits");
	ok &= expect(events.changed.empty(), "clear and restore are silent no-update writes");
	ok &= expect(Tile::sapling.getSpawnResourcesAuxValue(7) == 3, "sapling drops mask the species bits");
	return ok;
}

// F020/B10. Both snow materials select the snowy side texture.
bool grassSnowySides()
{
	World level;
	level.put(0, 63, 0, Tile::grass.id, 0);
	level.put(0, 64, 0, Tile::snow.id, 0);
	bool ok = expect(Tile::grass.getTexture(level, 0, 63, 0, Facing::NORTH) == 68,
		"snow layer above selects the snowy side");
	level.put(0, 64, 0, Tile::snowBlock.id, 0);
	ok &= expect(Tile::grass.getTexture(level, 0, 63, 0, Facing::NORTH) == 68,
		"full snow block above also selects the snowy side");
	level.put(0, 64, 0, Tile::rock.id, 0);
	ok &= expect(Tile::grass.getTexture(level, 0, 63, 0, Facing::NORTH) == 3,
		"stone above keeps the normal side");
	ok &= expect(Tile::grass.getTexture(level, 0, 63, 0, Facing::UP) == 0
			&& Tile::grass.getTexture(level, 0, 63, 0, Facing::DOWN) == 2,
		"top and bottom faces are unaffected");
	return ok;
}

// F023/B10 plus F067. Fern metadata keeps the white tint without touching the
// biome sampler, and the two grass variants map to their exact textures.
bool tallGrassTintAndTextures()
{
	World level;
	level.put(0, 64, 0, Tile::tallGrass.id, 0);
	bool ok = expect(Tile::tallGrass.getColor(level, 0, 64, 0) == 0xFFFFFF,
		"fern tall grass stays white");
	ok &= expect(Tile::tallGrass.getTexture(Facing::UP, 0) == Tile::tallGrass.tex + 16,
		"shrub maps to base texture plus 16");
	ok &= expect(Tile::tallGrass.getTexture(Facing::UP, 1) == Tile::tallGrass.tex,
		"tall grass maps to the base texture");
	ok &= expect(Tile::tallGrass.getTexture(Facing::UP, 2) == Tile::tallGrass.tex + 17,
		"fern maps to base texture plus 17");
	ok &= expect(Tile::tallGrass.getItemColor(0) == 0xFFFFFF, "tall-grass item tint is white");
	return ok;
}

// F025/B13. Removing a leaf marks the 3x3x3 neighbourhood decay bit with the
// silent setter, and a supported decay tick clears it silently. Sheared drops
// go through the shared scatter helper with the species bits, which the aux
// mask below pins down.
bool leafDecayNotifications()
{
	World level;
	level.floor(-4, -4, 4, 4, 63, Tile::rock.id);
	level.put(0, 65, 0, Tile::treeTrunk.id, 0);
	for (int_t dx = -1; dx <= 1; ++dx)
		for (int_t dy = -1; dy <= 1; ++dy)
			for (int_t dz = -1; dz <= 1; ++dz)
				if (dx != 0 || dy != 0 || dz != 0)
					level.put(dx, 65 + dy, dz, Tile::leaves.id, 0);

	Recorder markEvents;
	level.addListener(markEvents);
	Tile::leaves.onRemove(level, 0, 65, 0);
	level.removeListener(markEvents);

	bool ok = expect((level.getData(1, 65, 0) & LeafTile::CHECK_DECAY_BIT) != 0,
		"neighbours of a removed leaf gain the decay bit");
	ok &= expect(markEvents.changed.empty(), "decay marking is a silent metadata write");
	ok &= expect((level.getData(1, 65, 0) & LeafTile::LEAF_TYPE_MASK) == 0,
		"marking preserves the species bits");

	level.put(2, 65, 0, Tile::leaves.id, LeafTile::CHECK_DECAY_BIT | 1);
	Recorder tickEvents;
	level.addListener(tickEvents);
	Tile::leaves.tick(level, 2, 65, 0, level.random);
	level.removeListener(tickEvents);
	ok &= expect((level.getData(2, 65, 0) & LeafTile::CHECK_DECAY_BIT) == 0,
		"supported leaves clear the decay bit");
	ok &= expect(tickEvents.changed.empty(), "the clear is silent, matching setBlockMetadata");
	ok &= expect(Tile::leaves.getSpawnResourcesAuxValue(7) == 3,
		"leaf drops mask the species bits with data and 3");
	ok &= expect(Tile::leaves.getTexture(Facing::UP, 1) == Tile::leaves.tex + (ClientTarget::isAlphaPlace() ? 0 : 80),
		"spruce leaf texture respects the selected client target");
	return ok;
}

// F019/B09. Water or rain hydrates to moisture 7, and the hydration write
// notifies neighbors even when the farmland is already wet. Dry farmland
// without water or crops decays and finally reverts to dirt.
bool farmlandHydration()
{
	World water;
	water.floor(-6, -6, 6, 6, 62, Tile::rock.id);
	water.put(3, 63, 0, Tile::calmWater.id, 0);
	water.put(0, 63, 0, Tile::farmland.id, 0);

	for (int_t i = 0; i < 200 && water.getData(0, 63, 0) != 7; ++i)
		Tile::farmland.tick(water, 0, 63, 0, water.random);
	bool ok = expect(water.getData(0, 63, 0) == 7, "water nearby hydrates dry farmland");

	water.put(1, 63, 0, Tile::flower.id, 0);
	for (int_t i = 0; i < 60; ++i)
		Tile::farmland.tick(water, 0, 63, 0, water.random);
	ok &= expect(water.getData(0, 63, 0) == 7, "hydrated farmland stays at moisture 7");
	ok &= expect(water.getTile(1, 63, 0) == 0, "repeated hydration notifies the unsupported neighboring flower");

	World dry;
	dry.floor(-6, -6, 6, 6, 62, Tile::rock.id);
	dry.put(0, 63, 0, Tile::farmland.id, 0);
	for (int_t i = 0; i < 400 && dry.getTile(0, 63, 0) == Tile::farmland.id; ++i)
		Tile::farmland.tick(dry, 0, 63, 0, dry.random);
	ok &= expect(dry.getTile(0, 63, 0) == Tile::dirt.id, "dry farmland without crops reverts to dirt");
	return ok;
}

// F022/B07. Survival reads full brightness, not sky-darkened light. Open sky
// keeps a flower even in the dark branch, while a capped flower with stored
// darkness fails; mushrooms need darkness under a cap and solid soil.
bool plantLightSurvival()
{
	World level;
	level.floor(-4, -4, 4, 4, 63, Tile::rock.id);
	level.put(0, 63, 0, Tile::grass.id, 0);
	bool ok = expect(Tile::flower.canStay(level, 0, 64, 0), "open-sky flower survives on grass");

	level.put(0, 65, 0, Tile::rock.id, 0);
	bool capped = Tile::flower.canStay(level, 0, 64, 0);
	ok &= expect(!capped || level.canSeeSky(0, 64, 0),
		"capped flower without sky needs full brightness of 8");
	level.put(0, 65, 0, 0, 0);

	level.put(4, 63, 0, Tile::rock.id, 0);
	level.put(4, 65, 0, Tile::rock.id, 0);
	ok &= expect(Tile::brownMushroom.canStay(level, 4, 64, 0), "capped mushroom survives on solid soil");
	ok &= expect(!Tile::brownMushroom.canStay(level, 4, 200, 0), "mushroom above height bounds fails");
	return ok;
}

// B05 bone-meal entry points used by ItemDye. Fertilize notifies to 7 while a
// sapling grow attempt in solid rock restores silently; ItemsR owns the stack
// accounting and the grass scatter.
bool boneMealHooks()
{
	World level;
	level.floor(-2, -2, 2, 2, 63, Tile::rock.id);
	level.put(0, 63, 0, Tile::farmland.id, 7);
	level.put(0, 64, 0, Tile::crops.id, 3);

	Tile::crops.fertilize(level, 0, 64, 0);
	bool ok = expect(level.getData(0, 64, 0) == 7, "bone meal sets crops to full growth");
	return ok;
}

bool run()
{
	Tile::initTiles();
	bool ok = bushAdmission();
	ok &= flowerSupportLossDrops();
	ok &= cactusReedAdmissionAndDrops();
	ok &= saplingBounds();
	ok &= saplingGrowthSilentRestore();
	ok &= grassSnowySides();
	ok &= tallGrassTintAndTextures();
	ok &= leafDecayNotifications();
	ok &= farmlandHydration();
	ok &= plantLightSurvival();
	ok &= boneMealHooks();
	std::cout << "audit-plants cases: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}
}

bool runAuditPlantsCases()
{
	return AuditPlants::Detail::run();
}
