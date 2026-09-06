#include "world/level/tile/IceTile.h"

#include "world/level/Level.h"
#include "world/level/LevelSource.h"
#include "world/level/material/Material.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/Tile.h"

IceTile::IceTile(int_t id, int_t tex) : TransparentTile(id, tex, Material::ice(), false)
{
	friction = 0.98f;
	setTicking(true);
	updateCachedProperties();
}

int_t IceTile::getRenderLayer()
{
	return 1;
}

bool IceTile::shouldRenderFace(LevelSource &level, int_t x, int_t y, int_t z, Facing face)
{
	// B173 - IceTile tests the opposite face: super.shouldRenderFace(..., 1 - face).
	// The parent is the half-transparent rule, so ice against ice culls the shared
	// face. For the four horizontal faces 1 - face is negative and matches no shape
	// test, leaving the base rule at its !isSolidTile fallthrough.
	const Facing opposite = static_cast<Facing>(1 - static_cast<int_t>(face));
	return TransparentTile::shouldRenderFace(level, x, y, z, opposite);
}

void IceTile::harvestBlock(Level &level, Player &player, int_t x, int_t y, int_t z, int_t data)
{
	Tile::harvestBlock(level, player, x, y, z, data);
	const Material &below = level.getMaterial(x, y - 1, z);
	if (below.blocksMotion() || below.isLiquid())
		level.setTile(x, y, z, Tile::water.id);
}

int_t IceTile::getResourceCount(Random &random)
{
	(void)random;
	return 0;
}

void IceTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	if (level.getBrightness(LightLayer::Block, x, y, z) > 11 - Tile::lightBlock[id])
	{
		spawnResources(level, x, y, z, level.getData(x, y, z));
		level.setTile(x, y, z, Tile::calmWater.id);
	}
}

