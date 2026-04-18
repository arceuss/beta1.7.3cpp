#pragma once

#include "world/level/tile/Tile.h"

class PumpkinTile : public Tile
{
public:
	PumpkinTile(int_t id, int_t tex);

	int_t getTexture(Facing face, int_t data) override;
};
