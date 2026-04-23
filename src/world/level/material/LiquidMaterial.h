#pragma once

#include "world/level/material/Material.h"

class MapColor;

class LiquidMaterial : public Material
{
public:
	LiquidMaterial() = default;
	explicit LiquidMaterial(MapColor &color) { setMapColor(color); }

	bool isLiquid() const override;
	bool isSolid() const override;
	bool blocksLight() const override;
	bool blocksMotion() const override;
};
