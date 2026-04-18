#include "world/level/levelgen/feature/FlowerFeature.h"

#include "world/level/Level.h"

FlowerFeature::FlowerFeature(int_t flowerId) : flowerId(flowerId) {}

bool FlowerFeature::place(Level &level, Random &random, int_t x, int_t y, int_t z) {
	for (int_t i = 0; i < 64; ++i) {
		int_t xx = x + random.nextInt(8) - random.nextInt(8);
		int_t yy = y + random.nextInt(4) - random.nextInt(4);
		int_t zz = z + random.nextInt(8) - random.nextInt(8);
		if (level.isEmptyTile(xx, yy, zz))
			level.setTileNoUpdate(xx, yy, zz, flowerId);
	}

	return true;
}
