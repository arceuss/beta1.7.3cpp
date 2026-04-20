#pragma once

#include "world/level/tile/RailTile.h"

class DetectorRailTile : public RailTile
{
private:
	void updateMinecartState(Level &level, int_t x, int_t y, int_t z, int_t data);

public:
	DetectorRailTile(int_t id, int_t tex);

	int_t getTickDelay() override { return 20; }
	bool isSignalSource() override { return true; }
	bool isDirectSignalTo(Level &level, int_t x, int_t y, int_t z, int_t dir) override;
	bool isIndirectSignalTo(Level &level, int_t x, int_t y, int_t z, int_t dir) override;
	void entityInside(Level &level, int_t x, int_t y, int_t z, Entity &entity) override;
	void tick(Level &level, int_t x, int_t y, int_t z, Random &random) override;
};
