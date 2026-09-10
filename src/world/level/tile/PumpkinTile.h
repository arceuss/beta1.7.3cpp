#pragma once

#include "world/level/tile/Tile.h"

class PumpkinTile : public Tile
{
private:
	bool lit = false;

public:
	PumpkinTile(int_t id, int_t tex, bool lit);

	int_t getTexture(Facing face, int_t data) override;
	bool mayPlace(Level &level, int_t x, int_t y, int_t z) override;
	void setPlacedBy(Level &level, int_t x, int_t y, int_t z, Player &player) override;
};