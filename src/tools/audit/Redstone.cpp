// Torch burnout, repeater timing and wire notification regressions.

#include "world/level/tile/StoneTile.h"
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "java/Random.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/TilePos.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/NotGateTile.h"
#include "world/level/tile/RedStoneDustTile.h"
#include "world/level/tile/RepeaterTile.h"
#include "world/level/tile/Tile.h"

namespace AuditRedstone
{
namespace Detail
{

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "audit-redstone FAILED: " << message << '\n';
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
	jstring gatherStats() override { return u"audit-redstone"; }
};

// Records listener-visible effects in arrival order so a case can assert the
// relative order of a state write and its burnout sound, plus every scheduled
// block update through the virtual schedule hook.
struct Event
{
	int kind; // 0 = tileChanged, 1 = playSound, 2 = addParticle
	int_t x, y, z;
	jstring name;
	float pitch;
	double px, py, pz;
};

struct Scheduled
{
	int_t x, y, z, tileId, delay;
};

class Recorder : public LevelListener
{
public:
	std::vector<Event> events;
	std::vector<double> reddustX;

	void tileChanged(int_t x, int_t y, int_t z) override
	{
		events.push_back({0, x, y, z, u"", 0.0f, 0.0, 0.0, 0.0});
	}
	void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
	void allChanged() override {}
	void playSound(const jstring &name, double x, double y, double z, float, float pitch) override
	{
		events.push_back({1, (int_t)x, (int_t)y, (int_t)z, name, pitch, x, y, z});
	}
	void addParticle(const jstring &name, double x, double y, double z, double, double, double) override
	{
		events.push_back({2, (int_t)x, (int_t)y, (int_t)z, name, 0.0f, x, y, z});
		if (name == u"reddust")
			reddustX.push_back(x);
	}
	void playMusic(const jstring &, double, double, double, float) override {}
	void entityAdded(std::shared_ptr<Entity>) override {}
	void entityRemoved(std::shared_ptr<Entity>) override {}
	void skyColorChanged() override {}
	void playStreamingMusic(const jstring &, int_t, int_t, int_t) override {}
	void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
	void levelEvent(Player *, int_t, int_t, int_t, int_t, int_t) override {}

	void clear()
	{
		events.clear();
		reddustX.clear();
	}

	int countSound(const jstring &name) const
	{
		int n = 0;
		for (const Event &e : events)
			if (e.kind == 1 && e.name == name)
				n++;
		return n;
	}

	int countParticle(const jstring &name) const
	{
		int n = 0;
		for (const Event &e : events)
			if (e.kind == 2 && e.name == name)
				n++;
		return n;
	}

	int firstTileChanged() const
	{
		for (size_t i = 0; i < events.size(); i++)
			if (events[i].kind == 0)
				return (int)i;
		return -1;
	}

	int firstSound(const jstring &name) const
	{
		for (size_t i = 0; i < events.size(); i++)
			if (events[i].kind == 1 && events[i].name == name)
				return (int)i;
		return -1;
	}
};

class World : public Level
{
public:
	std::shared_ptr<Source> source = std::make_shared<Source>();
	std::vector<Scheduled> scheduled;

