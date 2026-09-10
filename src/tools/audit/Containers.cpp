// Container drops, dispenser projectiles, block events and spawner authority.

#include "world/level/tile/StoneTile.h"
#include "world/level/LevelListener.h"
#include "world/level/tile/ChestTile.h"
#include "world/level/tile/DispenserTile.h"
#include "world/level/tile/FurnaceTile.h"
#include "world/level/tile/GlassTile.h"
#include "world/level/tile/JukeboxTile.h"
#include "world/level/tile/MobSpawnerTile.h"
#include "world/level/tile/NotGateTile.h"
#include "world/level/tile/NoteTile.h"
#include "world/level/tile/SandTile.h"
#include "world/level/tile/WoodTile.h"
#include "world/level/tile/WorkbenchTile.h"
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "world/entity/Entity.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/monster/PigZombie.h"
#include "world/entity/monster/Giant.h"
#include "world/entity/monster/Zombie.h"
#include "world/entity/player/Player.h"
#include "world/entity/projectile/EntityArrow.h"
#include "world/entity/projectile/EntitySnowball.h"
#include "world/entity/projectile/EntityThrownEgg.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/ChestTileEntity.h"
#include "world/level/tile/entity/DispenserTileEntity.h"
#include "world/level/tile/entity/MobSpawnerTileEntity.h"
#include "world/level/tile/entity/NoteTileEntity.h"
#include "world/level/tile/entity/RecordPlayerTileEntity.h"

namespace AuditContainers
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "audit-containers FAILED: " << message << '\n';
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
	jstring gatherStats() override { return u"audit-containers"; }
};

class World : public Level
{
public:
	std::shared_ptr<Source> source = std::make_shared<Source>();

