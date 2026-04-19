#include "world/level/material/Material.h"

#include "world/level/material/GasMaterial.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/material/DecorationMaterial.h"

GasMaterial Material::air;
Material Material::dirt;
Material Material::wood = Material().flammable();
Material Material::stone = Material().noHarvest();
Material Material::sand;
Material Material::clay;
LiquidMaterial Material::water;
LiquidMaterial Material::lava;

Material Material::sponge;
Material Material::cloth = Material().flammable();
Material Material::glass;
Material Material::iron = Material().noHarvest();
Material Material::builtSnow = Material().noHarvest();
Material Material::tnt = Material().flammable();
Material Material::web = Material().noHarvest();
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
	static bool initialized = (material.noHarvest(), true);
	(void)initialized;
	return material;
}

Material &Material::ice()
{
	static DecorationMaterial material(false, false, true, false);
	return material;
}

Material &Material::circuits()
{
	static DecorationMaterial material(false, false, false, true);
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

bool Material::isHarvestable() const
{
	return harvestableFlag;
}

Material &Material::flammable()
{
	flammableFlag = true;
	return *this;
}

Material &Material::noHarvest()
{
	harvestableFlag = false;
	return *this;
}

bool Material::isFlammable() const
{
	return flammableFlag;
}
