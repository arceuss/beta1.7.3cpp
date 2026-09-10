// Placement, lighting and ray-collision regressions.

#include "world/level/tile/GlassTile.h"
#include "world/level/tile/GlowStoneTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/SlabTile.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/StoneTile.h"
#include "world/level/tile/TNTTile.h"
#include "world/level/tile/TallGrassTile.h"
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "world/level/Level.h"
#include "world/level/Region.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/dimension/Dimension.h"
#include "world/level/tile/Tile.h"
#include "world/phys/HitResult.h"
#include "world/phys/Vec3.h"
#include "util/ProgressListener.h"

namespace AuditFoundation
{
namespace Detail
{

bool expect(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "audit-foundation FAILED: " << message << '\n';
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
    jstring gatherStats() override { return u"audit-foundation"; }
};

class World : public Level
{
public:
    std::shared_ptr<Source> source = std::make_shared<Source>();

    World() : Level(u"audit-foundation", Dimension::Id_Normal, 1234567, false)
    {
        setChunkSource(source);
        for (int_t cx = -1; cx <= 1; ++cx)
            for (int_t cz = -1; cz <= 1; ++cz)
                source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
    }

    void put(int_t x, int_t y, int_t z, int_t tile, int_t data)
    {
        source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(tile);
        setDataNoUpdate(x, y, z, data);
    }
};

bool brightnessMinimumLevel()
{
    World level;
    level.skyDarken = 15;
    float glow = Tile::glowstone.getBrightness(level, 4, 64, 4);
    float stone = Tile::rock.getBrightness(level, 4, 64, 4);
    bool ok = expect(glow > stone, "glowstone renders above ambient stone in darkness");
    ok &= expect(glow == level.dimension->brightnessRamp[15], "glowstone clamps to its emission minimum");
    float floor = level.getMinBrightness(4, 64, 4, 0);
    ok &= expect(floor == stone, "zero minimum keeps ambient sampling");
    return ok;
}

bool brightnessMinimumRegion()
{
    World level;
    level.skyDarken = 15;
    Region region(level, 0, 0, 0, 15, 127, 15);
    float glow = Tile::glowstone.getBrightness(region, 4, 64, 4);
    float stone = Tile::rock.getBrightness(region, 4, 64, 4);
    bool ok = expect(glow > stone, "region glowstone renders above ambient stone in darkness");
    ok &= expect(glow == level.dimension->brightnessRamp[15], "region glowstone clamps to its emission minimum");
    return ok;
}

bool clipStartsInsideBlock()
{
    World level;
    level.put(0, 64, 0, Tile::rock.id, 0);
    Vec3 from(0.2, 64.5, 0.5);
    Vec3 to(1.2, 64.5, 0.5);
    HitResult hit = level.clip(from, to, false, false);
    bool ok = expect(hit.type != HitResult::Type::NONE, "ray exiting a solid block hits its starting cell");
    ok &= expect(hit.x == 0 && hit.y == 64 && hit.z == 0, "exit hit reports the starting cell");

    World empty;
    Vec3 from2(0.2, 64.5, 0.5);
    Vec3 to2(0.8, 64.5, 0.5);
    HitResult miss = empty.clip(from2, to2, false, false);
    ok &= expect(miss.type == HitResult::Type::NONE, "same-cell ray in air stays a miss");
    return ok;
}

bool clipIgnoreNoAABB()
{
    World level;
    level.put(2, 64, 2, Tile::snow.id, 0);
    Vec3 from(2.2, 64.05, 2.2);
    Vec3 to(2.2, 64.2, 2.2);
    HitResult hit = level.clip(from, to, false, false);
    bool ok = expect(hit.type != HitResult::Type::NONE, "thin snow is hittable without the skip flag");
    Vec3 from2(2.2, 64.05, 2.2);
    Vec3 to2(2.2, 64.2, 2.2);
    HitResult skipped = level.clip(from2, to2, false, true);
    ok &= expect(skipped.type == HitResult::Type::NONE, "ignoreNoAABB skips the boxless snow cell");
    return ok;
}

bool normalCubeIsNotOpacity()
{
    World level;
    level.put(5, 64, 5, Tile::rock.id, 0);
    level.put(6, 64, 5, Tile::tnt.id, 0);
    level.put(7, 64, 5, Tile::glass.id, 0);
    bool ok = expect(level.isBlockNormalCube(5, 64, 5), "stone is a normal cube");
    ok &= expect(level.isSolidTile(5, 64, 5), "stone is solid render");
    ok &= expect(level.isSolidTile(6, 64, 5), "tnt is solid render");
    ok &= expect(!level.isBlockNormalCube(6, 64, 5), "tnt is not a normal cube");
    ok &= expect(!level.isBlockNormalCube(7, 64, 5), "glass is not a normal cube");
    Region region(level, 0, 0, 0, 15, 127, 15);
    ok &= expect(region.isBlockNormalCube(5, 64, 5), "region agrees on stone");
    ok &= expect(region.isBlockNormalCube(6, 64, 5), "region uses live motion blocking for tnt");
    return ok;
}

bool mayPlaceBaseAdmission()
{
    World level;
    level.put(5, 64, 5, Tile::rock.id, 0);
    level.put(6, 64, 6, Tile::snow.id, 0);
    level.put(7, 64, 7, Tile::water.id, 0);
    bool ok = expect(level.mayPlace(Tile::rock.id, 4, 64, 4, false, Facing::UP), "rock fits in air");
    ok &= expect(!level.mayPlace(Tile::rock.id, 5, 64, 5, false, Facing::UP), "rock refuses solid stone");
    ok &= expect(level.mayPlace(Tile::rock.id, 6, 64, 6, false, Facing::UP), "rock replaces ground-cover snow");
    ok &= expect(level.mayPlace(Tile::rock.id, 7, 64, 7, false, Facing::UP), "rock replaces water");
    ok &= expect(!level.mayPlace(0, 4, 64, 4, false, Facing::UP), "empty id never places");
    ok &= expect(!level.mayPlace(Tile::rock.id, 5, 64, 5, false, Facing::UP), "face dispatch keeps the solid refusal");
    ok &= expect(Tile::rock.mayPlaceOnFace(level, 4, 64, 4, Facing::UP) == Tile::rock.mayPlace(level, 4, 64, 4),
        "base face dispatch delegates to the ground-cover rule");
    return ok;
}

bool descriptionKeys()
{
    bool ok = expect(Tile::tallGrass.descriptionId == u"tile.tallgrass", "tall grass key");
    ok &= expect(Tile::slabDouble.descriptionId == u"tile.stoneSlab", "double slab root key");
    ok &= expect(Tile::slabSingle.descriptionId == u"tile.stoneSlab", "half slab root key");
    ok &= expect(Tile::water.descriptionId == u"tile.water", "water key");
    ok &= expect(Tile::calmWater.descriptionId == u"tile.water", "still water key");
    ok &= expect(Tile::lava.descriptionId == u"tile.lava", "lava key");
    ok &= expect(Tile::calmLava.descriptionId == u"tile.lava", "still lava key");
    ok &= expect(Tile::rock.descriptionId == u"tile.stone", "stone control key");
    return ok;
}

bool run()
{
    Tile::initTiles();
    bool ok = brightnessMinimumLevel();
    ok &= brightnessMinimumRegion();
    ok &= clipStartsInsideBlock();
    ok &= clipIgnoreNoAABB();
    ok &= normalCubeIsNotOpacity();
    ok &= mayPlaceBaseAdmission();
    ok &= descriptionKeys();
    std::cout << "audit-foundation cases: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

}
}

bool runAuditFoundationCases()
{
    return AuditFoundation::Detail::run();
}
