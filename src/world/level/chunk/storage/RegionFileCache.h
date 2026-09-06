#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "java/Type.h"

class File;
class RegionFile;

class RegionFileCache
{
private:
	static const int MAX_CACHE_SIZE = 256;

	// B173 - LRU cache of open region files. `lru` orders paths most- to
	// least-recently used; each map entry keeps its own position in that list.
	struct Entry
	{
		std::shared_ptr<RegionFile> file;
		std::list<std::string>::iterator lruPos;
	};

	static std::unordered_map<std::string, Entry> cache;
	static std::list<std::string> lru;
	static std::recursive_mutex mutex;

	static std::string getRegionPath(const std::string &baseDir, int_t chunkX, int_t chunkZ);

public:
	static std::shared_ptr<RegionFile> getRegionFile(const std::string &baseDir, int_t chunkX, int_t chunkZ);
	static void clearCache();

	static int_t getSizeDelta(const std::string &baseDir, int_t chunkX, int_t chunkZ);
};
