// Block item dispatch, placement, subtype and item-use regressions.

#include "world/level/tile/BedTile.h"
#include "world/level/tile/ChestTile.h"
#include "world/level/tile/ClothTile.h"
#include "world/level/tile/DirtTile.h"
#include "world/level/tile/DoorTile.h"
#include "world/level/tile/GlassTile.h"
#include "world/level/tile/GrassTile.h"
#include "world/level/tile/JukeboxTile.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/PistonBaseTile.h"
#include "world/level/tile/SaplingTile.h"
#include "world/level/tile/SignTile.h"
#include "world/level/tile/SlabTile.h"
#include "world/level/tile/StoneTile.h"
#include "world/level/tile/TreeTile.h"
#include "world/level/tile/WebTile.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/RedStoneDustTile.h"
#include <iostream>
#include <map>
#include <memory>

#include "world/entity/animal/Pig.h"
#include "world/entity/animal/Sheep.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/RecordPlayerTileEntity.h"

namespace AuditItems
{

bool expect(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "audit-items FAILED: " << message << '\n';
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
    jstring gatherStats() override { return u"audit-items"; }
};

class World : public Level
{
public:
    std::shared_ptr<Source> source = std::make_shared<Source>();

    World() : Level(u"audit-items", Dimension::Id_Normal, 987654321LL, false)
    {
        setChunkSource(source);
        for (int_t cx = -2; cx <= 2; ++cx)
            for (int_t cz = -2; cz <= 2; ++cz)
                source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
    }

