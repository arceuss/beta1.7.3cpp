#include "world/level/tile/TallGrassTile.h"

#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/LevelSource.h"
#include "world/level/GrassColor.h"

TallGrassTile::TallGrassTile(int_t id, int_t tex) : FlowerTile(id, tex)
{
	updateDefaultShape();
}

int_t TallGrassTile::getTexture(Facing face, int_t data)
{
	if (data == 1)
		return tex;
	if (data == 2)
		return tex + 17;
	if (data == 0)
		return tex + 16;
	return tex;
}

int_t TallGrassTile::getColor(LevelSource &level, int_t x, int_t y, int_t z)
{
	int_t data = level.getData(x, y, z);
	if (data == 0)
		return 0xFFFFFF;

	// BlockTallGrass.colorMultiplier. The seed is a 32-bit int expression in Java, so it
	// wraps before it is sign-extended to 64 bits; unsigned math keeps that exact.
	uint_t seed = static_cast<uint_t>(x) * 3129871u + static_cast<uint_t>(z) * 6129781u + static_cast<uint_t>(y);
	ulong_t hash = static_cast<ulong_t>(static_cast<long_t>(static_cast<int_t>(seed)));
	hash = hash * hash * 42317861ull + hash * 11ull;

	// Java also displaces y here, but only x and z reach the biome lookup.
	int_t sampleX = static_cast<int_t>(static_cast<long_t>(x) + static_cast<long_t>((hash >> 14) & 0x1Full));
	int_t sampleZ = static_cast<int_t>(static_cast<long_t>(z) + static_cast<long_t>((hash >> 24) & 0x1Full));

	level.getBiomeSource().getBiomeBlock(sampleX, sampleZ, 1, 1);

	return GrassColor::get(level.getBiomeSource().temperatures[0], level.getBiomeSource().downfalls[0]);
}

// Beta 1.7.3 has no ItemTallGrass: the item icon uses Item.getColorFromDamage, white.
int_t TallGrassTile::getItemColor(int_t data)
{
	(void)data;
	return 0xFFFFFF;
}


int_t TallGrassTile::getResource(int_t data, Random &random)
{
	return random.nextInt(8) == 0 ? Items::seeds->getShiftedIndex() : -1;
}

void TallGrassTile::updateDefaultShape()
{
	float radius = 0.4f;
	setShape(0.5f - radius, 0.0f, 0.5f - radius, 0.5f + radius, 0.8f, 0.5f + radius);
}