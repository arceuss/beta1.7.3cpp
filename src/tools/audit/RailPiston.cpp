// Mixed rail metadata, notifications and piston placeholder ordering.

#include "world/level/tile/StoneTile.h"
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/TilePos.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/DetectorRailTile.h"
#include "world/level/tile/PistonBaseTile.h"
#include "world/level/tile/PistonExtensionTile.h"
#include "world/level/tile/PistonMovingTile.h"
#include "world/level/tile/RailTile.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/PistonTileEntity.h"
#include "world/phys/AABB.h"

namespace AuditRailPiston
{
namespace Detail
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "audit-railpiston FAILED: " << message << '\n';
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
	jstring gatherStats() override { return u"audit-railpiston"; }
};

// Records the two listener callbacks a metadata write can reach: notifyBlockChange
// sends tileChanged, a plain no-update write plus render mark sends setTilesDirty.
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
	bool sawDirty(int_t x, int_t y, int_t z) const { return contains(dirty, x, y, z); }

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

	World() : Level(u"audit-railpiston", Dimension::Id_Normal, 1234567, false)
	{
		setChunkSource(source);
		for (int_t cx = -1; cx <= 1; ++cx)
			for (int_t cz = -1; cz <= 1; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
	}

	// Raw placement: no onPlace, no notification, so a case starts from an exact
	// world state and only the method under test produces callbacks.
	void put(int_t x, int_t y, int_t z, int_t tile, int_t data)
	{
		source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(tile);
		setDataNoUpdate(x, y, z, data);
	}

	void floor(int_t x0, int_t z0, int_t x1, int_t z1, int_t y)
	{
		for (int_t x = x0; x <= x1; ++x)
			for (int_t z = z0; z <= z1; ++z)
				put(x, y, z, Tile::rock.id, 0);
	}
};

// F048/B34 plus the notified write of F049/B35.
//
// Three powered rails form a straight, already saturated east-west line. A plain
// rail is placed to the north of the middle one. Reading the rail type per cell,
// the middle powered rail decodes metadata 9 as "flat east-west, powered", it
// already holds two connections and therefore refuses the newcomer, so the plain
// rail settles as an unconnected north-south rail. When the helper inherits the
// placed rail's plain type instead, it decodes 9 as curve 9, loses the line,
// accepts the connection and rewrites the powered rail as plain metadata 0.
bool mixedRailTypes()
{
	World level;
	level.floor(3, 6, 7, 9, 63);
	level.put(4, 64, 8, Tile::railPowered.id, 9);
	level.put(5, 64, 8, Tile::railPowered.id, 9);
	level.put(6, 64, 8, Tile::railPowered.id, 9);
	level.put(5, 64, 7, Tile::rail.id, 1);

	Recorder events;
	level.addListener(events);
	Tile::rail.onPlace(level, 5, 64, 7);
	level.removeListener(events);

	bool ok = expect(level.getData(5, 64, 7) == 0,
		"a plain rail refused by a saturated powered line stays north-south flat");
	// The notified write reaches the powered rail to its south, which keeps its
	// east-west shape bits and drops the stale power bit; that write notifies the
	// rest of the line, so the whole chain settles unpowered.
	ok &= expect(level.getData(5, 64, 8) == 1,
		"the powered rail keeps its east-west shape and loses only the stale power bit");
	ok &= expect(level.getData(4, 64, 8) == 1 && level.getData(6, 64, 8) == 1,
		"the notified powered-rail write propagates along the line");
	ok &= expect(events.sawChanged(5, 64, 7),
		"the rail shape write goes through the notifying metadata path");
	return ok;
}

