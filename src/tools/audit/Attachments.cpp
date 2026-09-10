// Attachment support, placement, interaction and authority regressions.

#include "world/level/tile/ButtonTile.h"
#include "world/level/tile/CakeTile.h"
#include "world/level/tile/DoorTile.h"
#include "world/level/tile/FenceTile.h"
#include "world/level/tile/GlassTile.h"
#include "world/level/tile/LadderTile.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/tile/LeverTile.h"
#include "world/level/tile/PressurePlateTile.h"
#include "world/level/tile/PumpkinTile.h"
#include "world/level/tile/SlabTile.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/StoneTile.h"
#include "world/level/tile/TNTTile.h"
#include "world/level/tile/TorchTile.h"
#include "world/level/tile/TrapDoorTile.h"
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "world/entity/Entity.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/monster/PigZombie.h"
#include "world/entity/player/Player.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/TileEntity.h"

namespace AuditAttachments
{

bool expect(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "audit-attachments FAILED: " << message << '\n';
    return condition;
}

bool near(double a, double b)
{
    return std::abs(a - b) < 1.0e-7;
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
    jstring gatherStats() override { return u"audit-attachments"; }
};

class World : public Level
{
public:
    std::shared_ptr<Source> source = std::make_shared<Source>();

    World() : Level(u"audit-attachments", Dimension::Id_Normal, 987654321LL, false)
    {
        setChunkSource(source);
        for (int_t cx = -2; cx <= 3; ++cx)
            for (int_t cz = -2; cz <= 2; ++cz)
                source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
    }

    void raw(int_t x, int_t y, int_t z, int_t id, int_t data = 0)
    {
        source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(id);
        setDataNoUpdate(x, y, z, data);
    }

    void clearCell(int_t x, int_t y, int_t z)
    {
        raw(x, y, z, 0, 0);
    }
};

struct Event
{
    int_t id = 0;
    int_t x = 0;
    int_t y = 0;
    int_t z = 0;
    int_t data = 0;
};

class Recorder : public LevelListener
{
public:
    std::vector<Event> events;

