#include "tools/RegionIoSmoke.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "java/File.h"
#include "java/String.h"
#include "java/Type.h"
#include "world/level/Level.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/chunk/storage/McRegionChunkStorage.h"
#include "world/level/chunk/storage/RegionFile.h"
#include "world/level/chunk/storage/RegionFileCache.h"

#include "zlib.h"

struct RegionIoSmokeLevel : public Level
{
	RegionIoSmokeLevel()
		: Level(File::open(u"build/region-io-smoke/saves"), u"region-io-smoke", 424242LL, 0)
	{
	}
};

static bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "FAILED: " << message << '\n';
	return condition;
}

static void removeDirectory(File &directory)
{
	if (!directory.exists())
		return;
	auto children = directory.listFiles();
	Level::deleteRecursive(children);
	directory.remove();
}

// Compress a payload exactly the way McRegionChunkStorage's worker does before
// handing it to RegionFile::writeChunkData.
static std::vector<byte_t> zlibCompress(const std::string &payload)
{
	uLongf bound = compressBound(static_cast<uLong>(payload.size()));
	std::vector<byte_t> compressed(bound);
	uLongf compSize = bound;
	if (compress2(reinterpret_cast<Bytef *>(compressed.data()), &compSize,
	              reinterpret_cast<const Bytef *>(payload.data()), static_cast<uLong>(payload.size()),
	              Z_DEFAULT_COMPRESSION) != Z_OK)
		return {};
	compressed.resize(compSize);
	return compressed;
}

static bool writePayload(RegionFile &region, int_t x, int_t z, const std::string &payload)
{
	std::vector<byte_t> compressed = zlibCompress(payload);
	if (compressed.empty())
		return false;
	region.writeChunkData(x, z, compressed.data(), static_cast<int_t>(compressed.size()));
	return true;
}

static bool readMatches(RegionFile &region, int_t x, int_t z, const std::string &payload)
{
	std::vector<byte_t> data = region.getChunkData(x, z);
	return data.size() == payload.size() &&
	       std::string(reinterpret_cast<const char *>(data.data()), data.size()) == payload;
}

static std::shared_ptr<LevelChunk> makePatternChunk(Level &level, int_t x, int_t z, int_t salt)
{
	std::array<ubyte_t, 16 * 128 * 16> blocks;
	for (size_t i = 0; i < blocks.size(); ++i)
		blocks[i] = static_cast<ubyte_t>((i * 31 + salt) % 5);
	auto chunk = std::make_shared<LevelChunk>(level, blocks.data(), x, z);
	chunk->recalcHeightmap();
	return chunk;
}

static bool runRegionFileCases(bool &ok)
{
	std::unique_ptr<File> root(File::open(u"build/region-io-smoke/regions"));
	removeDirectory(*root);
	root->mkdirs();
	std::string baseDir = String::toUTF8(root->toString());

	// Roundtrip through write compression and the reusable decompression path.
	std::shared_ptr<RegionFile> held = RegionFileCache::getRegionFile(baseDir, 0, 0);
	std::string small = "region-io-smoke small payload";
	ok &= expect(writePayload(*held, 0, 0, small), "small payload should compress");
	ok &= expect(readMatches(*held, 0, 0, small), "small payload should roundtrip");

	// Large payload: decompressed size far exceeds both the 16 KiB floor and
	// 4x the compressed size, forcing the geometric growth path.
	std::string large;
	large.reserve(300000);
	for (int i = 0; i < 300000; ++i)
		large.push_back(static_cast<char>('A' + (i % 7)));
	ok &= expect(writePayload(*held, 1, 0, large), "large payload should compress");
	ok &= expect(readMatches(*held, 1, 0, large), "large payload should roundtrip");

	// Repeated reads reuse the per-file scratch buffer; bytes must not change.
	ok &= expect(readMatches(*held, 1, 0, large), "second read should match after scratch reuse");
	ok &= expect(readMatches(*held, 0, 0, small), "small payload should survive a larger read in between");

	// An unheld region: written, released, then subject to eviction pressure.
	std::string other = "unheld region payload";
	{
		std::shared_ptr<RegionFile> unheld = RegionFileCache::getRegionFile(baseDir, -32, 0);
		ok &= expect(writePayload(*unheld, 0, 0, other), "unheld region payload should compress");
	}

	// Capacity pressure: touch more distinct regions than MAX_CACHE_SIZE (256).
	// Region coordinates step by 32 chunks.
	for (int i = 1; i <= 300; ++i)
		RegionFileCache::getRegionFile(baseDir, i * 32, 32);

	// The externally held entry must never be evicted: a lookup under pressure
	// returns the very same open object, and writes through both agree.
	std::shared_ptr<RegionFile> lookedUp = RegionFileCache::getRegionFile(baseDir, 0, 0);
	ok &= expect(lookedUp.get() == held.get(), "held region must stay cached as the same object under pressure");
	std::string viaCache = "written via cache lookup";
	ok &= expect(writePayload(*lookedUp, 2, 0, viaCache), "cache-returned handle should be writable");
	ok &= expect(readMatches(*held, 2, 0, viaCache), "held handle must see write done via cache lookup");
	ok &= expect(readMatches(*held, 0, 0, small), "held handle must keep earlier data under pressure");

	// The unheld region was evictable; re-fetching after it was closed and
	// reopened must return intact data. (Object identity is not asserted; the
	// allocator may reuse the old address.)
	std::shared_ptr<RegionFile> reopened = RegionFileCache::getRegionFile(baseDir, -32, 0);
	ok &= expect(readMatches(*reopened, 0, 0, other), "reopened region should retain flushed data");
	return true;
}

