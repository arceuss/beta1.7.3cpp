#pragma once

#include "world/level/levelgen/feature/Feature.h"

// WorldGenFire
class FireFeature : public Feature
{
public:
	bool place(Level &level, Random &random, int_t x, int_t y, int_t z) override;
};

// WorldGenGlowStone1
class GlowStone1Feature : public Feature
{
public:
	bool place(Level &level, Random &random, int_t x, int_t y, int_t z) override;
};

// WorldGenGlowStone2
class GlowStone2Feature : public Feature
{
public:
	bool place(Level &level, Random &random, int_t x, int_t y, int_t z) override;
};

// WorldGenHellLava
class HellLavaFeature : public Feature
{
	int_t liquidId;
public:
	HellLavaFeature(int_t liquidId);
	bool place(Level &level, Random &random, int_t x, int_t y, int_t z) override;
};