    void raw(int_t x, int_t y, int_t z, int_t id, int_t data = 0)
    {
        source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(id);
        setDataNoUpdate(x, y, z, data);
    }
};

class Recorder : public LevelListener
{
public:
    int_t event1005 = 0;

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
    void levelEvent(Player *, int_t event, int_t, int_t, int_t, int_t) override
    {
        if (event == 1005)
            event1005++;
    }
};

// S03/F002: aux-to-data mapping. The cloth icon/name inversion (~aux & 15)
// must not leak into the placed data, leaves set the player bit, pistons use
// the staging sentinel, and ordinary blocks normalize to zero.
bool placedMetadata()
{
    bool ok = true;
    ok &= expect(Item::items[Tile::dirt.id]->getLevelDataForAuxValue(7) == 0, "ordinary block item normalizes aux to 0");
    ok &= expect(Item::items[Tile::wool.id]->getLevelDataForAuxValue(5) == 5, "cloth preserves raw aux in placed data");
    ok &= expect(Item::items[Tile::treeTrunk.id]->getLevelDataForAuxValue(2) == 2, "log preserves raw aux");
    ok &= expect(Item::items[Tile::sapling.id]->getLevelDataForAuxValue(1) == 1, "sapling preserves raw aux");
    ok &= expect(Item::items[Tile::leaves.id]->getLevelDataForAuxValue(1) == 9, "leaves place aux | 8");
    ok &= expect(Item::items[Tile::pistonBase.id]->getLevelDataForAuxValue(4) == 7, "piston stages metadata 7");
    ok &= expect(Item::items[Tile::pistonStickyBase.id]->getLevelDataForAuxValue(0) == 7, "sticky piston stages metadata 7");
    ok &= expect(Item::items[Tile::slabSingle.id]->getLevelDataForAuxValue(5) == 5, "slab forwards raw aux, no masking shortcut");
    return ok;
}

// F067: slab variant names keep the tile root plus the per-aux suffix.
bool slabNames()
{
    bool ok = true;
    ok &= expect(Item::items[Tile::slabSingle.id]->getDescriptionId(ItemInstance(Tile::slabSingle.id, 1, 0)) == u"tile.stoneSlab.stone", "slab aux 0 name");
    ok &= expect(Item::items[Tile::slabSingle.id]->getDescriptionId(ItemInstance(Tile::slabSingle.id, 1, 2)) == u"tile.stoneSlab.wood", "slab aux 2 name");
    ok &= expect(Item::items[Tile::slabSingle.id]->getDescriptionId(ItemInstance(Tile::slabSingle.id, 1, 3)) == u"tile.stoneSlab.cobble", "slab aux 3 name");
    return ok;
}

// S01/F001: a held block without tool permission must not satisfy
// tool-required harvesting; the web exception still works.
bool harvestFallback()
{
    bool ok = true;
    ItemInstance dirt(Tile::dirt.id, 1, 0);
    ok &= expect(!dirt.canDestroySpecial(Tile::rock), "held dirt grants no special harvest");
    ItemInstance unresolvable(31999, 1, 0);
    ok &= expect(unresolvable.getItem() == nullptr, "fixture id has no item");
    ok &= expect(!unresolvable.canDestroySpecial(Tile::rock), "missing item grants no special harvest");
    ItemInstance shears(Items::shears->getShiftedIndex(), 1, 0);
    ok &= expect(shears.canDestroySpecial(Tile::cobweb), "shears keep the web exception");
    ok &= expect(!shears.canDestroySpecial(Tile::rock), "shears grant nothing for stone");
    return ok;
}

// F012/I09: shears wear on leaves/web but report the base boolean so the
// item-use statistic does not fire for ordinary mining.
bool shearsStats()
{
    bool ok = true;
    World world;
    Player player(world);
    ItemInstance onLeaves(Items::shears->getShiftedIndex(), 1, 0);
    ok &= expect(!Items::shears->mineBlock(onLeaves, Tile::leaves.id, 0, 64, 0, player), "shears report base result on leaves");
    ok &= expect(onLeaves.itemDamage == 1, "shears still wear on leaves");
    ItemInstance onStone(Items::shears->getShiftedIndex(), 1, 0);
    ok &= expect(!Items::shears->mineBlock(onStone, Tile::rock.id, 0, 64, 0, player), "shears report base result on stone");
    ok &= expect(onStone.itemDamage == 0, "shears do not wear on stone");
    return ok;
}

// F006: every axe tier mines chests at tier efficiency, stone stays slow.
bool axeChest()
{
    bool ok = true;
    ItemInstance axe(Items::axeWood->getShiftedIndex(), 1, 0);
    ok &= expect(axe.getDestroySpeed(Tile::chest) == 2.0f, "wood axe is efficient on chests");
    ok &= expect(axe.getDestroySpeed(Tile::rock) == 1.0f, "wood axe stays slow on stone");
    return ok;
}

// F010/F011/I10: stew icon and the three hand-equipped flags.
bool registryVisuals()
{
    bool ok = true;
    ItemInstance stew(Items::bowlSoup->getShiftedIndex(), 1, 0);
    ok &= expect(stew.getIcon() == 72, "stew uses atlas index 72");
    ok &= expect(Items::stick->isFull3D(), "stick is hand-equipped");
    ok &= expect(Items::bone->isFull3D(), "bone is hand-equipped");
    ok &= expect(Items::sugar->isFull3D(), "sugar is hand-equipped");
    ok &= expect(!Items::flint->isFull3D(), "flint stays flat");
    return ok;
}

// F008/I08: the hit-entity hook saddles exactly like right-click and always
// reports the use.
bool saddleHit()
{
    bool ok = true;
    World world;
    Player player(world);
    {
        Pig pig(world);
        ok &= expect(!pig.isSaddled(), "pig starts unsaddled");
        ItemInstance saddle(Items::saddle->getShiftedIndex(), 1, 0);
        ok &= expect(Items::saddle->hurtEnemy(saddle, pig, player), "saddle hit reports use");
        ok &= expect(pig.isSaddled() && saddle.stackSize == 0, "hit saddles an eligible pig");
    }
    {
        Pig pig(world);
        pig.setSaddled(true);
        ItemInstance saddle(Items::saddle->getShiftedIndex(), 1, 0);
        ok &= expect(Items::saddle->hurtEnemy(saddle, pig, player), "hit on saddled pig still reports use");
        ok &= expect(saddle.stackSize == 1, "hit on saddled pig consumes nothing");
    }
    {
        Sheep sheep(world);
        ItemInstance saddle(Items::saddle->getShiftedIndex(), 1, 0);
        ok &= expect(Items::saddle->hurtEnemy(saddle, sheep, player), "hit on non-pig still reports use");
        ok &= expect(saddle.stackSize == 1, "hit on non-pig consumes nothing");
    }
    return ok;
}

// F009/I11: the client reports the use without mutating; the server inserts,
// broadcasts event 1005 and consumes.
bool recordGuard()
{
    bool ok = true;
    {
        World world;
        Player player(world);
        world.raw(0, 64, 0, Tile::jukebox.id, 0);
        world.isOnline = true;
        ItemInstance disc(Items::record13->getShiftedIndex(), 1, 0);
        ok &= expect(Items::record13->useOn(disc, player, world, 0, 64, 0, Facing::UP), "online record use reports success");
        ok &= expect(disc.stackSize == 1 && world.getData(0, 64, 0) == 0, "online record use mutates nothing");
        world.isOnline = false;
    }
    {
        World world;
        Recorder recorder;
        world.addListener(recorder);
        Player player(world);
        world.setTileAndData(0, 64, 0, Tile::jukebox.id, 0);
        ItemInstance disc(Items::record13->getShiftedIndex(), 1, 0);
        ok &= expect(Items::record13->useOn(disc, player, world, 0, 64, 0, Facing::UP), "offline record use succeeds");
        auto entity = std::dynamic_pointer_cast<RecordPlayerTileEntity>(world.getTileEntity(0, 64, 0));
        ok &= expect(entity != nullptr && entity->record == Items::record13->getShiftedIndex(), "offline record is stored");
        ok &= expect(world.getData(0, 64, 0) == 1 && disc.stackSize == 0, "offline record consumes and marks played");
        ok &= expect(recorder.event1005 == 1, "offline record broadcasts exactly one event 1005");
    }
    {
        World world;
        Player player(world);
        world.raw(0, 64, 0, Tile::rock.id, 0);
        ItemInstance disc(Items::record13->getShiftedIndex(), 1, 0);
        ok &= expect(!Items::record13->useOn(disc, player, world, 0, 64, 0, Facing::UP), "record on non-jukebox fails");
    }
    return ok;
}

// F005/I01: only aux 15 acts, only on the three targets, only offline.
bool dyeGuards()
{
    bool ok = true;
    World world;
    Player player(world);
    {
        world.raw(0, 64, 0, Tile::rock.id, 0);
        ItemInstance dye(Items::dyePowder->getShiftedIndex(), 1, 0);
        ok &= expect(!Items::dyePowder->useOn(dye, player, world, 0, 64, 0, Facing::UP), "non-bone-meal dye does nothing");
        ok &= expect(dye.stackSize == 1, "non-bone-meal dye is not consumed");
    }
    {
        world.raw(1, 64, 0, Tile::rock.id, 0);
        ItemInstance meal(Items::dyePowder->getShiftedIndex(), 1, 15);
        ok &= expect(!Items::dyePowder->useOn(meal, player, world, 1, 64, 0, Facing::UP), "bone meal rejects unknown targets");
        ok &= expect(meal.stackSize == 1, "rejected bone meal is not consumed");
    }
    {
        world.raw(2, 64, 0, Tile::grass.id, 0);
        ItemInstance meal(Items::dyePowder->getShiftedIndex(), 2, 15);
        ok &= expect(Items::dyePowder->useOn(meal, player, world, 2, 64, 0, Facing::UP), "bone meal accepts grass");
        ok &= expect(meal.stackSize == 1, "accepted bone meal is consumed once");
    }
    return ok;
}

// A03 (soup part): eating always yields one bowl, even from an oversized
// stack that creative-style inventories can hold.
bool soupBowl()
{
    bool ok = true;
    World world;
    Player player(world);
    {
        ItemInstance soup(Items::bowlSoup->getShiftedIndex(), 5, 0);
        Items::bowlSoup->use(soup, world, player);
        ok &= expect(soup.itemID == Items::bowlEmpty->getShiftedIndex() && soup.stackSize == 1, "oversized soup still becomes one bowl");
    }
    {
        ItemInstance soup(Items::bowlSoup->getShiftedIndex(), 1, 0);
        Items::bowlSoup->use(soup, world, player);
        ok &= expect(soup.itemID == Items::bowlEmpty->getShiftedIndex() && soup.stackSize == 1, "normal soup becomes one bowl");
    }
    return ok;
}

// F017/I05: doors defer admission to DoorTile.mayPlace and write both halves
// with notified writes; non-UP clicks fail.
bool doorPlacement()
{
    bool ok = true;
    World world;
    Player player(world);
    player.yRot = 0.0f;
    world.raw(0, 63, 0, Tile::rock.id, 0);
    {
        ItemInstance door(Items::doorWood->getShiftedIndex(), 1, 0);
        ok &= expect(Items::doorWood->useOn(door, player, world, 0, 63, 0, Facing::UP), "door places on stone");
        ok &= expect(world.getTile(0, 64, 0) == Tile::doorWood.id, "door bottom half placed");
        ok &= expect(world.getTile(0, 65, 0) == Tile::doorWood.id, "door top half placed");
        ok &= expect((world.getData(0, 65, 0) & 8) != 0, "door top half carries the upper bit");
        ok &= expect(door.stackSize == 0, "door is consumed");
    }
    {
        ItemInstance door(Items::doorWood->getShiftedIndex(), 1, 0);
        ok &= expect(!Items::doorWood->useOn(door, player, world, 4, 63, 0, Facing::NORTH), "door rejects non-UP faces");
        ok &= expect(door.stackSize == 1, "rejected door is not consumed");
    }
    return ok;
}

// F018: signs need a solid clicked block, then the destination follows the
// air-or-ground-cover rule: liquids pass, solid obstructions fail.
bool signPlacement()
{
    bool ok = true;
    World world;
    Player player(world);
    player.yRot = 0.0f;
    world.raw(0, 63, 0, Tile::rock.id, 0);
    {
        ItemInstance sign(Items::sign->getShiftedIndex(), 1, 0);
        ok &= expect(Items::sign->useOn(sign, player, world, 0, 63, 0, Facing::UP), "standing sign places");
        ok &= expect(world.getTile(0, 64, 0) == Tile::signPost.id, "standing sign writes the post");
        ok &= expect(sign.stackSize == 0, "sign is consumed");
    }
    {
        world.raw(4, 63, 0, Tile::rock.id, 0);
        ItemInstance sign(Items::sign->getShiftedIndex(), 1, 0);
        ok &= expect(!Items::sign->useOn(sign, player, world, 4, 63, 0, Facing::DOWN), "sign rejects DOWN");
    }
    {
        world.raw(8, 63, 0, Tile::rock.id, 0);
        ItemInstance sign(Items::sign->getShiftedIndex(), 1, 0);
        ok &= expect(Items::sign->useOn(sign, player, world, 8, 63, 0, Facing::NORTH), "wall sign places");
        ok &= expect(world.getTile(8, 63, -1) == Tile::signWall.id, "wall sign writes the wall block");
        ok &= expect(world.getData(8, 63, -1) == static_cast<int_t>(Facing::NORTH), "wall sign stores the face");
    }
    {
        world.raw(12, 63, 0, Tile::rock.id, 0);
        world.raw(12, 64, 0, Tile::water.id, 0);
        ItemInstance sign(Items::sign->getShiftedIndex(), 1, 0);
        ok &= expect(Items::sign->useOn(sign, player, world, 12, 63, 0, Facing::UP), "sign replaces liquid ground cover");
        ok &= expect(sign.stackSize == 0 && world.getTile(12, 64, 0) == Tile::signPost.id, "sign consumes one item when replacing water");
    }
    {
        world.raw(12, 64, 0, Tile::rock.id, 0);
        ItemInstance sign(Items::sign->getShiftedIndex(), 1, 0);
        ok &= expect(!Items::sign->useOn(sign, player, world, 12, 63, 0, Facing::UP), "sign rejects a solid destination");
        ok &= expect(sign.stackSize == 1, "rejected sign is not consumed");
    }
    return ok;
}

// S02/F002: solid blocks stop at y=127 through the item itself, and ordinary
// aux values normalize to zero in the world.
bool heightLimit()
{
    bool ok = true;
    World world;
    Player player(world);
    {
        world.raw(0, 126, 0, Tile::rock.id, 0);
        ItemInstance dirt(Tile::dirt.id, 1, 7);
        ok &= expect(!Item::items[Tile::dirt.id]->useOn(dirt, player, world, 0, 126, 0, Facing::UP), "solid placement at y=127 fails");
        ok &= expect(dirt.stackSize == 1 && world.getTile(0, 127, 0) == 0, "failed placement consumes and writes nothing");
    }
    {
        world.raw(0, 64, 0, Tile::rock.id, 0);
        ItemInstance dirt(Tile::dirt.id, 1, 7);
        ok &= expect(Item::items[Tile::dirt.id]->useOn(dirt, player, world, 0, 64, 0, Facing::UP), "ordinary placement succeeds");
        ok &= expect(world.getTile(0, 65, 0) == Tile::dirt.id && world.getData(0, 65, 0) == 0, "ordinary aux normalizes to data 0");
    }
    return ok;
}

// S04 (bed part): beds need air halves and normal-cube support; glass is not
// a normal cube even though it renders solidly.
bool bedSupport()
{
    bool ok = true;
    World world;
    Player player(world);
    player.yRot = 0.0f;
    {
        world.raw(0, 63, 0, Tile::rock.id, 0);
        world.raw(0, 63, 1, Tile::rock.id, 0);
        ItemInstance bed(Items::bed->getShiftedIndex(), 1, 0);
        ok &= expect(Items::bed->useOn(bed, player, world, 0, 63, 0, Facing::UP), "bed places on stone");
        ok &= expect(world.getTile(0, 64, 0) == Tile::bed.id && world.getTile(0, 64, 1) == Tile::bed.id, "both bed halves placed");
    }
    {
        world.raw(4, 63, 0, Tile::glass.id, 0);
        world.raw(4, 63, 1, Tile::glass.id, 0);
        ItemInstance bed(Items::bed->getShiftedIndex(), 1, 0);
        ok &= expect(!Items::bed->useOn(bed, player, world, 4, 63, 0, Facing::UP), "bed rejects glass support");
        ok &= expect(bed.stackSize == 1, "rejected bed is not consumed");
    }
    return ok;
}

bool redstoneOnSnow()
{
    World world;
    Player player(world);
    world.raw(0, 63, 0, Tile::rock.id);
    world.raw(0, 64, 0, Tile::snow.id);
    ItemInstance dust(Items::redstone->getShiftedIndex(), 1, 0);
    bool ok = expect(dust.useOn(player, world, 0, 64, 0, Facing::EAST), "redstone accepts a side click on supported snow");
    ok &= expect(world.getTile(0, 64, 0) == Tile::redstoneWire.id && world.getTile(1, 64, 0) == 0 && dust.stackSize == 0,
        "redstone replaces the clicked snow instead of moving to the adjacent cell");

    world.raw(4, 64, 0, Tile::snow.id);
    ItemInstance unsupported(Items::redstone->getShiftedIndex(), 1, 0);
    ok &= expect(unsupported.useOn(player, world, 4, 64, 0, Facing::UP), "unsupported redstone placement keeps Java's handled return");
    ok &= expect(world.getTile(4, 64, 0) == Tile::snow.id && unsupported.stackSize == 1,
        "unsupported snow does not consume redstone or change the target");
    return ok;
}

}

bool runAuditItemsCases()
{
    bool ok = AuditItems::placedMetadata();
    ok &= AuditItems::slabNames();
    ok &= AuditItems::harvestFallback();
    ok &= AuditItems::shearsStats();
    ok &= AuditItems::axeChest();
    ok &= AuditItems::registryVisuals();
    ok &= AuditItems::saddleHit();
    ok &= AuditItems::recordGuard();
    ok &= AuditItems::dyeGuards();
    ok &= AuditItems::soupBowl();
    ok &= AuditItems::doorPlacement();
    ok &= AuditItems::signPlacement();
    ok &= AuditItems::heightLimit();
    ok &= AuditItems::bedSupport();
    ok &= AuditItems::redstoneOnSnow();
    std::cout << "audit-items: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}