	World() : Level(u"audit-redstone", Dimension::Id_Normal, 1234567, false)
	{
		setChunkSource(source);
		for (int_t cx = -1; cx <= 2; ++cx)
			for (int_t cz = -1; cz <= 2; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
	}

	void scheduleBlockUpdate(int_t x, int_t y, int_t z, int_t tileId, int_t delay) override
	{
		scheduled.push_back({x, y, z, tileId, delay});
		Level::scheduleBlockUpdate(x, y, z, tileId, delay);
	}

	// Raw placement: no onPlace, no notification, so a case starts from an
	// exact world state and only the method under test produces callbacks.
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

	int countScheduled(int_t x, int_t y, int_t z) const
	{
		int n = 0;
		for (const Scheduled &s : scheduled)
			if (s.x == x && s.y == y && s.z == z)
				n++;
		return n;
	}
};

// F044/B19. Eight rapid turn-offs of the same torch burn it out: the
// off-state write lands before the fizz, the pitch uses the 2.6 base with two
// level.random draws, and the five smokes draw from the tick random.
bool torchBurnoutOrderAndRng()
{
	World level;
	Recorder rec;
	level.addListener(rec);
	level.floor(23, 23, 25, 25, 61);
	// The lower torch powers the solid block supporting the subject.
	level.put(24, 62, 24, Tile::torchRedstoneActive.id, 5);
	level.put(24, 63, 24, Tile::rock.id, 0);
	level.put(24, 64, 24, Tile::torchRedstoneActive.id, 5);
	level.time = 50000;
	level.random.setSeed(4242);
	Random tickRand(777);

	bool ok = true;
	for (int i = 0; i < 7; i++)
	{
		Tile::torchRedstoneActive.tick(level, 24, 64, 24, tickRand);
		ok &= expect(level.getTile(24, 64, 24) == Tile::torchRedstoneIdle.id,
			"torch turns off while its attachment is powered");
		ok &= expect(rec.countSound(u"random.fizz") == 0,
			"no burnout fizz before the eighth rapid toggle");
		level.put(24, 64, 24, Tile::torchRedstoneActive.id, 5);
	}

	rec.clear();
	Tile::torchRedstoneActive.tick(level, 24, 64, 24, tickRand);
	ok &= expect(level.getTile(24, 64, 24) == Tile::torchRedstoneIdle.id,
		"burnout tick still writes the idle state");
	ok &= expect(rec.countSound(u"random.fizz") == 1,
		"eighth rapid toggle burns out with one fizz");
	ok &= expect(rec.countParticle(u"smoke") == 5,
		"burnout emits five smoke particles");

	int changedAt = rec.firstTileChanged();
	int fizzAt = rec.firstSound(u"random.fizz");
	ok &= expect(changedAt >= 0 && fizzAt >= 0 && changedAt < fizzAt,
		"off-state write precedes the burnout sound");

	Random probe(4242);
	float fa = probe.nextFloat();
	float fb = probe.nextFloat();
	float wantPitch = 2.6f + (fa - fb) * 0.8f;
	float gotPitch = 0.0f;
	for (const Event &e : rec.events)
		if (e.kind == 1 && e.name == u"random.fizz")
			gotPitch = e.pitch;
	ok &= expect(gotPitch == wantPitch,
		"burnout pitch is 2.6 plus two level.random draws");
	ok &= expect(probe.nextFloat() == level.random.nextFloat(),
		"burnout sound consumes exactly two level.random draws");

	Random smokeProbe(777);
	for (int i = 0; i < 15; i++)
		smokeProbe.nextDouble();
	ok &= expect(tickRand.nextDouble() == smokeProbe.nextDouble(),
		"burnout smoke consumes fifteen tick-random doubles");

	bool smokeInside = true;
	for (const Event &e : rec.events)
		if (e.kind == 2 && e.name == u"smoke")
			smokeInside &= e.px >= 24.2 && e.px <= 24.8 && e.py >= 64.2 && e.py <= 64.8 && e.pz >= 24.2 && e.pz <= 24.8;
	ok &= expect(smokeInside, "burnout smoke stays in the inner cube");

	level.removeListener(rec);
	return ok;
}

// F044/B19. The display particle jitters with the tick random instead of
// sitting on the fixed base coordinate.
bool torchAnimateTickJitter()
{
	World level;
	Recorder rec;
	level.addListener(rec);
	level.floor(0, 0, 2, 2, 63);
	level.put(1, 64, 1, Tile::torchRedstoneActive.id, 5);

	for (int seed : {111, 222, 333})
	{
		Random r(seed);
		Tile::torchRedstoneActive.animateTick(level, 1, 64, 1, r);
	}
	bool ok = expect(rec.countParticle(u"reddust") == 3,
		"one display particle per animateTick");
	bool spread = false;
	for (size_t i = 1; i < rec.reddustX.size(); i++)
		if (rec.reddustX[i] != rec.reddustX[0])
			spread = true;
	ok &= expect(spread, "display particle position follows the tick random");

	Random r(98765);
	Tile::torchRedstoneActive.animateTick(level, 1, 64, 1, r);
	Random ref(98765);
	ref.nextFloat();
	ref.nextFloat();
	ref.nextFloat();
	ok &= expect(r.nextFloat() == ref.nextFloat(),
		"animateTick consumes exactly three jitter draws");

	// An idle torch emits nothing.
	rec.clear();
	Random idle(5);
	Tile::torchRedstoneIdle.animateTick(level, 1, 64, 1, idle);
	ok &= expect(rec.events.empty(), "idle torch has no display particle");

	level.removeListener(rec);
	return ok;
}

// B19. The top face of both torch states shows the wire texture.
bool torchTopTexture()
{
	bool ok = expect(Tile::torchRedstoneActive.getTexture(Facing::UP, 0) == Tile::redstoneWire.getTexture(Facing::UP, 0),
		"active torch top face delegates to wire texture");
	ok &= expect(Tile::torchRedstoneIdle.getTexture(Facing::UP, 0) == Tile::redstoneWire.getTexture(Facing::UP, 0),
		"idle torch top face delegates to wire texture");
	return ok;
}

// F045/B20. A pulse shorter than the repeater delay still produces an output:
// the scheduled activation turns the repeater on even though the input is
// already gone, and banks the matching off tick.
bool repeaterShortPulse()
{
	World level;
	level.floor(0, 0, 2, 3, 63);
	level.time = 100;
	level.put(1, 64, 1, Tile::repeaterIdle.id, 0);
	level.put(1, 64, 2, Tile::redstoneWire.id, 15);
	Tile::repeaterIdle.neighborChanged(level, 1, 64, 1, Tile::redstoneWire.id);
	level.put(1, 64, 2, 0, 0);

	level.time = 101;
	level.tickPendingTicks(false);
	bool ok = expect(level.getTile(1, 64, 1) == Tile::repeaterIdle.id,
		"short pulse waits for its configured activation delay");
	level.time = 102;
	level.tickPendingTicks(false);
	ok &= expect(level.getTile(1, 64, 1) == Tile::repeaterActive.id,
		"scheduled activation turns on despite vanished input");
	level.time = 103;
	level.tickPendingTicks(false);
	ok &= expect(level.getTile(1, 64, 1) == Tile::repeaterActive.id,
		"short-pulse output persists until its off delay");
	level.time = 104;
	level.tickPendingTicks(false);
	ok &= expect(level.getTile(1, 64, 1) == Tile::repeaterIdle.id,
		"scheduled off tick ends the short pulse");

	// A real source keeps the input powered during neighbor callbacks.
	level.put(1, 64, 2, Tile::torchRedstoneActive.id, 5);
	level.scheduled.clear();
	Random r(3);
	Tile::repeaterIdle.tick(level, 1, 64, 1, r);
	ok &= expect(level.getTile(1, 64, 1) == Tile::repeaterActive.id && level.countScheduled(1, 64, 1) == 0,
		"sustained input activates without scheduling an off tick");
	return ok;
}

// F045. All four delay settings schedule their reference delays.
bool repeaterDelays()
{
	World level;
	level.floor(4, 0, 6, 3, 63);
	const int want[4] = {2, 4, 6, 8};
	bool ok = true;
	for (int setting = 0; setting < 4; setting++)
	{
		int_t data = (int_t)(setting << 2);
		level.put(5, 64, 1, Tile::repeaterIdle.id, data);
		level.put(5, 64, 2, Tile::redstoneWire.id, 15);
		level.scheduled.clear();
		Tile::repeaterIdle.neighborChanged(level, 5, 64, 1, Tile::redstoneWire.id);
		ok &= expect(level.scheduled.size() == 1 && level.scheduled[0].delay == want[setting],
			"repeater delay setting schedules its reference ticks");
	}
	return ok;
}

// B20 active branch plus B21 faces.
bool repeaterActiveAndFaces()
{
	World level;
	level.floor(7, 0, 9, 3, 63);
	Random r(11);
	level.put(8, 64, 1, Tile::repeaterActive.id, 0);
	level.put(8, 64, 2, Tile::redstoneWire.id, 15);
	Tile::repeaterActive.tick(level, 8, 64, 1, r);
	bool ok = expect(level.getTile(8, 64, 1) == Tile::repeaterActive.id,
		"powered active repeater stays on");
	level.put(8, 64, 2, Tile::redstoneWire.id, 0);
	Tile::repeaterActive.tick(level, 8, 64, 1, r);
	ok &= expect(level.getTile(8, 64, 1) == Tile::repeaterIdle.id,
		"unpowered active repeater turns off");

	// B21: horizontal faces always render even against opaque rock, top and
	// bottom never do.
	level.put(7, 64, 1, Tile::rock.id, 0);
	level.put(9, 64, 1, Tile::rock.id, 0);
	level.put(8, 64, 0, Tile::rock.id, 0);
	level.put(8, 64, 2, Tile::rock.id, 0);
	ok &= expect(Tile::repeaterIdle.shouldRenderFace(level, 8, 64, 1, Facing::NORTH),
		"repeater north face renders against opaque rock");
	ok &= expect(Tile::repeaterIdle.shouldRenderFace(level, 8, 64, 1, Facing::SOUTH),
		"repeater south face renders against opaque rock");
	ok &= expect(Tile::repeaterIdle.shouldRenderFace(level, 8, 64, 1, Facing::WEST),
		"repeater west face renders against opaque rock");
	ok &= expect(Tile::repeaterIdle.shouldRenderFace(level, 8, 64, 1, Facing::EAST),
		"repeater east face renders against opaque rock");
	ok &= expect(!Tile::repeaterIdle.shouldRenderFace(level, 8, 64, 1, Facing::DOWN),
		"repeater bottom face stays hidden");
	ok &= expect(!Tile::repeaterIdle.shouldRenderFace(level, 8, 64, 1, Facing::UP),
		"repeater top face stays hidden");
	return ok;
}

// B06 admission for the owned tiles: wire needs normal-cube support, the
// repeater adds the replaceable-target check on top.
bool placementAdmission()
{
	World level;
	level.floor(11, 9, 13, 11, 63);
	bool ok = expect(Tile::redstoneWire.mayPlace(level, 12, 64, 10),
		"wire admits placement on a normal cube");
	ok &= expect(!Tile::redstoneWire.mayPlace(level, 20, 64, 20),
		"wire refuses floating placement");
	ok &= expect(Tile::repeaterIdle.mayPlace(level, 12, 64, 10),
		"repeater admits placement on a normal cube");
	ok &= expect(!Tile::repeaterIdle.mayPlace(level, 20, 64, 20),
		"repeater refuses floating placement");
	level.put(12, 64, 10, Tile::rock.id, 0);
	ok &= expect(!Tile::repeaterIdle.mayPlace(level, 12, 64, 10),
		"repeater refuses an occupied non-replaceable cell");
	return ok;
}

// F043/B22. On an online level the wire callbacks leave authoritative state
// alone; offline the same calls recompute.
bool wireOnlineGuards()
{
	World level;
	Recorder rec;
	level.addListener(rec);
	level.floor(19, 19, 22, 22, 63);
	level.put(20, 64, 20, Tile::redstoneWire.id, 7);

	level.isOnline = true;
	Tile::redstoneWire.onPlace(level, 20, 64, 20);
	bool ok = expect(level.getData(20, 64, 20) == 7,
		"online onPlace keeps server wire strength");
	Tile::redstoneWire.onRemove(level, 20, 64, 20);
	ok &= expect(level.getData(20, 64, 20) == 7,
		"online onRemove keeps server wire strength");
	// Support pulled away: the client must not drop the wire itself.
	level.put(20, 63, 20, 0, 0);
	Tile::redstoneWire.neighborChanged(level, 20, 64, 20, Tile::rock.id);
	ok &= expect(level.getTile(20, 64, 20) == Tile::redstoneWire.id && level.getData(20, 64, 20) == 7,
		"online neighbor change neither drops nor recomputes wire");
	ok &= expect(level.scheduled.empty(), "online callbacks schedule nothing");
	level.isOnline = false;

	level.put(21, 64, 21, Tile::redstoneWire.id, 7);
	Tile::redstoneWire.onPlace(level, 21, 64, 21);
	ok &= expect(level.getData(21, 64, 21) == 0,
		"offline onPlace recomputes stale strength");

	level.put(22, 64, 22, Tile::redstoneWire.id, 7);
	Tile::redstoneWire.neighborChanged(level, 22, 64, 22, Tile::rock.id);
	ok &= expect(level.getTile(22, 64, 22) == Tile::redstoneWire.id && level.getData(22, 64, 22) == 0,
		"offline neighbor change recomputes but keeps supported wire");

	level.removeListener(rec);
	return ok;
}

// The two neighboring wire centers each notify the idle torch once.
// Repeated centers would deliver additional callbacks to that torch.
bool wireDedupDispatch()
{
	World level;
	level.floor(0, 3, 4, 6, 63);
	level.put(1, 64, 4, Tile::redstoneWire.id, 1);
	level.put(1, 64, 5, Tile::redstoneWire.id, 1);
	level.put(2, 64, 5, Tile::redstoneWire.id, 1);
	level.put(2, 64, 4, Tile::torchRedstoneIdle.id, 5);

	Tile::redstoneWire.neighborChanged(level, 1, 64, 4, Tile::redstoneWire.id);
	bool ok = expect(level.getData(1, 64, 4) == 0 && level.getData(1, 64, 5) == 0 && level.getData(2, 64, 5) == 0,
		"notch wires all drain in one update");
	ok &= expect(level.getTile(2, 64, 4) == Tile::torchRedstoneIdle.id,
		"notified torch survives on its support");
	ok &= expect(level.countScheduled(2, 64, 4) == 2,
		"each unique neighboring center notifies the torch once");
	return ok;
}

// B23. A torch-fed wire line reaches the reference decay, and a uniform line
// with no feed drains fully in one update.
bool wirePropagationLevels()
{
	World level;
	level.floor(9, 9, 15, 11, 63);
	level.put(10, 64, 11, Tile::torchRedstoneActive.id, 5);
	for (int_t x = 10; x <= 14; x++)
		level.put(x, 64, 10, Tile::redstoneWire.id, 0);

	Tile::redstoneWire.neighborChanged(level, 10, 64, 10, Tile::torchRedstoneActive.id);
	bool ok = expect(level.getData(10, 64, 10) == 15, "head wire holds full strength");
	ok &= expect(level.getData(11, 64, 10) == 14, "wire decays by one per step");
	ok &= expect(level.getData(12, 64, 10) == 13, "wire decay continues down the line");
	ok &= expect(level.getData(13, 64, 10) == 12, "wire decay reaches the fourth cell");
	ok &= expect(level.getData(14, 64, 10) == 11, "wire decay reaches the fifth cell");

	level.put(10, 64, 11, 0, 0);
	for (int_t x = 10; x <= 14; x++)
		level.put(x, 64, 10, Tile::redstoneWire.id, 1);
	Tile::redstoneWire.neighborChanged(level, 10, 64, 10, Tile::torchRedstoneActive.id);
	bool drained = true;
	for (int_t x = 10; x <= 14; x++)
		drained &= level.getData(x, 64, 10) == 0;
	ok &= expect(drained, "feedless uniform line drains fully in one update");
	return ok;
}

// F047/B23. The production deferred set dedups, snapshots before clearing so
// reentrant work lands in a fresh set, and iterates every member.
bool deferredSetSemantics()
{
	JavaTilePosSet set;
	bool ok = expect(set.emplace(3, 64, 7), "first insert reports new");
	ok &= expect(!set.emplace(3, 64, 7), "duplicate insert reports existing");
	for (int i = 0; i < 40; i++)
		set.emplace(i, 64, -i);
	ok &= expect(set.size() == 41, "distinct positions accumulate across resizes");

	std::vector<TilePos> snapshot(set.begin(), set.end());
	set.clear();
	ok &= expect(set.size() == 0, "clear empties the set for reentrant work");
	ok &= expect(snapshot.size() == 41, "snapshot keeps every queued position");

	set.emplace(9, 64, 9);
	ok &= expect(set.size() == 1, "post-clear inserts start a fresh generation");

	bool found = false;
	for (const TilePos &p : snapshot)
		if (p.x == 3 && p.y == 64 && p.z == 7)
			found = true;
	ok &= expect(found, "snapshot iteration covers every member");
	return ok;
}

bool run()
{
	Tile::initTiles();
	bool ok = torchBurnoutOrderAndRng();
	ok &= torchAnimateTickJitter();
	ok &= torchTopTexture();
	ok &= repeaterShortPulse();
	ok &= repeaterDelays();
	ok &= repeaterActiveAndFaces();
	ok &= placementAdmission();
	ok &= wireOnlineGuards();
	ok &= wireDedupDispatch();
	ok &= wirePropagationLevels();
	ok &= deferredSetSemantics();
	std::cout << "audit-redstone cases: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

}
}

bool runAuditRedstoneCases()
{
	return AuditRedstone::Detail::run();
}