static bool runPendingSaveCases(bool &ok)
{
	RegionIoSmokeLevel level;
	std::unique_ptr<File> storageDir(File::open(u"build/region-io-smoke/storage"));
	removeDirectory(*storageDir);
	McRegionChunkStorage storage(std::shared_ptr<File>(File::open(u"build/region-io-smoke/storage")), true);

	// Queue several filler saves so the worker is busy when the target chunk is
	// loaded; the target's snapshot is then still in the pending map and the
	// load must serve it from the queued NBT without waiting for the disk write.
	for (int_t i = 0; i < 8; ++i)
	{
		std::shared_ptr<LevelChunk> filler = makePatternChunk(level, 10 + i, 3, i);
		storage.save(level, *filler);
	}
	std::shared_ptr<LevelChunk> target = makePatternChunk(level, 5, 7, 99);
	storage.save(level, *target);

	std::shared_ptr<LevelChunk> loaded = storage.load(level, 5, 7);
	ok &= expect(loaded != nullptr, "read-after-write load should find the just-saved chunk");
	if (loaded != nullptr)
	{
		ok &= expect(loaded->isAt(5, 7), "loaded chunk should be at the saved position");
		ok &= expect(loaded->blocks == target->blocks, "loaded blocks should match saved blocks before flush");
	}

	// After flush the pending map is drained; the same data must come back from
	// the region file itself.
	storage.flush();
	std::shared_ptr<LevelChunk> fromDisk = storage.load(level, 5, 7);
	ok &= expect(fromDisk != nullptr, "post-flush load should find the chunk on disk");
	if (fromDisk != nullptr)
		ok &= expect(fromDisk->blocks == target->blocks, "post-flush blocks should match saved blocks");
	return true;
}

static void runPinnedCapacityCase(bool &ok)
{
	std::vector<std::shared_ptr<RegionFile>> held;
	for (int i = 0; i < 260; ++i)
		held.push_back(RegionFileCache::getRegionFile("build/region-io-smoke/pinned", i * 32, 0));
	std::weak_ptr<RegionFile> fifthOldest = held[4];
	std::weak_ptr<RegionFile> newest = held.back();
	held.clear();
	RegionFileCache::getRegionFile("build/region-io-smoke/pinned", 260 * 32, 0);
	ok &= expect(fifthOldest.expired(), "released pinned files should let the cache return to its soft limit");
	ok &= expect(!newest.expired(), "shrinking an over-limit cache should retain its recent entries");
}

int runRegionIoSmoke()
{
	std::unique_ptr<File> root(File::open(u"build/region-io-smoke"));
	RegionFileCache::clearCache();
	removeDirectory(*root);
	root->mkdirs();

	bool ok = true;
	try
	{
		runRegionFileCases(ok);
		RegionFileCache::clearCache();
		runPinnedCapacityCase(ok);
		RegionFileCache::clearCache();
		runPendingSaveCases(ok);
	}
	catch (const std::exception &exception)
	{
		RegionFileCache::clearCache();
		removeDirectory(*root);
		std::cerr << "Region IO smoke exception: " << exception.what() << '\n';
		return 1;
	}

	RegionFileCache::clearCache();
	removeDirectory(*root);

	if (!ok)
		return 1;
	std::cout << "Region IO smoke passed\n";
	return 0;
}