// A01: Beta's selection bounds test raw metadata, so a powered rail on a slope
// (raw 10..13) keeps the flat box while the same slope unpowered gets the tall one.
bool railSelectionBox()
{
	World level;
	level.put(2, 63, 2, Tile::rock.id, 0);
	level.put(2, 64, 2, Tile::railPowered.id, 2);

	AABB *box = Tile::railPowered.getTileAABB(level, 2, 64, 2);
	bool ok = expect(box != nullptr && near(box->y1, 64.0 + 10.0 / 16.0),
		"an unpowered ascending rail uses the tall selection box");

	level.setDataNoUpdate(2, 64, 2, 10);
	box = Tile::railPowered.getTileAABB(level, 2, 64, 2);
	ok &= expect(box != nullptr && near(box->y1, 64.0 + 2.0 / 16.0),
		"Beta keeps the flat selection box for raw ascending metadata 10..13");
	return ok;
}

// F054/B36. The extension writes its placeholders without notification and
// attaches each moving tile entity immediately; the only notified write is the
// piston base metadata at the end. The observer is a powered rail carrying a
// stale power bit next to the head cell but not next to the base: any callback
// fired for the placeholder would recompute and clear that bit.
bool pistonPlaceholders()
{
	World level;
	level.floor(4, 4, 8, 7, 63);
	level.put(5, 64, 5, Tile::pistonBase.id, 5);
	level.put(6, 64, 5, Tile::rock.id, 0);
	level.put(6, 64, 6, Tile::railPowered.id, 9);

	Recorder events;
	level.addListener(events);
	Tile::pistonBase.playBlock(level, 5, 64, 5, 0, 5);
	level.removeListener(events);

	bool ok = expect(level.getTile(6, 64, 5) == Tile::pistonMoving.id && level.getData(6, 64, 5) == 5,
		"the head cell holds the moving block with the piston direction");
	auto head = std::dynamic_pointer_cast<PistonTileEntity>(level.getTileEntity(6, 64, 5));
	ok &= expect(head != nullptr && head->getStoredBlockID() == Tile::pistonExtension.id
			&& head->getDirection() == 5 && head->isExtending(),
		"the head placeholder carries an extending piston head tile entity");

	ok &= expect(level.getTile(7, 64, 5) == Tile::pistonMoving.id && level.getData(7, 64, 5) == 0,
		"the pushed block's destination holds the moving block with the carried metadata");
	auto carried = std::dynamic_pointer_cast<PistonTileEntity>(level.getTileEntity(7, 64, 5));
	ok &= expect(carried != nullptr && carried->getStoredBlockID() == Tile::rock.id
			&& carried->getBlockMetadata() == 0 && carried->isExtending(),
		"the pushed stone is carried by its own moving tile entity");

	ok &= expect(level.getData(5, 64, 5) == (5 | 8),
		"the extension ends with the notified powered base metadata");
	ok &= expect(level.getData(6, 64, 6) == 9,
		"no neighbour callback observed a placeholder before its tile entity existed");
	ok &= expect(events.sawChanged(5, 64, 5),
		"the base metadata write is the notifying one");
	return ok;
}

// F049/B35 for the detector rail: releasing with no minecart uses the notifying
// metadata write, keeps the shape bits and still marks the cell for redraw.
bool detectorRelease()
{
	World level;
	level.put(9, 63, 9, Tile::rock.id, 0);
	level.put(9, 64, 9, Tile::railDetector.id, 9);

	Recorder events;
	level.addListener(events);
	Tile::railDetector.tick(level, 9, 64, 9, level.random);
	level.removeListener(events);

	bool ok = expect(level.getData(9, 64, 9) == 1,
		"a detector rail with no minecart releases and keeps its shape bits");
	ok &= expect(events.sawDirty(9, 64, 9),
		"the release still marks the rail cell for redraw");
	return ok;
}

}

bool run()
{
	Tile::initTiles();
	bool ok = Detail::mixedRailTypes();
	ok &= Detail::railSelectionBox();
	ok &= Detail::pistonPlaceholders();
	ok &= Detail::detectorRelease();
	std::cout << "audit-railpiston cases: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}

bool runAuditRailPistonCases()
{
	return AuditRailPiston::run();
}
