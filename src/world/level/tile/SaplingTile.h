#pragma once

#include "world/level/tile/FlowerTile.h"

class SaplingTile : public FlowerTile
{
public:
	SaplingTile(int_t id, int_t tex);

	void tick(Level &level, int_t x, int_t y, int_t z, Random &random) override;
	void updateDefaultShape() override;
	int_t getTexture(Facing face, int_t data) override;
	int_t getResource(int_t data, Random &random) override;
	int_t getSpawnResourcesAuxValue(int_t data) override;

	// BlockSapling.growTree; also the bone meal entry point from ItemDye.
	void growTree(Level &level, int_t x, int_t y, int_t z, Random &random);
};