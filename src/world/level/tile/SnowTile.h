#pragma once

#include "world/level/tile/Tile.h"

class SnowTile : public Tile
{
public:
	SnowTile(int_t id, int_t tex);

	bool isCubeShaped() override;
	bool isSolidRender() override;
	AABB *getAABB(Level &level, int_t x, int_t y, int_t z) override;
	void neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile) override;
	void onPlace(Level &level, int_t x, int_t y, int_t z) override;
	void updateShape(LevelSource &level, int_t x, int_t y, int_t z) override;
	void updateDefaultShape() override;

private:
	bool canSnowStay(LevelSource &level, int_t x, int_t y, int_t z);
};
