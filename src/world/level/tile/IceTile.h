#pragma once

#include "world/level/tile/TransparentTile.h"

class IceTile : public TransparentTile
{
public:
	IceTile(int_t id, int_t tex);

	int_t getRenderLayer() override;

private:
	bool isTranslucent() override;
};
