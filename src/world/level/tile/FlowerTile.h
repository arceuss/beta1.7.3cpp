#pragma once

#include "world/level/tile/Tile.h"

class FlowerTile : public Tile
{
public:
	FlowerTile(int_t id, int_t tex);

	bool isCubeShaped() override;
	Shape getRenderShape() override;
	AABB *getAABB(Level &level, int_t x, int_t y, int_t z) override;
	bool isSolidRender() override;
	void tick(Level &level, int_t x, int_t y, int_t z, Random &random) override;
	void neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile) override;
	bool mayPlace(Level &level, int_t x, int_t y, int_t z) override;
	void updateDefaultShape() override;

	virtual bool canStay(Level &level, int_t x, int_t y, int_t z);

protected:
	// BlockFlower.func_268_h
	void checkAlive(Level &level, int_t x, int_t y, int_t z);

	virtual bool canSurviveOn(int_t belowTile) const;
};