    void tileChanged(int_t, int_t, int_t) override {}
    void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
    void allChanged() override {}
    void playSound(const jstring &, double, double, double, float, float) override {}
    void addParticle(const jstring &, double, double, double, double, double, double) override {}
    void playMusic(const jstring &, double, double, double, float) override {}
    void entityAdded(std::shared_ptr<Entity>) override {}
    void entityRemoved(std::shared_ptr<Entity>) override {}
    void skyColorChanged() override {}
    void playStreamingMusic(const jstring &, int_t, int_t, int_t) override {}
    void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
    void levelEvent(Player *, int_t event, int_t x, int_t y, int_t z, int_t data) override
    {
        events.push_back({event, x, y, z, data});
    }
};

static int_t stoneId() { return Tile::rock.id; }
static int_t glassId() { return Tile::glass.id; }
static int_t tntId() { return Tile::tnt.id; }
static int_t fenceId() { return Tile::fence.id; }
static int_t leavesId() { return Tile::leaves.id; }

bool pumpkinAndFence()
{
    bool ok = true;
    {
        World world;
        Player player(world);
        const int_t y = 64;
        world.raw(0, y - 1, 0, stoneId(), 0);
        world.clearCell(0, y, 0);
        ok &= expect(Tile::pumpkin.mayPlace(world, 0, y, 0), "pumpkin admits air above a normal cube");
        ok &= expect(Tile::jackOLantern.mayPlace(world, 0, y, 0), "jack-o-lantern admits air above a normal cube");
        world.raw(0, y - 1, 0, glassId(), 0);
        ok &= expect(!Tile::pumpkin.mayPlace(world, 0, y, 0), "pumpkin refuses glass below");
        world.raw(0, y - 1, 0, stoneId(), 0);
        world.raw(0, y, 0, stoneId(), 0);
        ok &= expect(!Tile::pumpkin.mayPlace(world, 0, y, 0), "pumpkin refuses a non-ground-cover target");
        world.clearCell(0, y, 0);
        world.raw(0, y, 0, Tile::snow.id, 0);
        ok &= expect(Tile::pumpkin.mayPlace(world, 0, y, 0), "pumpkin admits a ground-cover target");
        world.clearCell(0, y, 0);
        const struct { float yaw; int_t data; } sectors[] = {
            {0.0f, 2}, {90.0f, 3}, {180.0f, 0}, {270.0f, 1},
        };
        for (const auto &s : sectors)
        {
            for (int variant = 0; variant < 2; ++variant)
            {
                Tile &tile = (variant == 0) ? static_cast<Tile &>(Tile::pumpkin) : static_cast<Tile &>(Tile::jackOLantern);
                world.raw(0, y, 0, tile.id, 0);
                player.yRot = s.yaw;
                tile.setPlacedBy(world, 0, y, 0, player);
                bool match = (world.getData(0, y, 0) == s.data);
                if (!match)
                    std::cerr << "audit-attachments FAILED: pumpkin yaw sector sets metadata"
                              << " yaw=" << s.yaw << " variant=" << variant << '\n';
                ok &= match;
            }
        }
        world.raw(0, y, 0, Tile::pumpkin.id, 0);
        player.yRot = 44.0f;
        Tile::pumpkin.setPlacedBy(world, 0, y, 0, player);
        ok &= expect(world.getData(0, y, 0) == 2, "pumpkin yaw near a sector boundary stays in sector");
    }
    {
        World world;
        const int_t y = 64;
        world.raw(5, y - 1, 0, fenceId(), 0);
        world.raw(5, y - 2, 0, 0, 0);
        ok &= expect(Tile::fence.mayPlace(world, 5, y, 0), "fence stacks on fence");
        world.raw(6, y - 1, 0, stoneId(), 0);
        ok &= expect(Tile::fence.mayPlace(world, 6, y, 0), "fence admits solid material below");
        world.raw(7, y - 1, 0, glassId(), 0);
        ok &= expect(Tile::fence.mayPlace(world, 7, y, 0), "fence admits glass below via the solid-material rule");
        world.raw(8, y - 1, 0, 0, 0);
        ok &= expect(!Tile::fence.mayPlace(world, 8, y, 0), "fence refuses air below");
    }
    std::cout << "audit-attachments pumpkin/fence: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool snowSupport()
{
    bool ok = true;
    World world;
    const int_t y = 64;
    world.raw(0, y - 1, 0, stoneId(), 0);
    ok &= expect(Tile::snow.mayPlace(world, 0, y, 0), "snow admits stone below");
    world.raw(1, y - 1, 0, glassId(), 0);
    ok &= expect(!Tile::snow.mayPlace(world, 1, y, 0), "snow refuses glass below");
    world.raw(2, y - 1, 0, leavesId(), 0);
    Tile::leaves.setFancy(false);
    ok &= expect(Tile::leaves.isSolidRender(), "fast leaves render solid");
    ok &= expect(Tile::snow.mayPlace(world, 2, y, 0), "snow admits fast leaves via the live predicate");
    Tile::leaves.setFancy(true);
    ok &= expect(!Tile::leaves.isSolidRender(), "fancy leaves render non-solid");
    ok &= expect(!Tile::snow.mayPlace(world, 2, y, 0), "snow refuses fancy leaves via the live predicate");
    Tile::leaves.setFancy(false);
    world.raw(3, y - 1, 0, tntId(), 0);
    ok &= expect(Tile::snow.mayPlace(world, 3, y, 0) == Tile::tiles[tntId()]->isSolidRender(),
        "snow follows the supporting tile live solidity on TNT");
    world.raw(0, y, 0, Tile::snow.id, 0);
    world.raw(0, y - 1, 0, stoneId(), 0);
    Tile::snow.neighborChanged(world, 0, y, 0, stoneId());
    ok &= expect(world.getTile(0, y, 0) == Tile::snow.id, "snow survives supported neighbor updates");
    world.raw(0, y - 1, 0, 0, 0);
    Tile::snow.neighborChanged(world, 0, y, 0, 0);
    ok &= expect(world.getTile(0, y, 0) == 0, "snow drops once support is gone");
    {
        size_t before = world.entities.size();
        world.raw(4, y, 0, Tile::snow.id, 0);
        Player player(world);
        Tile::snow.harvestBlock(world, player, 4, y, 0, 0);
        ok &= expect(world.getTile(4, y, 0) == 0, "snow harvest clears the block");
        ok &= expect(world.entities.size() == before + 1, "snow harvest drops without rechecking the held tool");
    }
    std::cout << "audit-attachments snow: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool attachmentSupport()
{
    bool ok = true;
    World world;
    const int_t y = 64;
    world.raw(20, y, 1, stoneId(), 0);
    world.raw(20, y, -1, 0, 0);
    world.raw(21, y, 0, 0, 0);
    world.raw(19, y, 0, 0, 0);
    ok &= expect(Tile::ladder.mayPlace(world, 20, y, 0), "ladder admits a stone wall");
    world.raw(20, y, 1, tntId(), 0);
    ok &= expect(!Tile::ladder.mayPlace(world, 20, y, 0), "ladder refuses a TNT-only wall");
    world.raw(30, y - 1, 0, stoneId(), 0);
    ok &= expect(Tile::doorWood.mayPlace(world, 30, y, 0), "door admits a normal cube below with free halves");
    world.raw(31, y - 1, 0, tntId(), 0);
    ok &= expect(!Tile::doorWood.mayPlace(world, 31, y, 0), "door refuses TNT below");
    ok &= expect(!Tile::doorWood.mayPlace(world, 30, 127, 0), "door refuses the build ceiling");
    world.raw(40, y, 1, stoneId(), 0);
    ok &= expect(Tile::trapdoor.mayPlaceOnFace(world, 40, y, 0, Facing::NORTH), "trapdoor admits a supported wall face");
    world.raw(40, y, 1, tntId(), 0);
    ok &= expect(!Tile::trapdoor.mayPlaceOnFace(world, 40, y, 0, Facing::NORTH), "trapdoor refuses TNT on the clicked face");
    ok &= expect(!Tile::trapdoor.mayPlaceOnFace(world, 40, y, 0, Facing::UP), "trapdoor refuses floor clicks");
    ok &= expect(!Tile::trapdoor.mayPlaceOnFace(world, 40, y, 0, Facing::DOWN), "trapdoor refuses ceiling clicks");
    world.raw(50, y, 1, stoneId(), 0);
    ok &= expect(Tile::lever.mayPlaceOnFace(world, 50, y, 0, Facing::NORTH), "lever admits the clicked supported wall");
    ok &= expect(!Tile::lever.mayPlaceOnFace(world, 50, y, 0, Facing::SOUTH), "lever refuses an unsupported clicked wall");
    world.raw(50, y - 1, 0, stoneId(), 0);
    ok &= expect(Tile::lever.mayPlaceOnFace(world, 50, y, 0, Facing::UP), "lever admits a supported floor");
    ok &= expect(Tile::buttonStone.mayPlaceOnFace(world, 50, y, 0, Facing::NORTH), "button admits the clicked supported wall");
    ok &= expect(!Tile::buttonStone.mayPlaceOnFace(world, 50, y, 0, Facing::UP), "button refuses floor clicks");
    std::cout << "audit-attachments support: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool doorAndTrapdoorClick()
{
    bool ok = true;
    {
        World world;
        Recorder recorder;
        world.addListener(recorder);
        const int_t y = 64;
        world.raw(0, y - 1, 0, stoneId(), 0);
        world.setTileAndData(0, y, 0, Tile::doorWood.id, 1);
        world.setTileAndData(0, y + 1, 0, Tile::doorWood.id, 9);
        Player player(world);
        recorder.events.clear();
        Tile::doorWood.attack(world, 0, y, 0, player);
        ok &= expect((world.getData(0, y, 0) & 4) != 0, "left-click toggles a wooden door");
        ok &= expect((world.getData(0, y + 1, 0) & 4) != 0, "left-click toggles both door halves");
        ok &= expect(!recorder.events.empty() && recorder.events.back().id == 1003, "door toggle routes world event 1003");
        int_t bottom = world.getData(0, y, 0);
        int_t top = world.getData(0, y + 1, 0);
        ok &= expect(top == (bottom | 8), "door halves retain matching orientation and open state");
        world.removeListener(recorder);
    }
    {
        World world;
        Player player(world);
        const int_t y = 64;
        world.raw(4, y - 1, 0, stoneId(), 0);
        world.setTileAndData(4, y, 0, Tile::doorIron.id, 1);
        world.setTileAndData(4, y + 1, 0, Tile::doorIron.id, 9);
        Tile::doorIron.attack(world, 4, y, 0, player);
        ok &= expect(world.getData(4, y, 0) == 1, "iron doors refuse left-click toggles");
        Tile::doorIron.use(world, 4, y, 0, player);
        ok &= expect(world.getData(4, y, 0) == 1, "iron doors refuse right-click toggles");
    }
    {
        World world;
        Recorder recorder;
        world.addListener(recorder);
        Player player(world);
        const int_t y = 64;
        world.raw(8, y, 1, stoneId(), 0);
        world.setTileAndData(8, y, 0, Tile::trapdoor.id, 0);
        recorder.events.clear();
        Tile::trapdoor.attack(world, 8, y, 0, player);
        ok &= expect((world.getData(8, y, 0) & 4) != 0, "left-click toggles a trapdoor");
        ok &= expect(!recorder.events.empty() && recorder.events.back().id == 1003, "trapdoor toggle routes world event 1003");
        world.removeListener(recorder);
    }
    {
        Tile::trapdoor.updateDefaultShape();
        ok &= expect(near(Tile::trapdoor.xx0, 0.0) && near(Tile::trapdoor.xx1, 1.0), "trapdoor item bounds span the block");
        ok &= expect(near(Tile::trapdoor.yy0, 0.5 - 3.0 / 32.0) && near(Tile::trapdoor.yy1, 0.5 + 3.0 / 32.0),
            "trapdoor item bounds center on the middle");
    }
    std::cout << "audit-attachments door/trapdoor: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool leverBehavior()
{
    bool ok = true;
    {
        World world;
        const int_t y = 64;
        world.raw(0, y, 0, Tile::lever.id, 5 | 8);
        ok &= expect(Tile::lever.getDirectSignal(world, 0, y, 0, 1), "floor orientation 5 powers downward");
        world.setDataNoUpdate(0, y, 0, 6 | 8);
        ok &= expect(Tile::lever.getDirectSignal(world, 0, y, 0, 1), "floor orientation 6 powers downward");
        ok &= expect(!Tile::lever.getDirectSignal(world, 0, y, 0, 0), "floor orientations do not power upward");
        world.setDataNoUpdate(0, y, 0, 5);
        ok &= expect(!Tile::lever.getDirectSignal(world, 0, y, 0, 1), "unpowered levers give no direct signal");
        world.setDataNoUpdate(0, y, 0, 4 | 8);
        ok &= expect(Tile::lever.getDirectSignal(world, 0, y, 0, 2), "wall orientation 4 powers its facing");
        ok &= expect(!Tile::lever.getDirectSignal(world, 0, y, 0, 1), "wall orientations do not power downward");
    }
    {
        World world;
        world.raw(4, 64, 0, Tile::lever.id, 5);
        Tile::lever.updateShape(world, 4, 64, 0);
        ok &= expect(near(Tile::lever.xx0, 0.25) && near(Tile::lever.xx1, 0.75), "floor lever selection spans the wide box");
        ok &= expect(near(Tile::lever.yy0, 0.0) && near(Tile::lever.yy1, 0.6), "floor lever selection keeps the low height");
        world.setDataNoUpdate(4, 64, 0, 1);
        Tile::lever.updateShape(world, 4, 64, 0);
        ok &= expect(near(Tile::lever.yy0, 0.2) && near(Tile::lever.yy1, 0.8), "wall lever selection keeps the tall box");
        ok &= expect(near(Tile::lever.xx1, 0.375), "wall orientation 1 keeps the narrow width");
    }
    {
        World world;
        const int_t y = 64;
        world.raw(8, y - 1, 0, stoneId(), 0);
        world.setTileAndData(8, y, 0, Tile::lever.id, 0);
        Tile::lever.setPlacedOnFace(world, 8, y, 0, Facing::UP);
        int_t floorData = world.getData(8, y, 0);
        ok &= expect(floorData == 5 || floorData == 6, "floor placement draws orientation 5 or 6");
        world.raw(9, y, 1, stoneId(), 0);
        world.setTileAndData(9, y, 0, Tile::lever.id, 0);
        Tile::lever.setPlacedOnFace(world, 9, y, 0, Facing::NORTH);
        ok &= expect(world.getData(9, y, 0) == 4, "wall placement keeps the clicked-face orientation");
        world.setTileAndData(10, y, 0, Tile::lever.id, 0);
        Tile::lever.setPlacedOnFace(world, 10, y, 0, Facing::NORTH);
        ok &= expect(world.getTile(10, y, 0) == 0, "invalid faces drop instead of searching another wall");
    }
    {
        World world;
        const int_t y = 64;
        world.raw(12, y - 1, 0, stoneId(), 0);
        ok &= expect(world.setTileAndData(12, y, 0, Tile::lever.id, 5), "lever placement writes");
        ok &= expect(world.setTileAndData(12, y, 0, Tile::lever.id, 13), "powered lever metadata rewrites");
        ok &= expect(world.getTile(12, y, 0) == Tile::lever.id && world.getData(12, y, 0) == 13,
            "chunk updates preserve powered lever metadata without an onPlace reset");
        Player player(world);
        world.setDataNoUpdate(12, y, 0, 5);
        Tile::lever.use(world, 12, y, 0, player);
        ok &= expect(world.getData(12, y, 0) == (5 | 8), "lever use toggles the powered bit");
        world.isOnline = true;
        Tile::lever.use(world, 12, y, 0, player);
        ok &= expect(world.getData(12, y, 0) == (5 | 8), "online lever use keeps server authority");
        world.isOnline = false;
    }
    std::cout << "audit-attachments lever: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool buttonBehavior()
{
    bool ok = true;
    World world;
    const int_t y = 64;
    world.raw(0, y, 1, stoneId(), 0);
    ok &= expect(Tile::buttonStone.mayPlace(world, 0, y, 0), "button admits a supported wall");
    ok &= expect(Tile::buttonStone.mayPlaceOnFace(world, 0, y, 0, Facing::NORTH), "button admits the clicked supported wall");
    ok &= expect(!Tile::buttonStone.mayPlaceOnFace(world, 0, y, 0, Facing::SOUTH), "button refuses the unsupported clicked wall");
    world.setTileAndData(0, y, 0, Tile::buttonStone.id, 0);
    Tile::buttonStone.setPlacedOnFace(world, 0, y, 0, Facing::NORTH);
    ok &= expect(world.getData(0, y, 0) == 4, "button keeps the clicked-face orientation");
    world.raw(4 - 1, y, 0, stoneId(), 0);
    world.raw(4 + 1, y, 0, 0, 0);
    world.raw(4, y, -1, 0, 0);
    world.raw(4, y, 1, 0, 0);
    world.setTileAndData(4, y, 0, Tile::buttonStone.id, 0);
    Tile::buttonStone.setPlacedOnFace(world, 4, y, 0, Facing::SOUTH);
    ok &= expect(world.getData(4, y, 0) == 1, "button falls back to the supported wall per the reference orientation scan");
    Player player(world);
    world.raw(8, y, 1, stoneId(), 0);
    world.setTileAndData(8, y, 0, Tile::buttonStone.id, 4);
    Tile::buttonStone.use(world, 8, y, 0, player);
    ok &= expect(world.getData(8, y, 0) == (4 | 8), "button use sets the powered bit");
    Tile::buttonStone.use(world, 8, y, 0, player);
    ok &= expect(world.getData(8, y, 0) == (4 | 8), "powered button use is a no-op");
    world.isOnline = true;
    Tile::buttonStone.tick(world, 8, y, 0, world.random);
    ok &= expect(world.getData(8, y, 0) == (4 | 8), "online button ticks keep server authority");
    world.isOnline = false;
    Tile::buttonStone.tick(world, 8, y, 0, world.random);
    ok &= expect(world.getData(8, y, 0) == 4, "offline button ticks release");
    world.raw(8, y, 1, 0, 0);
    Tile::buttonStone.neighborChanged(world, 8, y, 0, 0);
    ok &= expect(world.getTile(8, y, 0) == 0, "button drops once its wall is gone");
    std::cout << "audit-attachments button: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool torchFence()
{
    bool ok = true;
    World world;
    const int_t y = 64;
    world.raw(0, y - 1, 0, fenceId(), 0);
    ok &= expect(Tile::torch.mayPlace(world, 0, y, 0), "torch admits a fence below");
    world.setTileAndData(0, y, 0, Tile::torch.id, 0);
    Tile::torch.setPlacedOnFace(world, 0, y, 0, Facing::UP);
    ok &= expect(world.getData(0, y, 0) == 5, "torch on a fence top stands upright");
    Tile::torch.neighborChanged(world, 0, y, 0, fenceId());
    ok &= expect(world.getTile(0, y, 0) == Tile::torch.id, "torch survives fence-top neighbor updates");
    world.raw(4, y - 1, 0, stoneId(), 0);
    world.raw(4 - 1, y, 0, fenceId(), 0);
    world.setTileAndData(4, y, 0, Tile::torch.id, 0);
    Tile::torch.setPlacedOnFace(world, 4, y, 0, Facing::EAST);
    ok &= expect(world.getData(4, y, 0) == 5, "a fence-side click preserves the supported floor orientation");
    world.raw(8, y, 1, stoneId(), 0);
    world.setTileAndData(8, y, 0, Tile::torch.id, 0);
    Tile::torch.setPlacedOnFace(world, 8, y, 0, Facing::NORTH);
    ok &= expect(world.getData(8, y, 0) == 4, "torch keeps the clicked wall orientation");
    world.raw(8, y, 1, tntId(), 0);
    Tile::torch.neighborChanged(world, 8, y, 0, tntId());
    ok &= expect(world.getTile(8, y, 0) == 0, "torch drops once its wall stops being a normal cube");
    world.raw(12, y - 1, 0, tntId(), 0);
    ok &= expect(!Tile::torch.mayPlace(world, 12, y, 0), "torch refuses a TNT-only floor");
    world.raw(12 - 1, y, 0, stoneId(), 0);
    ok &= expect(Tile::torch.mayPlace(world, 12, y, 0), "torch admits normal-cube walls");
    std::cout << "audit-attachments torch: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool pressureAuthority()
{
    bool ok = true;
    {
        World world;
        const int_t y = 64;
        world.raw(0, y - 1, 0, stoneId(), 0);
        world.setTileAndData(0, y, 0, Tile::pressurePlateStone.id, 0);
        auto mob = std::make_shared<PigZombie>(world);
        mob->moveTo(0.5, static_cast<double>(y) + 0.1, 0.5, 0.0f, 0.0f);
        world.addEntity(mob);
        Tile::pressurePlateStone.entityInside(world, 0, y, 0, *mob);
        ok &= expect(world.getData(0, y, 0) == 1, "offline plate collision presses");
        world.isOnline = true;
        world.setDataNoUpdate(0, y, 0, 0);
        Tile::pressurePlateStone.entityInside(world, 0, y, 0, *mob);
        ok &= expect(world.getData(0, y, 0) == 0, "online plate collision keeps server authority");
        world.isOnline = false;
    }
    {
        World world;
        const int_t y = 64;
        world.raw(4, y - 1, 0, stoneId(), 0);
        world.setTileAndData(4, y, 0, Tile::pressurePlateStone.id, 1);
        world.isOnline = true;
        Tile::pressurePlateStone.tick(world, 4, y, 0, world.random);
        ok &= expect(world.getData(4, y, 0) == 1, "online plate ticks keep server authority");
        world.isOnline = false;
        Tile::pressurePlateStone.tick(world, 4, y, 0, world.random);
        ok &= expect(world.getData(4, y, 0) == 0, "offline plate ticks release with no riders");
    }
    {
        World world;
        const int_t y = 64;
        world.raw(8, y, 1, stoneId(), 0);
        world.setTileAndData(8, y, 0, Tile::trapdoor.id, 0);
        world.raw(8, y, 1, 0, 0);
        world.isOnline = true;
        Tile::trapdoor.neighborChanged(world, 8, y, 0, 0);
        ok &= expect(world.getTile(8, y, 0) == Tile::trapdoor.id, "online trapdoor neighbors keep server authority");
        world.isOnline = false;
        Tile::trapdoor.neighborChanged(world, 8, y, 0, 0);
        ok &= expect(world.getTile(8, y, 0) == 0, "offline trapdoor neighbors drop unsupported plates");
    }
    std::cout << "audit-attachments authority: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool cakeAndSlab()
{
    bool ok = true;
    {
        World world;
        Player player(world);
        const int_t y = 64;
        world.raw(0, y - 1, 0, stoneId(), 0);
        ok &= expect(Tile::cake.mayPlace(world, 0, y, 0), "cake admits solid material below");
        world.raw(1, y - 1, 0, 0, 0);
        ok &= expect(!Tile::cake.mayPlace(world, 1, y, 0), "cake refuses air below");
        world.setTileAndData(0, y, 0, Tile::cake.id, 0);
        player.health = 10;
        Tile::cake.use(world, 0, y, 0, player);
        ok &= expect(player.health == 13, "cake heals three half-hearts");
        ok &= expect(world.getData(0, y, 0) == 1, "cake use advances one slice");
        player.health = 20;
        Tile::cake.use(world, 0, y, 0, player);
        ok &= expect(world.getData(0, y, 0) == 1, "full health refuses cake");
        player.health = 10;
        world.setDataNoUpdate(0, y, 0, 5);
        Tile::cake.attack(world, 0, y, 0, player);
        ok &= expect(world.getTile(0, y, 0) == 0, "the last slice removes the cake");
        world.setTileAndData(4, y, 0, Tile::cake.id, 0);
        world.raw(4, y - 1, 0, 0, 0);
        Tile::cake.neighborChanged(world, 4, y, 0, 0);
        ok &= expect(world.getTile(4, y, 0) == 0, "cake drops once its support is gone");
    }
    {
        World world;
        const int_t y = 64;
        world.raw(8, y, 0, Tile::slabSingle.id, 3);
        world.setDataNoUpdate(8, y, 0, 3);
        ok &= expect(world.setTileAndData(8, y + 1, 0, Tile::slabSingle.id, 3), "upper slab placement writes");
        ok &= expect(world.getTile(8, y + 1, 0) == 0, "matching slabs consume the upper half");
        ok &= expect(world.getTile(8, y, 0) == Tile::slabDouble.id && world.getData(8, y, 0) == 3,
            "matching slabs merge into a double slab below");
    }
    std::cout << "audit-attachments cake/slab: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool ladderPlacement()
{
    bool ok = true;
    World world;
    const int_t y = 64;
    world.raw(0, y, 1, stoneId(), 0);
    world.setTileAndData(0, y, 0, Tile::ladder.id, 0);
    Tile::ladder.setPlacedOnFace(world, 0, y, 0, Facing::NORTH);
    ok &= expect(world.getData(0, y, 0) == 2, "ladder keeps the clicked wall");
    world.setDataNoUpdate(0, y, 0, 0);
    world.raw(0, y, 1, 0, 0);
    world.raw(0, y, -1, stoneId(), 0);
    Tile::ladder.setPlacedOnFace(world, 0, y, 0, Facing::NORTH);
    ok &= expect(world.getData(0, y, 0) == 3, "ladder without orientation takes the first supported wall");
    world.raw(0, y, -1, 0, 0);
    Tile::ladder.neighborChanged(world, 0, y, 0, 0);
    ok &= expect(world.getTile(0, y, 0) == 0, "ladder drops once its wall is gone");
    std::cout << "audit-attachments ladder: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

}

bool runAuditAttachmentsCases()
{
    bool ok = AuditAttachments::pumpkinAndFence();
    ok &= AuditAttachments::snowSupport();
    ok &= AuditAttachments::attachmentSupport();
    ok &= AuditAttachments::doorAndTrapdoorClick();
    ok &= AuditAttachments::leverBehavior();
    ok &= AuditAttachments::buttonBehavior();
    ok &= AuditAttachments::torchFence();
    ok &= AuditAttachments::pressureAuthority();
    ok &= AuditAttachments::cakeAndSlab();
    ok &= AuditAttachments::ladderPlacement();
    std::cout << "audit-attachments: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}
