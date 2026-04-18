#include "world/level/material/Material.h"

#include "world/level/material/GasMaterial.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/material/DecorationMaterial.h"

GasMaterial Material::air;
Material Material::dirt;
Material Material::wood = Material().flammable();
Material Material::stone;
Material Material::sand;
LiquidMaterial Material::water;
LiquidMaterial Material::lava;

Material &Material::plants()
{
	static DecorationMaterial material(false, false, false, true);
	return material;
}

Material &Material::cactus()
{
	static DecorationMaterial material(false, false, true, false);
	return material;
}

Material &Material::pumpkin()
{
	static DecorationMaterial material(true, true, true, false);
	return material;
}

Material &Material::snow()
{
	static DecorationMaterial material(false, false, false, true);
	return material;
}

Material &Material::ice()
{
	static DecorationMaterial material(false, false, true, false);
	return material;
}

bool Material::isLiquid() const
{
	return false;
}

bool Material::letsWaterThrough() const
{
	return (!isLiquid() && !isSolid());
}

bool Material::isSolid() const
{
	return true;
}

bool Material::blocksLight() const
{
	return true;
}

bool Material::blocksMotion() const
{
	return true;
}

Material &Material::flammable()
{
	flammableFlag = true;
	return *this;
}

bool Material::isFlammable() const
{
	return flammableFlag;
}