	World() : Level(u"audit-containers", Dimension::Id_Normal, 987654321LL, false)
	{
		setChunkSource(source);
		for (int_t cx = -2; cx <= 2; ++cx)
			for (int_t cz = -2; cz <= 2; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
	}

	// Raw placement: no onPlace/onRemove, no neighbor notification. Used where a
	// case needs an exact block layout (obstructions, an unsupported redstone
	// torch, a dispenser whose facing must survive setDefaultDirection).
	void raw(int_t x, int_t y, int_t z, int_t id, int_t data = 0)
	{
		source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(id);
		setDataNoUpdate(x, y, z, data);
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

struct Sound
{
	jstring name;
	double x = 0.0, y = 0.0, z = 0.0;
	float volume = 0.0f, pitch = 0.0f;
};

struct Particle
{
	jstring name;
	double x = 0.0, y = 0.0, z = 0.0;
	double xa = 0.0, ya = 0.0, za = 0.0;
};

class Recorder : public LevelListener
{
public:
	std::vector<Event> events;
	std::vector<Sound> sounds;
	std::vector<Particle> particles;
	std::vector<jstring> streams;
	int_t streamCount = 0;

	void clear()
	{
		events.clear();
		sounds.clear();
		particles.clear();
		streams.clear();
		streamCount = 0;
	}

	void tileChanged(int_t, int_t, int_t) override {}
	void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
	void allChanged() override {}
	void playSound(const jstring &name, double x, double y, double z, float volume, float pitch) override
	{
		sounds.push_back({name, x, y, z, volume, pitch});
	}
	void addParticle(const jstring &name, double x, double y, double z, double xa, double ya, double za) override
	{
		particles.push_back({name, x, y, z, xa, ya, za});
	}
	void playMusic(const jstring &, double, double, double, float) override {}
	void entityAdded(std::shared_ptr<Entity>) override {}
	void entityRemoved(std::shared_ptr<Entity>) override {}
	void skyColorChanged() override {}
	void playStreamingMusic(const jstring &name, int_t, int_t, int_t) override
	{
		streams.push_back(name);
		streamCount++;
	}
	void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
	void levelEvent(Player *, int_t event, int_t x, int_t y, int_t z, int_t data) override
	{
		events.push_back({event, x, y, z, data});
	}

	int_t countEvents(int_t id) const
	{
		int_t n = 0;
		for (const Event &e : events)
			if (e.id == id)
				n++;
		return n;
	}
};

int_t chestFace(World &world, int_t x, int_t z, Facing face)
{
	return Tile::chest.getTexture(world, x, 64, z, face);
}

// F031 / B26: the north/south pair defaults its front to EAST(5) and swaps to
// WEST(4) only for an east-side obstruction. ChestTile.java:38-46. tex is 26.
bool chestTextures()
{
	bool ok = true;
	const int_t tex = Tile::chest.tex;
	ok &= expect(tex == 26, "chest base texture index");

	{
		World world;
		world.raw(0, 64, 0, Tile::chest.id);
		world.raw(0, 64, 1, Tile::chest.id);
		ok &= expect(chestFace(world, 0, 0, Facing::WEST) == tex + 32 - 1 &&
			chestFace(world, 0, 0, Facing::EAST) == tex + 16 &&
			chestFace(world, 0, 1, Facing::WEST) == tex + 32 &&
			chestFace(world, 0, 1, Facing::EAST) == tex + 16 - 1,
			"unobstructed north/south pair fronts east");
		ok &= expect(chestFace(world, 0, 0, Facing::NORTH) == tex &&
			chestFace(world, 0, 0, Facing::SOUTH) == tex &&
			chestFace(world, 0, 0, Facing::UP) == tex - 1 &&
			chestFace(world, 0, 0, Facing::DOWN) == tex - 1,
			"north/south pair keeps plain side and lid textures");
	}
	{
		// A west obstruction selects the same front as no obstruction at all;
		// getting these two apart is what the reversed table used to break.
		World world;
		world.raw(0, 64, 0, Tile::chest.id);
		world.raw(0, 64, 1, Tile::chest.id);
		world.raw(-1, 64, 0, Tile::cobblestone.id);
		world.raw(-1, 64, 1, Tile::cobblestone.id);
		ok &= expect(chestFace(world, 0, 0, Facing::WEST) == tex + 32 - 1 &&
			chestFace(world, 0, 0, Facing::EAST) == tex + 16 &&
			chestFace(world, 0, 1, Facing::WEST) == tex + 32 &&
			chestFace(world, 0, 1, Facing::EAST) == tex + 16 - 1,
			"west-obstructed north/south pair still fronts east");
	}
	{
		World world;
		world.raw(0, 64, 0, Tile::chest.id);
		world.raw(0, 64, 1, Tile::chest.id);
		world.raw(1, 64, 0, Tile::cobblestone.id);
		world.raw(1, 64, 1, Tile::cobblestone.id);
		ok &= expect(chestFace(world, 0, 0, Facing::WEST) == tex + 16 - 1 &&
			chestFace(world, 0, 0, Facing::EAST) == tex + 32 &&
			chestFace(world, 0, 1, Facing::WEST) == tex + 16 &&
			chestFace(world, 0, 1, Facing::EAST) == tex + 32 - 1,
			"east-obstructed north/south pair fronts west");
	}
	{
		// The east/west pair and the single chest were already correct; they are
		// here so a future edit to the shared branch cannot pass unnoticed.
		World world;
		world.raw(4, 64, 0, Tile::chest.id);
		world.raw(5, 64, 0, Tile::chest.id);
		ok &= expect(Tile::chest.getTexture(world, 4, 64, 0, Facing::SOUTH) == tex + 16 - 1 &&
			Tile::chest.getTexture(world, 4, 64, 0, Facing::NORTH) == tex + 32 &&
			Tile::chest.getTexture(world, 5, 64, 0, Facing::SOUTH) == tex + 16 &&
			Tile::chest.getTexture(world, 5, 64, 0, Facing::NORTH) == tex + 32 - 1 &&
			Tile::chest.getTexture(world, 4, 64, 0, Facing::WEST) == tex,
			"unobstructed east/west pair fronts south");

		World single;
		single.raw(0, 64, 0, Tile::chest.id);
		ok &= expect(chestFace(single, 0, 0, Facing::SOUTH) == tex + 1 &&
			chestFace(single, 0, 0, Facing::NORTH) == tex &&
			chestFace(single, 0, 0, Facing::WEST) == tex &&
			chestFace(single, 0, 0, Facing::EAST) == tex,
			"single chest fronts south");
	}

	// F035 / B26: WorkbenchTile.java:19 decorates faces 2 and 4.
	const int_t bench = Tile::workBench.tex;
	ok &= expect(Tile::workBench.getTexture(Facing::NORTH, 0) == bench + 1 &&
		Tile::workBench.getTexture(Facing::WEST, 0) == bench + 1 &&
		Tile::workBench.getTexture(Facing::SOUTH, 0) == bench &&
		Tile::workBench.getTexture(Facing::EAST, 0) == bench &&
		Tile::workBench.getTexture(Facing::UP, 0) == bench - 16 &&
		Tile::workBench.getTexture(Facing::DOWN, 0) == Tile::wood.getTexture(Facing::DOWN),
		"crafting table decorates north and west");

	std::cout << "audit-containers textures: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F032 / A03: ChestTileEntity.java:44 and DispenserTileEntity.java:52 clamp to
// the container's own maximum of 64, never to the item's stack size.
bool containerCap()
{
	bool ok = true;
	ok &= expect(Items::bread->getMaxStackSize() == 1, "precondition: bread is a one-per-slot item");

	auto chest = std::make_shared<ChestTileEntity>();
	chest->setItem(0, ItemInstance(Items::stick->getShiftedIndex(), 65, 0));
	chest->setItem(1, ItemInstance(Items::bread->getShiftedIndex(), 2, 0));
	ok &= expect(chest->getItem(0).stackSize == 64, "chest setter clamps 65 to the container maximum");
	ok &= expect(chest->getItem(1).stackSize == 2, "chest setter does not apply the item's own one-per-slot limit");

	auto dispenser = std::make_shared<DispenserTileEntity>();
	dispenser->setItem(0, ItemInstance(Items::stick->getShiftedIndex(), 65, 0));
	dispenser->setItem(1, ItemInstance(Items::bread->getShiftedIndex(), 2, 0));
	ok &= expect(dispenser->getItem(0).stackSize == 64, "dispenser setter clamps 65 to the container maximum");
	ok &= expect(dispenser->getItem(1).stackSize == 2, "dispenser setter does not apply the item's own limit");

	std::cout << "audit-containers container cap: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

struct DropSummary
{
	int_t entities = 0;
	int_t items = 0;
};

DropSummary summarize(World &world)
{
	DropSummary summary;
	for (const auto &entity : world.entities)
	{
		EntityItem *item = dynamic_cast<EntityItem *>(entity.get());
		if (item == nullptr)
			continue;
		summary.entities++;
		summary.items += item->item.stackSize;
	}
	return summary;
}

// F033 / B25: ChestTile.java:144-167, FurnaceTile.java:150-175 and
// DispenserTile.java:175-198 all draw from the block instance's own Random and
// scale nextGaussian by 0.05f.
bool breakDrops()
{
	bool ok = true;

	struct Case
	{
		const char *name;
		int_t tileId;
		int_t slots;
	};
	const Case cases[] = {
		{"chest", Tile::chest.id, 27},
		{"dispenser", Tile::dispenser.id, 9},
		{"furnace", Tile::furnace.id, 3},
		{"lit furnace", Tile::furnaceLit.id, 3},
	};

	for (const Case &c : cases)
	{
		World world;
		world.setTile(0, 64, 0, c.tileId);
		auto container = std::dynamic_pointer_cast<IInventory>(world.getTileEntity(0, 64, 0));
		ok &= expect(container != nullptr, "break-drop case has its tile entity");
		if (container == nullptr)
			continue;

		int_t stored = 0;
		for (int_t slot = 0; slot < c.slots; ++slot)
		{
			container->setInventorySlotContents(slot, ItemInstance(Items::stick->getShiftedIndex(), 64, 0));
			stored += 64;
		}

		// Control: an untouched world RNG before the break, so a difference after
		// it can only come from the drop path.
		const ulong_t worldRngBefore = world.random.rawState();
		world.setTile(0, 64, 0, 0);
		const DropSummary summary = summarize(world);

		ok &= expect(world.random.rawState() == worldRngBefore,
			"breaking a container must not consume world RNG draws");
		ok &= expect(summary.items == stored, "every stored item reaches a dropped stack");
		ok &= expect(summary.entities >= c.slots, "each occupied slot produces at least one stack");
	}

	// Keeping the inventory across a lit/unlit swap must not drop anything, and
	// the two furnace blocks are separate instances with separate generators.
	{
		World world;
		world.setTile(0, 64, 0, Tile::furnace.id);
		auto furnace = std::dynamic_pointer_cast<IInventory>(world.getTileEntity(0, 64, 0));
		if (furnace != nullptr)
			furnace->setInventorySlotContents(0, ItemInstance(Items::coal->getShiftedIndex(), 8, 0));
		FurnaceTile::setLitState(true, world, 0, 64, 0);
		ok &= expect(summarize(world).entities == 0, "lighting a furnace keeps its contents");
		ok &= expect(world.getTile(0, 64, 0) == Tile::furnaceLit.id, "lighting swaps to the lit block");
	}

	std::cout << "audit-containers break drops: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

struct FireResult
{
	std::shared_ptr<Entity> spawned;
	int_t spawnedCount = 0;
};

FireResult fireOnce(World &world, Recorder &recorder, int_t facing, const ItemInstance &load)
{
	world.entities.clear();
	recorder.clear();

	world.raw(0, 64, 0, Tile::dispenser.id, facing);
	// A floor redstone torch under the block powers it from below (dir 0), which
	// is what DispenserTile::tick tests through hasNeighborSignal.
	world.raw(0, 63, 0, Tile::torchRedstoneActive.id, 5);
	auto dispenser = std::make_shared<DispenserTileEntity>();
	world.setTileEntity(0, 64, 0, dispenser);
	if (!load.isEmpty())
		dispenser->setItem(4, load);

	Tile::dispenser.tick(world, 0, 64, 0, world.random);

	FireResult result;
	for (const auto &entity : world.entities)
	{
		result.spawnedCount++;
		result.spawned = entity;
	}
	return result;
}

// F034 / B24: DispenserTile.java:88-137. Facing 2/3/4/5 map to (0,-1), (0,1),
// (-1,0), (1,0); the muzzle sits 0.6 out, and every branch reports through the
// world event channel.
bool dispenser()
{
	bool ok = true;
	World world;
	Recorder recorder;
	world.addListener(recorder);

	struct Dir
	{
		int_t facing;
		int_t dx;
		int_t dz;
	};
	const Dir dirs[] = {{2, 0, -1}, {3, 0, 1}, {4, -1, 0}, {5, 1, 0}};

	ok &= expect(world.hasNeighborSignal(0, 64, 0) || true, "");
	for (const Dir &dir : dirs)
	{
		const double px = 0.0 + dir.dx * 0.6 + 0.5;
		const double py = 64.0 + 0.5;
		const double pz = 0.0 + dir.dz * 0.6 + 0.5;
		const int_t smokeData = dir.dx + 1 + (dir.dz + 1) * 3;

		{
			const ulong_t before = world.random.rawState();
			FireResult fired = fireOnce(world, recorder, dir.facing, ItemInstance(Items::arrow->getShiftedIndex(), 1, 0));
			auto arrow = std::dynamic_pointer_cast<EntityArrow>(fired.spawned);
			ok &= expect(fired.spawnedCount == 1 && arrow != nullptr, "an arrow dispenses as EntityArrow");
			if (arrow != nullptr)
			{
				ok &= expect(near(arrow->x, px) && near(arrow->y, py) && near(arrow->z, pz),
					"arrow leaves from the 0.6 muzzle offset");
				ok &= expect(arrow->doesArrowBelongToPlayer, "dispensed arrow is flagged player-owned");
				ok &= expect(arrow->xd * dir.dx + arrow->zd * dir.dz > 0.0, "arrow travels along the facing");
			}
			ok &= expect(recorder.countEvents(1002) == 1 && recorder.countEvents(2000) == 1 &&
				recorder.countEvents(1000) == 0 && recorder.countEvents(1001) == 0,
				"arrow reports shot event 1002 then smoke 2000");
			ok &= expect(recorder.events.back().id == 2000 && recorder.events.back().data == smokeData,
				"smoke event carries the packed direction");
			ok &= expect(recorder.sounds.empty(), "the block plays no sound of its own");
			ok &= expect(world.random.rawState() == before,
				"projectile dispensing draws only from the tile entity RNG");
		}
		{
			FireResult fired = fireOnce(world, recorder, dir.facing, ItemInstance(Items::egg->getShiftedIndex(), 1, 0));
			ok &= expect(fired.spawnedCount == 1 && std::dynamic_pointer_cast<EntityThrownEgg>(fired.spawned) != nullptr,
				"an egg dispenses as EntityThrownEgg");
			ok &= expect(recorder.countEvents(1002) == 1 && recorder.countEvents(2000) == 1, "egg reports 1002 and 2000");
		}
		{
			FireResult fired = fireOnce(world, recorder, dir.facing, ItemInstance(Items::snowball->getShiftedIndex(), 1, 0));
			ok &= expect(fired.spawnedCount == 1 && std::dynamic_pointer_cast<EntitySnowball>(fired.spawned) != nullptr,
				"a snowball dispenses as EntitySnowball");
			ok &= expect(recorder.countEvents(1002) == 1 && recorder.countEvents(2000) == 1, "snowball reports 1002 and 2000");
		}
		{
			FireResult fired = fireOnce(world, recorder, dir.facing, ItemInstance(Items::stick->getShiftedIndex(), 1, 0));
			auto item = std::dynamic_pointer_cast<EntityItem>(fired.spawned);
			ok &= expect(fired.spawnedCount == 1 && item != nullptr, "an ordinary item dispenses as EntityItem");
			if (item != nullptr)
			{
				ok &= expect(near(item->x, px) && near(item->y, py - 0.3) && near(item->z, pz),
					"ordinary ejection drops 0.3 below the muzzle");
				const double along = item->xd * dir.dx + item->zd * dir.dz;
				// speed is nextDouble()*0.1 + 0.2 plus a Gaussian term of stddev 0.045
				ok &= expect(along > 0.0 && along < 0.6, "ordinary ejection speed is randomised around 0.2..0.3");
			}
			ok &= expect(recorder.countEvents(1000) == 1 && recorder.countEvents(2000) == 1 &&
				recorder.countEvents(1002) == 0,
				"ordinary ejection reports 1000 then 2000");
			ok &= expect(recorder.sounds.empty(), "ordinary ejection plays no direct sound");
		}
		{
			FireResult fired = fireOnce(world, recorder, dir.facing, ItemInstance());
			ok &= expect(fired.spawnedCount == 0, "an empty dispenser spawns nothing");
			ok &= expect(recorder.countEvents(1001) == 1 && recorder.countEvents(2000) == 0,
				"an empty dispenser reports only the 1001 click");
			ok &= expect(recorder.sounds.empty(), "empty dispenser plays no direct sound");
		}
	}

	// Slot selection is reservoir sampling over the tile entity's own generator,
	// so a full dispenser still leaves the world RNG untouched on a projectile.
	{
		world.entities.clear();
		recorder.clear();
		world.raw(0, 64, 0, Tile::dispenser.id, 3);
		world.raw(0, 63, 0, Tile::torchRedstoneActive.id, 5);
		auto te = std::make_shared<DispenserTileEntity>();
		world.setTileEntity(0, 64, 0, te);
		for (int_t slot = 0; slot < te->getContainerSize(); ++slot)
			te->setItem(slot, ItemInstance(Items::arrow->getShiftedIndex(), 1, 0));
		const ulong_t before = world.random.rawState();
		std::vector<bool> emptied(static_cast<std::size_t>(te->getContainerSize()), false);
		for (int_t shot = 0; shot < te->getContainerSize(); ++shot)
			Tile::dispenser.tick(world, 0, 64, 0, world.random);
		int_t left = 0;
		for (int_t slot = 0; slot < te->getContainerSize(); ++slot)
			if (!te->getItem(slot).isEmpty())
				left++;
		ok &= expect(left == 0, "nine dispenses empty all nine slots exactly once each");
		ok &= expect(world.random.rawState() == before, "slot selection never touches the world RNG");
		ok &= expect(static_cast<int_t>(world.entities.size()) == te->getContainerSize(), "each dispense fires one arrow");
	}

	world.removeListener(recorder);
	std::cout << "audit-containers dispenser: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F036: RecordPlayerTile.java:42-43 emits worldEvent 1005 with data 0 and then
// clears the local stream, before dropping the disc with a 10-tick pickup delay.
bool jukebox()
{
	bool ok = true;
	World world;
	Recorder recorder;
	world.addListener(recorder);

	world.setTile(0, 64, 0, Tile::jukebox.id);
	const int_t discId = Items::record13->getShiftedIndex();
	Tile::jukebox.insertRecord(world, 0, 64, 0, discId);
	auto player = std::dynamic_pointer_cast<RecordPlayerTileEntity>(world.getTileEntity(0, 64, 0));
	ok &= expect(player != nullptr && player->record == discId && world.getData(0, 64, 0) == 1,
		"inserting a record arms the jukebox");

	recorder.clear();
	Tile::jukebox.ejectRecord(world, 0, 64, 0);
	ok &= expect(recorder.countEvents(1005) == 1, "ejection emits the stop event");
	ok &= expect(!recorder.events.empty() && recorder.events.front().id == 1005 &&
		recorder.events.front().data == 0 && recorder.events.front().x == 0 &&
		recorder.events.front().y == 64 && recorder.events.front().z == 0,
		"stop event carries data 0 at the jukebox position");
	ok &= expect(recorder.streamCount == 1 && recorder.streams.front().empty(),
		"ejection also clears this client's stream exactly once");
	ok &= expect(player != nullptr && player->record == 0 && world.getData(0, 64, 0) == 0,
		"ejection clears the record and metadata");

	int_t discs = 0;
	for (const auto &entity : world.entities)
	{
		EntityItem *item = dynamic_cast<EntityItem *>(entity.get());
		if (item == nullptr)
			continue;
		discs++;
		ok &= expect(item->item.itemID == discId && item->item.stackSize == 1 && item->throwTime == 10,
			"the ejected disc keeps its identity and pickup delay");
		ok &= expect(item->y > 64.6 && item->y < 65.4, "the disc is thrown from above the block centre");
	}
	ok &= expect(discs == 1, "ejection drops exactly one disc");

	recorder.clear();
	Tile::jukebox.ejectRecord(world, 0, 64, 0);
	ok &= expect(recorder.events.empty() && recorder.streamCount == 0, "an empty jukebox ejects nothing");

	world.removeListener(recorder);
	std::cout << "audit-containers jukebox: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F037 / B27: MusicTileEntity.java:50 raises a block event, MusicTile.java:52-67
// turns it into one sound and one particle, and MusicTile.java:37 refuses to act
// on a left click while online.
bool noteBlock()
{
	bool ok = true;
	World world;
	Recorder recorder;
	world.addListener(recorder);

	world.setTile(0, 64, 0, Tile::noteBlock.id);
	auto note = std::dynamic_pointer_cast<NoteTileEntity>(world.getTileEntity(0, 64, 0));
	ok &= expect(note != nullptr, "note block places its tile entity");
	if (note == nullptr)
		return false;

	struct Instrument
	{
		int_t below;
		const char16_t *sound;
	};
	const Instrument instruments[] = {
		{0, u"note.harp"},
		{Tile::rock.id, u"note.bd"},
		{Tile::sand.id, u"note.snare"},
		{Tile::glass.id, u"note.hat"},
		{Tile::wood.id, u"note.bassattack"},
	};

	for (const Instrument &instrument : instruments)
	{
		world.raw(0, 63, 0, instrument.below);
		for (int_t pitch = 0; pitch <= 24; ++pitch)
		{
			note->note = static_cast<byte_t>(pitch);
			recorder.clear();
			note->triggerNote(world, 0, 64, 0);
			const float expectedPitch = static_cast<float>(std::pow(2.0, (pitch - 12) / 12.0));
			ok &= expect(recorder.sounds.size() == 1 && recorder.particles.size() == 1,
				"one trigger produces exactly one sound and one particle");
			if (recorder.sounds.size() != 1 || recorder.particles.size() != 1)
				break;
			const Sound &sound = recorder.sounds.front();
			const Particle &particle = recorder.particles.front();
			ok &= expect(sound.name == jstring(instrument.sound) && sound.volume == 3.0f && sound.pitch == expectedPitch,
				"the block below picks the instrument and the note picks the pitch");
			ok &= expect(near(sound.x, 0.5) && near(sound.y, 64.5) && near(sound.z, 0.5),
				"the note sounds at the block centre");
			ok &= expect(particle.name == u"note" && near(particle.y, 65.2) && near(particle.xa, pitch / 24.0),
				"the note particle rises above the block and carries the note colour");
		}
	}

	// A block on top silences the trigger before it reaches the event channel.
	world.raw(0, 63, 0, Tile::rock.id);
	world.raw(0, 65, 0, Tile::rock.id);
	recorder.clear();
	note->triggerNote(world, 0, 64, 0);
	ok &= expect(recorder.sounds.empty() && recorder.particles.empty(), "a covered note block stays silent");
	world.raw(0, 65, 0, 0);

	// The received-packet path: playNoteAt is what NetClientHandler calls for
	// packet 54, and it must reach the same handler.
	recorder.clear();
	world.playNoteAt(0, 64, 0, 2, 7);
	ok &= expect(recorder.sounds.size() == 1 && recorder.sounds.front().name == u"note.snare" &&
		recorder.particles.size() == 1,
		"a received block event plays the note without a tile entity lookup");

	// Left click is server-authoritative.
	auto clicker = std::make_shared<Player>(world);
	recorder.clear();
	world.isOnline = true;
	Tile::noteBlock.attack(world, 0, 64, 0, *clicker);
	ok &= expect(recorder.sounds.empty() && recorder.particles.empty(),
		"an online left click does not play locally");
	world.isOnline = false;
	Tile::noteBlock.attack(world, 0, 64, 0, *clicker);
	ok &= expect(recorder.sounds.size() == 1, "an offline left click plays once");

	world.removeListener(recorder);
	std::cout << "audit-containers note block: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

// F038 / B37: MobSpawnerTileEntity.java:37 gates the countdown and the spawn
// attempts behind !isOnline, and the nearby cap counts assignable classes.
bool spawner()
{
	bool ok = true;

	auto build = [](World &world, const jstring &entityId, int_t delay) {
		world.setTile(0, 64, 0, Tile::mobSpawner.id);
		auto spawner = std::dynamic_pointer_cast<MobSpawnerTileEntity>(world.getTileEntity(0, 64, 0));
		if (spawner != nullptr)
		{
			spawner->setEntityId(entityId);
			spawner->spawnDelay = delay;
		}
		auto player = std::make_shared<Player>(world);
		player->moveTo(0.5, 65.0, 0.5, 0.0f, 0.0f);
		world.players.push_back(player);
		return spawner;
	};

	{
		World world;
		Recorder recorder;
		world.addListener(recorder);
		auto spawner = build(world, u"Pig", 5);
		ok &= expect(spawner != nullptr, "mob spawner places its tile entity");
		if (spawner == nullptr)
			return false;
		const double spinBefore = spawner->spin;
		spawner->tick();
		ok &= expect(spawner->spawnDelay == 4, "an offline spawner counts down");
		ok &= expect(spawner->spin > spinBefore, "the spinning mob keeps turning");
		ok &= expect(recorder.particles.size() == 2, "one smoke and one flame per tick");
		world.removeListener(recorder);
	}
	{
		World world;
		Recorder recorder;
		world.addListener(recorder);
		auto spawner = build(world, u"Pig", 0);
		if (spawner == nullptr)
			return false;
		world.isOnline = true;
		const double spinBefore = spawner->spin;
		for (int_t i = 0; i < 40; ++i)
			spawner->tick();
		ok &= expect(spawner->spawnDelay == 0, "an online spawner never advances its own countdown");
		ok &= expect(world.entities.empty(), "an online spawner never creates a local mob");
		ok &= expect(spawner->spin > spinBefore && recorder.particles.size() == 80,
			"an online spawner keeps its animation");
		world.removeListener(recorder);
	}
	{
		// Six PigZombies must satisfy a Zombie spawner's cap, because Java asks
		// Class.isAssignableFrom rather than comparing exact classes.
		World world;
		auto spawner = build(world, u"Zombie", 0);
		if (spawner == nullptr)
			return false;
		for (int_t i = 0; i < 6; ++i)
		{
			auto pigZombie = std::make_shared<PigZombie>(world);
			pigZombie->moveTo(0.5 + i * 0.1, 65.0, 0.5, 0.0f, 0.0f);
			world.addEntity(pigZombie);
		}
		spawner->tick();
		ok &= expect(spawner->spawnDelay >= 200 && spawner->spawnDelay < 800,
			"the cap counts PigZombie against a Zombie spawner and resets the delay");
		int_t plainZombies = 0;
		for (const auto &entity : world.entities)
			if (typeid(*entity) == typeid(Zombie))
				plainZombies++;
		ok &= expect(plainZombies == 0, "the capped spawner adds no mob");
	}
	for (bool giants : {false, true})
	{
		World world;
		auto spawner = build(world, u"Zombie", 0);
		if (spawner == nullptr)
			return false;
		const int_t nearby = giants ? 6 : 5;
		for (int_t i = 0; i < nearby; ++i)
		{
			std::shared_ptr<Entity> mob;
			if (giants) mob = std::make_shared<Giant>(world);
			else mob = std::make_shared<PigZombie>(world);
			mob->moveTo(0.5 + i * 0.1, 65.0, 0.5, 0.0f, 0.0f);
			world.addEntity(mob);
		}
		// Bright cells reject every attempted Zombie spawn, independently of RNG.
		for (int_t x = -4; x <= 4; ++x)
			for (int_t y = 62; y <= 67; ++y)
				for (int_t z = -4; z <= 4; ++z)
					world.setBrightness(1, x, y, z, 15);
		spawner->tick();
		ok &= expect(spawner->spawnDelay == 0 && world.entities.size() == static_cast<std::size_t>(nearby),
			giants ? "Giants do not count toward Java's Zombie spawner cap" : "five PigZombies stay below the spawner cap");
	}

	std::cout << "audit-containers spawner: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}

bool runAuditContainersCases()
{
	bool ok = AuditContainers::chestTextures();
	ok &= AuditContainers::containerCap();
	ok &= AuditContainers::breakDrops();
	ok &= AuditContainers::dispenser();
	ok &= AuditContainers::jukebox();
	ok &= AuditContainers::noteBlock();
	ok &= AuditContainers::spawner();
	std::cout << "audit-containers: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}
