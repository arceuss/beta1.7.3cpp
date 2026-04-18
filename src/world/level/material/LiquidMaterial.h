#pragma once

#include "world/level/material/Material.h"

class LiquidMaterial : public Material
{
public:
	bool isLiquid() const override;
	bool isSolid() const override;
	bool blocksLight() const override;
	bool blocksMotion() const override;
};
