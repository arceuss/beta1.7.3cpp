// Boat and minecart collision, inventory and interpolation regressions.

#include "world/level/tile/LiquidTile.h"
#include "world/level/LevelListener.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "world/entity/Entity.h"
#include "world/entity/animal/Pig.h"
#include "world/entity/item/EntityBoat.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/item/EntityMinecart.h"
#include "world/entity/player/Player.h"
#include "world/item/ItemInstance.h"
#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/TileEntity.h"
#include "util/Mth.h"

namespace AuditVehicles
{

bool expect(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "audit-vehicles FAILED: " << message << '\n';
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
    jstring gatherStats() override { return u"audit-vehicles"; }
};

class World : public Level
{
public:
    std::shared_ptr<Source> source = std::make_shared<Source>();

    World() : Level(u"audit-vehicles", Dimension::Id_Normal, 987654321LL, false)
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
    int_t splashes = 0;
    void clear() { splashes = 0; }
    void tileChanged(int_t, int_t, int_t) override {}
    void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
    void allChanged() override {}
    void playSound(const jstring &, double, double, double, float, float) override {}
    void addParticle(const jstring &name, double, double, double, double, double, double) override
    {
        if (name == u"splash")
            splashes++;
    }
    void playMusic(const jstring &, double, double, double, float) override {}
    void entityAdded(std::shared_ptr<Entity>) override {}
    void entityRemoved(std::shared_ptr<Entity>) override {}
    void skyColorChanged() override {}
    void playStreamingMusic(const jstring &, int_t, int_t, int_t) override {}
    void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
    void levelEvent(Player *, int_t, int_t, int_t, int_t, int_t) override {}
};

int_t countItems(World &world, int_t itemID)
{
    int_t total = 0;
    for (const auto &entity : world.entities)
    {
        EntityItem *item = dynamic_cast<EntityItem *>(entity.get());
        if (item != nullptr && item->item.itemID == itemID)
            total += item->item.stackSize;
    }
    return total;
}

int_t countItemEntities(World &world)
{
    int_t total = 0;
    for (const auto &entity : world.entities)
        if (dynamic_cast<EntityItem *>(entity.get()) != nullptr)
            total++;
    return total;
}

// F061: Boat.getCollideAgainstBox returns the other entity's live box even when
// that entity's own getCollideBox is null (all carts and mobs). The old code
// forwarded getCollideBox and lost every such collision.
bool boatCollideBox()
{
    bool ok = true;
    World world;
    auto boat = std::make_shared<EntityBoat>(world, 0.5, 65.0, 0.5);
    auto cart = std::make_shared<EntityMinecart>(world, 5.5, 65.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
    world.addEntity(boat);
    world.addEntity(cart);
    ok &= expect(cart->getCollideBox() == nullptr, "precondition: a cart reports no ordinary collide box");
    AABB *against = boat->getCollideAgainstBox(*cart);
    ok &= expect(against != nullptr, "a boat still collides with a cart whose own box is null");
    if (against != nullptr)
    {
        ok &= expect(against->x0 == cart->bb.x0 && against->y0 == cart->bb.y0 && against->z0 == cart->bb.z0 &&
            against->x1 == cart->bb.x1 && against->y1 == cart->bb.y1 && against->z1 == cart->bb.z1,
            "the boat uses the cart's live bounding box");
    }
    auto pig = std::make_shared<Pig>(world);
    pig->moveTo(10.5, 65.0, 0.5, 0.0f, 0.0f);
    world.addEntity(pig);
    AABB *mobBox = boat->getCollideAgainstBox(*pig);
    ok &= expect(mobBox != nullptr, "a boat still collides with a mob whose own box is null");
    std::cout << "audit-vehicles boat collide: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

// F061: the splash loop compares the int counter against 1+speed*60 instead of
// truncating first. At 0.16 (bound 10.6) the reference emits 11 particles.
bool boatSplashCount()
{
    bool ok = true;
    World world;
    world.isOnline = false;
    Recorder recorder;
    world.addListener(recorder);
    auto boat = std::make_shared<EntityBoat>(world, 0.5, 70.0, 0.5);
    world.addEntity(boat);
    boat->moveTo(0.5, 70.0, 0.5, 0.0f, 0.0f);
    boat->xd = 0.16;
    boat->yd = 0.0;
    boat->zd = 0.0;
    boat->onGround = false;
    boat->horizontalCollision = false;
    recorder.clear();
    boat->tick();
    ok &= expect(recorder.splashes == 11, "a fractional splash bound emits the extra particle");
    world.removeListener(recorder);
    std::cout << "audit-vehicles boat splash: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}
// F061 water part: the slice test is height-aware. Full still water floats the
// hull while the same block drained to data 7 leaves the same hull almost dry.
bool boatWaterHeight()
{
    bool ok = true;
    auto run = [](int_t data) {
        World world;
        world.isOnline = false;
        world.raw(0, 64, 0, Tile::calmWater.id, data);
        world.raw(1, 64, 0, Tile::calmWater.id, data);
        world.raw(-1, 64, 0, Tile::calmWater.id, data);
        world.raw(0, 64, 1, Tile::calmWater.id, data);
        world.raw(0, 64, -1, Tile::calmWater.id, data);
        auto boat = std::make_shared<EntityBoat>(world, 0.5, 64.5, 0.5);
        world.addEntity(boat);
        boat->moveTo(0.5, 64.5, 0.5, 0.0f, 0.0f);
        boat->xd = 0.0;
        boat->yd = 0.0;
        boat->zd = 0.0;
        boat->onGround = false;
        boat->horizontalCollision = false;
        boat->tick();
        return boat->yd;
    };
    double fullYd = run(0);
    double lowYd = run(7);
    ok &= expect(fullYd > lowYd, "drained water buoys less than full water at the same hull height");
    ok &= expect(fullYd > 0.0, "full water pushes a sunk hull up");
    std::cout << "audit-vehicles boat water: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

// F063: specialized cart push, placement blocking and cart-only tick delivery.
bool minecartPush()
{
    bool ok = true;
    {
        World world;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        auto boat = std::make_shared<EntityBoat>(world, 5.5, 65.0, 0.5);
        ok &= expect(cart->blocksBuilding && boat->blocksBuilding, "vehicles block placement");
    }
    {
        // A moving empty rideable cart mounts a touched mob. Through a base
        // Entity pointer this only happens when push dispatches virtually.
        World world;
        world.isOnline = false;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(cart);
        cart->moveTo(0.5, 65.0, 0.5, 0.0f, 0.0f);
        cart->xd = 0.5;
        cart->zd = 0.0;
        auto pig = std::make_shared<Pig>(world);
        pig->moveTo(1.0, 65.0, 0.5, 0.0f, 0.0f);
        world.addEntity(pig);
        Entity *base = cart.get();
        base->push(*pig);
        ok &= expect(pig->riding.get() == cart.get(), "a moving empty cart picks up a touched mob");
    }
    {
        // Separations under 1e-4 never push.
        World world;
        world.isOnline = false;
        auto a = std::make_shared<EntityMinecart>(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        auto b = std::make_shared<EntityMinecart>(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(a);
        world.addEntity(b);
        a->moveTo(0.5, 65.0, 0.5, 0.0f, 0.0f);
        b->moveTo(0.5, 65.0, 0.5, 0.0f, 0.0f);
        a->xd = 0.3;
        b->xd = -0.2;
        a->push(*b);
        ok &= expect(a->xd == 0.3 && b->xd == -0.2, "touching carts with no separation keep their motion");
    }
    {
        // The tick hands collisions only to other carts, so a nearby mob keeps
        // its motion while a nearby cart does not.
        World world;
        world.isOnline = false;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(cart);
        cart->moveTo(0.5, 65.0, 0.5, 0.0f, 0.0f);
        cart->xd = 0.0;
        cart->yd = 0.0;
        cart->zd = 0.0;
        auto pig = std::make_shared<Pig>(world);
        pig->moveTo(0.9, 65.0, 0.5, 0.0f, 0.0f);
        world.addEntity(pig);
        pig->xd = 0.0;
        auto other = std::make_shared<EntityMinecart>(world, 0.1, 65.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(other);
        other->moveTo(0.1, 65.0, 0.5, 0.0f, 0.0f);
        other->xd = 0.3;
        other->zd = 0.0;
        cart->xd = 0.25;
        cart->zd = 0.0;
        cart->tick();
        ok &= expect(pig->xd == 0.0 && pig->zd == 0.0, "a cart tick never shoves a nearby mob");
    }
    std::cout << "audit-vehicles cart push: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

// F064: chest-cart cargo is conserved exactly once, uses the cart RNG and the
// container cap of 64.
bool chestCargo()
{
    bool ok = true;
    ok &= expect(Items::bread->getMaxStackSize() == 1, "precondition: bread stacks to one");
    {
        World world;
        EntityMinecart cart(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_CHEST);
        cart.setItem(0, ItemInstance(Items::stick->getShiftedIndex(), 65, 0));
        cart.setItem(1, ItemInstance(Items::bread->getShiftedIndex(), 2, 0));
        ok &= expect(cart.getItem(0).stackSize == 64, "the cart setter clamps 65 to the container limit");
        ok &= expect(cart.getItem(1).stackSize == 2, "the cart setter keeps two of a one-per-slot item");
    }
    {
        // Removing a whole stack clears the slot to null, so a later scatter
        // sees it as empty.
        World world;
        EntityMinecart cart(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_CHEST);
        cart.setItem(0, ItemInstance(Items::stick->getShiftedIndex(), 10, 0));
        ItemInstance taken = cart.removeItem(0, 10);
        ok &= expect(taken.stackSize == 10, "a full removal returns the whole stack");
        ok &= expect(cart.getStackInSlot(0) == nullptr, "an emptied cart slot reads back as null");
        ItemInstance empty = cart.removeItem(0, 1);
        ok &= expect(empty.isEmpty(), "removing from an emptied slot returns nothing");
    }
    {
        // Direct removal scatters the cargo with the cart's own RNG and leaves
        // the world RNG alone.
        World world;
        world.isOnline = false;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_CHEST);
        world.addEntity(cart);
        cart->moveTo(0.5, 65.0, 0.5, 0.0f, 0.0f);
        cart->setItem(0, ItemInstance(Items::stick->getShiftedIndex(), 50, 0));
        const ulong_t worldBefore = world.random.rawState();
        const ulong_t cartBefore = cart->auditRandom().rawState();
        cart->remove();
        ok &= expect(world.random.rawState() == worldBefore, "cargo scatter never draws world RNG");
        ok &= expect(cart->auditRandom().rawState() != cartBefore, "cargo scatter draws the cart RNG");
        ok &= expect(countItems(world, Items::stick->getShiftedIndex()) == 50, "a direct removal drops every stored item once");
    }
    {
        // A lethal hit drops the cargo plus exactly one cart and one chest.
        World world;
        world.isOnline = false;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 65.0, 0.5, EntityMinecart::TYPE_CHEST);
        world.addEntity(cart);
        cart->moveTo(0.5, 65.0, 0.5, 0.0f, 0.0f);
        cart->setItem(0, ItemInstance(Items::stick->getShiftedIndex(), 40, 0));
        cart->hurt(nullptr, 100);
        ok &= expect(countItems(world, Items::stick->getShiftedIndex()) == 40, "a lethal hit drops the cargo exactly once");
        ok &= expect(countItems(world, Items::minecart->getShiftedIndex()) == 1, "a lethal hit drops one cart");
        ok &= expect(countItemEntities(world) >= 2, "cargo and vehicle drops are separate entities");
    }
    std::cout << "audit-vehicles chest cargo: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

// F065: interpolation stores the target Y unchanged, wraps yaw and restores
// saved motion.
bool minecartLerp()
{
    bool ok = true;
    {
        // Target Y is stored exactly; with 3 requested steps the cart keeps
        // steps+2 = 5, so one online tick covers a fifth of the way to 20.
        World world;
        world.isOnline = true;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 10.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(cart);
        cart->setPos(0.5, 10.0, 0.5);
        cart->lerpTo(0.5, 20.0, 0.5, 0.0f, 0.0f, 3);
        cart->tick();
        ok &= expect(std::abs(cart->y - 12.0) < 1.0e-9, "interpolation aims at the packet Y with no height lift");
    }
    {
        // Yaw 179 to -179 wraps the short way: 179 + 2/5 = 179.4.
        World world;
        world.isOnline = true;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 10.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(cart);
        cart->moveTo(0.5, 10.0, 0.5, 179.0f, 0.0f);
        cart->yRotO = 179.0f;
        cart->lerpTo(0.5, 10.0, 0.5, -179.0f, 0.0f, 3);
        cart->tick();
        ok &= expect(std::abs(cart->yRot - 179.4f) < 0.05f, "yaw interpolation wraps across the far meridian");
    }
    {
        // A velocity packet followed by a position packet restores the saved
        // motion instead of leaving whatever ran last.
        World world;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 10.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(cart);
        cart->lerpMotion(5.0, 6.0, 7.0);
        cart->xd = 0.0;
        cart->yd = 0.0;
        cart->zd = 0.0;
        cart->lerpTo(0.5, 10.0, 0.5, 0.0f, 0.0f, 3);
        ok &= expect(cart->xd == 5.0 && cart->yd == 6.0 && cart->zd == 7.0, "a position packet restores the saved motion");
    }
    std::cout << "audit-vehicles cart lerp: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

// F066 (vehicles): gravity and drag narrow through float before widening.
bool vehicleFloat()
{
    bool ok = true;
    {
        World world;
        world.isOnline = false;
        auto cart = std::make_shared<EntityMinecart>(world, 0.5, 70.0, 0.5, EntityMinecart::TYPE_RIDEABLE);
        world.addEntity(cart);
        cart->moveTo(0.5, 70.0, 0.5, 0.0f, 0.0f);
        cart->xd = 0.0;
        cart->yd = 0.0;
        cart->zd = 0.0;
        cart->onGround = false;
        cart->tick();
        const double floatYd = -static_cast<double>(0.04f) * static_cast<double>(0.95f);
        const double doubleYd = -0.04 * 0.95;
        ok &= expect(cart->yd == floatYd, "off-rail gravity and drag use the float-narrowed product");
        ok &= expect(cart->yd != doubleYd, "full double gravity would land nearby but not equal");
    }
    {
        World world;
        world.isOnline = false;
        auto boat = std::make_shared<EntityBoat>(world, 0.5, 70.0, 0.5);
        world.addEntity(boat);
        boat->moveTo(0.5, 70.0, 0.5, 0.0f, 0.0f);
        boat->xd = 0.0;
        boat->yd = 0.0;
        boat->zd = 0.0;
        boat->onGround = false;
        boat->horizontalCollision = false;
        boat->tick();
        const double floatYd = -static_cast<double>(0.04f) * static_cast<double>(0.95f);
        ok &= expect(boat->yd == floatYd, "dry-boat buoyancy and drag use the float-narrowed product");
    }
    std::cout << "audit-vehicles float parity: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

}

bool runAuditVehiclesCases()
{
    bool ok = AuditVehicles::boatCollideBox();
    ok &= AuditVehicles::boatSplashCount();
    ok &= AuditVehicles::boatWaterHeight();
    ok &= AuditVehicles::minecartPush();
    ok &= AuditVehicles::chestCargo();
    ok &= AuditVehicles::minecartLerp();
    ok &= AuditVehicles::vehicleFloat();
    std::cout << "audit-vehicles: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}
