#include "world/level/biome/BiomeSource.h"

#include <algorithm>

#include "world/level/Level.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/GrassTile.h"
#include "world/level/tile/DirtTile.h"
#include "world/level/tile/SandTile.h"

namespace
{
	constexpr int_t BIOME_LOOKUP_SIZE = 64;

	BiomeId pickBiomeForLookup(float temperature, float downfall)
	{
		downfall *= temperature;

		if (temperature < 0.1f)
			return BiomeId::Tundra;
		if (downfall < 0.2f)
		{
			if (temperature < 0.5f)
				return BiomeId::Tundra;
			if (temperature < 0.95f)
				return BiomeId::Savanna;
			return BiomeId::Desert;
		}
		if (downfall > 0.5f && temperature < 0.7f)
			return BiomeId::Swampland;
		if (temperature < 0.5f)
			return BiomeId::Taiga;
		if (temperature < 0.97f)
			return downfall < 0.35f ? BiomeId::Shrubland : BiomeId::Forest;
		if (downfall < 0.45f)
			return BiomeId::Plains;
		if (downfall < 0.9f)
			return BiomeId::SeasonalForest;
		return BiomeId::Rainforest;
	}

	std::array<BiomeId, BIOME_LOOKUP_SIZE * BIOME_LOOKUP_SIZE> createBiomeLookup()
	{
		std::array<BiomeId, BIOME_LOOKUP_SIZE * BIOME_LOOKUP_SIZE> lookup = {};

		for (int_t temperatureIndex = 0; temperatureIndex < BIOME_LOOKUP_SIZE; ++temperatureIndex)
		{
			for (int_t downfallIndex = 0; downfallIndex < BIOME_LOOKUP_SIZE; ++downfallIndex)
			{
				lookup[temperatureIndex + downfallIndex * BIOME_LOOKUP_SIZE] = pickBiomeForLookup(
					static_cast<float>(temperatureIndex) / static_cast<float>(BIOME_LOOKUP_SIZE - 1),
					static_cast<float>(downfallIndex) / static_cast<float>(BIOME_LOOKUP_SIZE - 1));
			}
		}

		return lookup;
	}

	const std::array<BiomeId, BIOME_LOOKUP_SIZE * BIOME_LOOKUP_SIZE> &getBiomeLookup()
	{
		static const std::array<BiomeId, BIOME_LOOKUP_SIZE * BIOME_LOOKUP_SIZE> lookup = createBiomeLookup();
		return lookup;
	}

	const std::array<BiomeInfo, 10> &getBiomeInfos()
	{
		static const std::array<BiomeInfo, 10> infos = {{
			{ Tile::grass.id, Tile::dirt.id, false, TreeStyle::Rainforest, 5, 0, 10, 0, 0, true },
			{ Tile::grass.id, Tile::dirt.id, false, TreeStyle::Default, 0, 0, 0, 0, 0, false },
			{ Tile::grass.id, Tile::dirt.id, false, TreeStyle::Default, 2, 4, 2, 0, 0, false },
			{ Tile::grass.id, Tile::dirt.id, false, TreeStyle::Forest, 5, 2, 2, 0, 0, false },
			{ Tile::grass.id, Tile::dirt.id, false, TreeStyle::Default, 0, 0, 0, 0, 0, false },
			{ Tile::grass.id, Tile::dirt.id, false, TreeStyle::Default, 0, 0, 0, 0, 0, false },
			{ Tile::grass.id, Tile::dirt.id, true, TreeStyle::Taiga, 5, 2, 1, 0, 0, false },
			{ Tile::sand.id, Tile::sand.id, false, TreeStyle::Default, -20, 0, 0, 2, 10, false },
			{ Tile::grass.id, Tile::dirt.id, false, TreeStyle::Default, -20, 3, 10, 0, 0, false },
			{ Tile::grass.id, Tile::dirt.id, true, TreeStyle::Default, -20, 0, 0, 0, 0, false },
		}};
		return infos;
	}
}

BiomeSource::BiomeSource(Level &level) :
	temperatureMap(Random(level.seed * 9871LL), 4),
	downfallMap(Random(level.seed * 39811LL), 4),
	noiseMap(Random(level.seed * 543321LL), 2)
{
	
}

BiomeId BiomeSource::pickBiome(float temperature, float downfall)
{
	return pickBiomeForLookup(temperature, downfall);
}

BiomeId BiomeSource::getBiomeFromLookup(double temperature, double downfall)
{
	int_t temperatureIndex = std::max(0, std::min(BIOME_LOOKUP_SIZE - 1, static_cast<int_t>(temperature * (BIOME_LOOKUP_SIZE - 1))));
	int_t downfallIndex = std::max(0, std::min(BIOME_LOOKUP_SIZE - 1, static_cast<int_t>(downfall * (BIOME_LOOKUP_SIZE - 1))));
	return getBiomeLookup()[temperatureIndex + downfallIndex * BIOME_LOOKUP_SIZE];
}

double BiomeSource::getTemperature(int_t x, int_t z)
{
	temperatureMap.getRegion(temperatures.data(), x, z, 1, 1, tempScale, tempScale, 0.5);
	return temperatures[0];
}

BiomeId BiomeSource::getBiome(int_t x, int_t z)
{
	getBiomeBlock(x, z, 1, 1);
	return biomes[0];
}

const BiomeInfo &BiomeSource::getBiomeInfo(BiomeId biome) const
{
	return getBiomeInfos()[static_cast<int_t>(biome)];
}

void BiomeSource::getBiomeBlock(int_t x, int_t z, int_t xd, int_t zd)
{
	temperatureMap.getRegion(temperatures.data(), x, z, xd, zd, tempScale, tempScale, 1.0 / 4.0);
	downfallMap.getRegion(downfalls.data(), x, z, xd, zd, downfallScale, downfallScale, 1.0 / 3.0);
	noiseMap.getRegion(noises.data(), x, z, xd, zd, noiseScale, noiseScale, 1.0 / 1.7);

	int_t index = 0;

	for (int_t xi = 0; xi < xd; ++xi)
	{
		for (int_t zi = 0; zi < zd; ++zi)
		{
			double nv = noises[index] * 1.1 + 0.5;

			double blend = 0.01;
			double inverseBlend = 1.0 - blend;
			double temperature = (temperatures[index] * 0.15 + 0.7) * inverseBlend + nv * blend;

			blend = 0.002;
			inverseBlend = 1.0 - blend;
			double downfall = (downfalls[index] * 0.15 + 0.5) * inverseBlend + nv * blend;

			temperature = 1.0 - (1.0 - temperature) * (1.0 - temperature);
			temperature = std::max(0.0, std::min(1.0, temperature));
			downfall = std::max(0.0, std::min(1.0, downfall));

			temperatures[index] = temperature;
			downfalls[index] = downfall;
			biomes[index] = getBiomeFromLookup(temperature, downfall);

			++index;
		}
	}
}
