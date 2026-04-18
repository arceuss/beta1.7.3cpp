#pragma once

#include <array>

#include "world/level/levelgen/synth/PerlinSimplexNoise.h"

#include "java/Type.h"

class Level;

enum class TreeStyle
{
	Default,
	Rainforest,
	Forest,
	Taiga,
};

enum class BiomeId
{
	Rainforest,
	Swampland,
	SeasonalForest,
	Forest,
	Savanna,
	Shrubland,
	Taiga,
	Desert,
	Plains,
	Tundra,
};

struct BiomeInfo
{
	int_t topTileId = 0;
	int_t fillerTileId = 0;
	bool enableSnow = false;
	TreeStyle treeStyle = TreeStyle::Default;
	int_t treeCountAdjustment = 0;
	int_t yellowFlowerCount = 0;
	int_t tallGrassCount = 0;
	int_t deadBushCount = 0;
	int_t cactusCount = 0;
	bool prefersFern = false;
};

class BiomeSource
{
private:
	PerlinSimplexNoise temperatureMap;
	PerlinSimplexNoise downfallMap;
	PerlinSimplexNoise noiseMap;

public:
	std::array<double, 16 * 16> temperatures = {};
	std::array<double, 16 * 16> downfalls = {};
	std::array<double, 16 * 16> noises = {};
	std::array<BiomeId, 16 * 16> biomes = {};

private:
	static constexpr float zoom = 2.0f;
	static constexpr float tempScale = 0.025f;
	static constexpr float downfallScale = 0.05f;
	static constexpr float noiseScale = 0.25f;

	static BiomeId pickBiome(float temperature, float downfall);
	static BiomeId getBiomeFromLookup(double temperature, double downfall);

public:
	BiomeSource(Level &level);

	double getTemperature(int_t x, int_t z);
	BiomeId getBiome(int_t x, int_t z);
	const BiomeInfo &getBiomeInfo(BiomeId biome) const;

	void getBiomeBlock(int_t x, int_t z, int_t xd, int_t zd);
};
