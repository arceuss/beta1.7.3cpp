#include "world/level/chunk/storage/RegionFileCache.h"

#include "world/level/chunk/storage/RegionFile.h"

#include "java/File.h"
#include "java/String.h"

#include <iostream>
#include <sstream>

std::unordered_map<std::string, RegionFileCache::Entry> RegionFileCache::cache;
std::list<std::string> RegionFileCache::lru;
std::recursive_mutex RegionFileCache::mutex;

// Arithmetic right shift for negative coords: floor division by 32
static int_t regionCoord(int_t chunkCoord)
{
	return chunkCoord >> 5;
}

std::string RegionFileCache::getRegionPath(const std::string &baseDir, int_t chunkX, int_t chunkZ)
{
	std::ostringstream oss;
	oss << baseDir << "/region/r." << regionCoord(chunkX) << "." << regionCoord(chunkZ) << ".mcr";
	return oss.str();
}

std::shared_ptr<RegionFile> RegionFileCache::getRegionFile(const std::string &baseDir, int_t chunkX, int_t chunkZ)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	std::string path = getRegionPath(baseDir, chunkX, chunkZ);

	auto it = cache.find(path);
	if (it != cache.end())
	{
		// Move to most-recently-used position.
		lru.splice(lru.begin(), lru, it->second.lruPos);
		return it->second.file;
	}

	// Ensure the region directory exists
	jstring regionDirPath = String::fromUTF8(baseDir) + u"/region";
	std::unique_ptr<File> regionDir(File::open(regionDirPath));
	if (!regionDir->exists())
		regionDir->mkdirs();

	// Evict the least-recently-used entry that nothing else holds. Externally
	// held files must stay cached so a later lookup for the same path returns
	// the same open object (a second RegionFile on one path would have its own
	// free-sector map and mutex, risking on-disk corruption). If every entry is
	// held, temporarily exceed the limit.
	for (auto lruIt = lru.end(); cache.size() >= MAX_CACHE_SIZE && lruIt != lru.begin();)
	{
		--lruIt;
		auto evictIt = cache.find(*lruIt);
		if (evictIt->second.file.use_count() > 1)
			continue;
		evictIt->second.file->close();
		cache.erase(evictIt);
		lruIt = lru.erase(lruIt);
	}

	auto regionFile = std::make_shared<RegionFile>(path);
	lru.push_front(path);
	cache[path] = Entry{regionFile, lru.begin()};
	return regionFile;
}

void RegionFileCache::clearCache()
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	for (auto &pair : cache)
		pair.second.file->close();

	cache.clear();
	lru.clear();
}

int_t RegionFileCache::getSizeDelta(const std::string &baseDir, int_t chunkX, int_t chunkZ)
{
	auto regionFile = getRegionFile(baseDir, chunkX, chunkZ);
	return regionFile->getSizeDelta();
}
