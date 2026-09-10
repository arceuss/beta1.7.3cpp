// Glass predicates cross-checked against the original Java renderer.

#include "world/level/tile/GlassTile.h"
#include "world/level/tile/IceTile.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/StoneTile.h"
#include <iostream>
#include <map>
#include <memory>
#include <utility>

#include "Facing.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/Tile.h"

namespace AuditJavaGlassProbe
{
namespace Detail
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "audit-javaglassprobe FAILED: " << message << '\n';
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
	jstring gatherStats() override { return u"audit-javaglassprobe"; }
};

class World : public Level
{
public:
	std::shared_ptr<Source> source = std::make_shared<Source>();

	World() : Level(u"audit-javaglassprobe", Dimension::Id_Normal, 1234567, false)
	{
		setChunkSource(source);
		for (int_t cx = -1; cx <= 1; ++cx)
			for (int_t cz = -1; cz <= 1; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
	}

	// Raw placement: no onPlace, no notification, so a case starts from an exact
	// world state and only the predicate under test reads it.
	void put(int_t x, int_t y, int_t z, int_t tile, int_t data)
	{
		source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(tile);
		setDataNoUpdate(x, y, z, data);
	}
};

// Beta face order shared by the Java probe and Facing.h: DOWN, UP, NORTH,
// SOUTH, WEST, EAST. Offsets point at the neighbour cell for that face.
int_t faceOffsetX(int face) { return face == 4 ? -1 : (face == 5 ? 1 : 0); }
int_t faceOffsetY(int face) { return face == 0 ? -1 : (face == 1 ? 1 : 0); }
int_t faceOffsetZ(int face) { return face == 2 ? -1 : (face == 3 ? 1 : 0); }

// Asks the glass predicate exactly the way the Java probe does: neighbour
// coordinates plus the face index of the glass block toward that neighbour.
bool glassFace(World &level, int_t gx, int_t gy, int_t gz, int face)
{
	return Tile::glass.shouldRenderFace(level,
		gx + faceOffsetX(face), gy + faceOffsetY(face), gz + faceOffsetZ(face),
		static_cast<Facing>(face));
}

// Java oracle predicate.tsv, glass-vs-air row: every face renders.
bool isolatedGlassShowsAllFaces()
{
	World level;
	level.put(8, 96, 8, Tile::glass.id, 0);
	bool ok = true;
	for (int face = 0; face < 6; ++face)
		ok &= expect(glassFace(level, 8, 96, 8, face),
			"isolated glass renders its face toward air");
	return ok;
}

// Geometry oracle: a face-adjacent pair emits 5 + 5 = 10 quads, so exactly the
// shared interface is suppressed on each side and nothing else.
bool glassPairSuppressesOnlyTheSharedInterface()
{
	World level;
	level.put(8, 96, 8, Tile::glass.id, 0);
	level.put(9, 96, 8, Tile::glass.id, 0);
	bool ok = expect(!glassFace(level, 8, 96, 8, 5),
		"glass hides its east face against neighbouring glass");
	ok &= expect(!glassFace(level, 9, 96, 8, 4),
		"glass hides its west face against neighbouring glass");
	for (int face = 0; face < 6; ++face)
	{
		if (face != 5)
			ok &= expect(glassFace(level, 8, 96, 8, face),
				"glass keeps its outer faces next to a glass neighbour");
		if (face != 4)
			ok &= expect(glassFace(level, 9, 96, 8, face),
				"glass keeps its outer faces next to a glass neighbour");
	}
	return ok;
}

// Java oracle predicate.tsv, glass-vs-stone and glass-vs-leaves rows: an
// opaque full cube on any face suppresses that face.
bool glassHidesFacesAgainstOpaqueCubes()
{
	bool ok = true;
	for (int face = 0; face < 6; ++face)
	{
		{
			World stone;
			stone.put(8, 96, 8, Tile::glass.id, 0);
			stone.put(8 + faceOffsetX(face), 96 + faceOffsetY(face), 8 + faceOffsetZ(face), Tile::rock.id, 0);
			ok &= expect(!glassFace(stone, 8, 96, 8, face),
				"glass hides its face against stone");
		}
		{
			World leaves;
			leaves.put(8, 96, 8, Tile::glass.id, 0);
			leaves.put(8 + faceOffsetX(face), 96 + faceOffsetY(face), 8 + faceOffsetZ(face), Tile::leaves.id, 0);
			ok &= expect(!glassFace(leaves, 8, 96, 8, face),
				"glass hides its face against leaves");
		}
	}
	return ok;
}

// Java oracle predicate.tsv, glass-vs-ice and glass-vs-waterStill rows: the
// same-ID rule must not leak across different transparent blocks.
bool glassKeepsFacesAgainstIceAndWater()
{
	bool ok = true;
	for (int face = 0; face < 6; ++face)
	{
		{
			World ice;
			ice.put(8, 96, 8, Tile::glass.id, 0);
			ice.put(8 + faceOffsetX(face), 96 + faceOffsetY(face), 8 + faceOffsetZ(face), Tile::ice.id, 0);
			ok &= expect(glassFace(ice, 8, 96, 8, face),
				"glass keeps its face against ice");
		}
		{
			World water;
			water.put(8, 96, 8, Tile::glass.id, 0);
			water.put(8 + faceOffsetX(face), 96 + faceOffsetY(face), 8 + faceOffsetZ(face), Tile::calmWater.id, 0);
			ok &= expect(glassFace(water, 8, 96, 8, face),
				"glass keeps its face against still water");
		}
	}
	return ok;
}

// Java oracle from Bind::verify in the probe: glass and stone ride render
// pass 0, ice and still water ride pass 1. Glass in the blended pass would
// lose back-face culling; ice in pass 0 would gain it.
bool renderPasses()
{
	bool ok = expect(Tile::glass.getRenderLayer() == 0, "glass renders in pass 0");
	ok &= expect(Tile::rock.getRenderLayer() == 0, "stone renders in pass 0");
	ok &= expect(Tile::ice.getRenderLayer() == 1, "ice renders in pass 1");
	ok &= expect(Tile::calmWater.getRenderLayer() == 1, "still water renders in pass 1");
	return ok;
}

// Glass must stay a non-solid render cube: solidity feeds both the base face
// predicate and the light table (Tile::initTiles writes lightBlock from it).
bool solidity()
{
	bool ok = expect(!Tile::glass.isSolidRender(), "glass is not a solid render cube");
	ok &= expect(!Tile::ice.isSolidRender(), "ice is not a solid render cube");
	ok &= expect(Tile::rock.isSolidRender(), "stone is a solid render cube");
	return ok;
}

}

// Not wired into CMake. Main can add this file to the smoke target that
// already links McBetaCppCore and call runAuditJavaGlassProbeCases().
bool run()
{
	Tile::initTiles();
	bool ok = Detail::isolatedGlassShowsAllFaces();
	ok &= Detail::glassPairSuppressesOnlyTheSharedInterface();
	ok &= Detail::glassHidesFacesAgainstOpaqueCubes();
	ok &= Detail::glassKeepsFacesAgainstIceAndWater();
	ok &= Detail::renderPasses();
	ok &= Detail::solidity();
	std::cout << "audit-javaglassprobe cases: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}

bool runAuditJavaGlassProbeCases()
{
	return AuditJavaGlassProbe::run();
}
