#pragma once

#include "world/level/chunk/storage/ChunkStorage.h"

#include <string>

class File;

class McRegionChunkStorage : public ChunkStorage
{
private:
	std::string baseDir;

public:
	McRegionChunkStorage(std::shared_ptr<File> dir, bool create);

	std::shared_ptr<LevelChunk> load(Level &level, int_t x, int_t z) override;
	void save(Level &level, LevelChunk &chunk) override;
	void saveEntities(Level &level, LevelChunk &chunk) override;
	void tick() override;
	void flush() override;
};
