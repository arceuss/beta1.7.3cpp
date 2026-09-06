#include "tools/stress/StateDigest.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "tools/stress/Sha256.h"

#include "java/String.h"
#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"
#include "world/entity/Entity.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkCache.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/tile/entity/TileEntity.h"

namespace stress
{
// Everything is fed to SHA-256 as little-endian fixed-width fields so the
// digest is a function of values, never of struct padding or host layout.
static void putU64(Sha256 &hash, ulong_t value)
{
	unsigned char bytes[8];
	for (int i = 0; i < 8; ++i)
		bytes[i] = static_cast<unsigned char>(value >> (i * 8));
	hash.update(bytes, 8);
}

static void putI64(Sha256 &hash, long_t value) { putU64(hash, static_cast<ulong_t>(value)); }
static void putI32(Sha256 &hash, int_t value) { putU64(hash, static_cast<ulong_t>(static_cast<uint_t>(value))); }
static void putBool(Sha256 &hash, bool value) { putU64(hash, value ? 1u : 0u); }

// Exact IEEE bit patterns (report requirement), not printed decimals.
static void putF32(Sha256 &hash, float value)
{
	uint_t bits;
	std::memcpy(&bits, &value, 4);
	putU64(hash, bits);
}

static void putF64(Sha256 &hash, double value)
{
	ulong_t bits;
	std::memcpy(&bits, &value, 8);
	putU64(hash, bits);
}

static void putString(Sha256 &hash, const jstring &text)
{
	const std::string utf8 = String::toUTF8(text);
	putU64(hash, utf8.size());
	hash.update(utf8.data(), utf8.size());
}

static void putRandom(Sha256 &hash, const Random &random)
{
	putU64(hash, random.rawState());
	putBool(hash, random.hasPendingGaussian());
	putF64(hash, random.pendingGaussian());
}

// Canonical NBT: compounds hash their entries in sorted-key order (the live
// CompoundTag stores an unordered_map, so its native write() order is not
// stable); lists recurse in element order; every leaf hashes its exact wire
// bytes via Tag::write, which is deterministic for non-compound tags.
static void hashTag(Sha256 &hash, Tag &tag)
{
	const byte_t id = tag.getId();
	putU64(hash, static_cast<ulong_t>(id));
	if (id == Tag::TAG_Compound)
	{
		CompoundTag &compound = static_cast<CompoundTag &>(tag);
		std::vector<std::pair<jstring, std::shared_ptr<Tag>>> entries(
			compound.getAllTags().begin(), compound.getAllTags().end());
		std::sort(entries.begin(), entries.end(),
			[](const std::pair<jstring, std::shared_ptr<Tag>> &a,
				const std::pair<jstring, std::shared_ptr<Tag>> &b) { return a.first < b.first; });
		putU64(hash, entries.size());
		for (const auto &entry : entries)
		{
			putString(hash, entry.first);
			hashTag(hash, *entry.second);
		}
	}
	else if (id == Tag::TAG_List)
	{
		ListTag &list = static_cast<ListTag &>(tag);
		putU64(hash, static_cast<ulong_t>(list.size()));
		for (int_t i = 0; i < list.size(); ++i)
			hashTag(hash, *list.get(i));
	}
	else
	{
		std::ostringstream bytes(std::ios::binary);
		tag.write(bytes);
		const std::string wire = bytes.str();
		putU64(hash, wire.size());
		hash.update(wire.data(), wire.size());
	}
}

static void hashEntityList(Sha256 &hash, std::vector<std::shared_ptr<Entity>> entities)
{
	std::sort(entities.begin(), entities.end(),
		[](const std::shared_ptr<Entity> &a, const std::shared_ptr<Entity> &b)
		{ return a->entityId < b->entityId; });
	putU64(hash, entities.size());
	for (const std::shared_ptr<Entity> &entityPtr : entities)
	{
		Entity &entity = *entityPtr;
		putI32(hash, entity.entityId);
		putString(hash, entity.getEncodeId());
		putF64(hash, entity.x);
		putF64(hash, entity.y);
		putF64(hash, entity.z);
		putF64(hash, entity.xd);
		putF64(hash, entity.yd);
		putF64(hash, entity.zd);
		putF32(hash, entity.yRot);
		putF32(hash, entity.xRot);
		putBool(hash, entity.onGround);
		putI32(hash, entity.onFire);
		putI32(hash, entity.tickCount);
		putBool(hash, entity.removed);
		putRandom(hash, entity.auditRandom());
		// Full serialized state (health, inventory, AI fields) via canonical NBT.
		CompoundTag tag;
		entity.saveWithoutId(tag);
		hashTag(hash, tag);
	}
}

static void hashChunkLighting(Sha256 &hash, LevelChunk &chunk)
{
	putI32(hash, chunk.x);
	putI32(hash, chunk.z);
	hash.update(chunk.skyLight.data.data(), chunk.skyLight.data.size());
	hash.update(chunk.blockLight.data.data(), chunk.blockLight.data.size());
	hash.update(chunk.heightmap.data(), chunk.heightmap.size());
	putI32(hash, chunk.minHeight);
}

const char *coverageStatement()
{
	return "state digest covers every loaded chunk in sorted (x,z) order (blocks,data,skylight,"
		"blocklight,heightmap,populated), tile entities sorted by position (encode_id,position,"
		"removed,full canonical sorted-key NBT), entities and weather entities sorted by id "
		"(id,encode_id,pos/vel/rot IEEE bits,onGround,onFire,tickCount,removed,per-entity RNG "
		"state,full canonical sorted-key NBT), world time/seed/spawn, weather internals, pending "
		"scheduled ticks in execution order, level RNG state, and the ordered light-update queue; "
		"excluded: map item data, session id, and render-only RNGs (gui/particles/sound engine)";
}

DigestResult digestLevel(Level &level)
{
	DigestResult result;
	Sha256 state;
	Sha256 light;

	// World scalars.
	putI64(state, level.time);
	putI64(state, level.seed);
	putI32(state, level.xSpawn);
	putI32(state, level.ySpawn);
	putI32(state, level.zSpawn);
	putI32(state, level.skyDarken);
	putI32(state, level.difficulty);
	const Level::WeatherAudit weather = level.auditWeather();
	putF32(state, weather.previousRainingStrength);
	putF32(state, weather.rainingStrength);
	putF32(state, weather.previousThunderingStrength);
	putF32(state, weather.thunderingStrength);
	putBool(state, weather.raining);
	putBool(state, weather.thundering);
	putI32(state, weather.rainTime);
	putI32(state, weather.thunderTime);
	putI32(state, weather.randValue);
	putI32(state, weather.addend);

	// Level RNG (raw LCG state plus the buffered second gaussian).
	putRandom(state, level.random);

	// Pending scheduled tile ticks in deterministic execution order.
	std::vector<long_t> pendingTicks;
	level.auditPendingTicks(pendingTicks);
	putU64(state, pendingTicks.size());
	for (const long_t value : pendingTicks)
		putI64(state, value);

	// Every chunk the local cache holds, in sorted (x,z) order. No loads are
	// triggered; farlands-distance chunks are covered because enumeration does
	// not depend on a scan window.
	ChunkCache *cache = dynamic_cast<ChunkCache *>(level.getChunkSource().get());
	if (!cache)
		throw std::runtime_error("State hashing requires the local ChunkCache chunk source");
	std::vector<std::shared_ptr<LevelChunk>> chunks;
	for (const std::shared_ptr<LevelChunk> &chunk : cache->auditChunks())
		if (chunk && !chunk->isEmpty())
			chunks.push_back(chunk);
	std::sort(chunks.begin(), chunks.end(),
		[](const std::shared_ptr<LevelChunk> &a, const std::shared_ptr<LevelChunk> &b)
		{ return a->x != b->x ? a->x < b->x : a->z < b->z; });
	result.chunkCount = static_cast<int>(chunks.size());
	putU64(state, chunks.size());
	putU64(light, chunks.size());
	std::vector<std::pair<TilePos, std::shared_ptr<TileEntity>>> chunkTiles;
	for (const std::shared_ptr<LevelChunk> &chunk : chunks)
	{
		putI32(state, chunk->x);
		putI32(state, chunk->z);
		state.update(chunk->blocks.data(), chunk->blocks.size());
		state.update(chunk->data.data.data(), chunk->data.data.size());
		state.update(chunk->skyLight.data.data(), chunk->skyLight.data.size());
		state.update(chunk->blockLight.data.data(), chunk->blockLight.data.size());
		state.update(chunk->heightmap.data(), chunk->heightmap.size());
		putBool(state, chunk->terrainPopulated);
		hashChunkLighting(light, *chunk);

		// Tile entities sorted by position, interiors included via canonical NBT.
		chunkTiles.assign(chunk->tileEntities.begin(), chunk->tileEntities.end());
		std::sort(chunkTiles.begin(), chunkTiles.end(),
			[](const std::pair<TilePos, std::shared_ptr<TileEntity>> &a,
				const std::pair<TilePos, std::shared_ptr<TileEntity>> &b)
			{
				const std::shared_ptr<TileEntity> &ta = a.second, &tb = b.second;
				if (ta->x != tb->x) return ta->x < tb->x;
				if (ta->y != tb->y) return ta->y < tb->y;
				return ta->z < tb->z;
			});
		putU64(state, chunkTiles.size());
		for (const auto &entry : chunkTiles)
		{
			TileEntity &tile = *entry.second;
			putString(state, tile.getEncodeId());
			putI32(state, tile.x);
			putI32(state, tile.y);
			putI32(state, tile.z);
			putBool(state, tile.isRemoved());
			CompoundTag tag;
			tile.save(tag);
			hashTag(state, tag);
		}
	}

	// Entities sorted by stable id (ids are unique per process run), then the
	// weather-effect entities (lightning bolts) as their own section.
	hashEntityList(state, level.entities);
	hashEntityList(state, level.getWeatherEffects());

	// Ordered light-update queue: part of both digests so a lighting divergence
	// is visible even before it lands in chunk light arrays.
	putU64(light, level.lightUpdates.size());
	for (const LightUpdate &update : level.lightUpdates)
	{
		putI32(light, update.layer);
		putI32(light, update.x0);
		putI32(light, update.y0);
		putI32(light, update.z0);
		putI32(light, update.x1);
		putI32(light, update.y1);
		putI32(light, update.z1);
	}
	putI64(light, level.time);

	result.lightHex = light.finishHex();
	// Fold the light digest into the state digest so state_sha256 alone gates parity.
	state.update(result.lightHex.data(), result.lightHex.size());
	result.stateHex = state.finishHex();
	return result;
}
}
