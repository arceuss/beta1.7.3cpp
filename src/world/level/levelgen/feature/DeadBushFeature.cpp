#include "world/level/levelgen/feature/DeadBushFeature.h"

#include "world/level/Level.h"
#include "world/level/tile/LeafTile.h"

DeadBushFeature::DeadBushFeature(int_t deadBushId) : deadBushId(deadBushId) {}

bool DeadBushFeature::place(Level &level, Random &random, int_t x, int_t y, int_t z) {
	while (y > 0) {
		int_t tile = level.getTile(x, y, z);
		if (tile != 0 && tile != Tile::leaves.id)
			break;
		--y;
	}

	for (int_t i = 0; i < 4; ++i) {
		int_t xx = x + random.nextInt(8) - random.nextInt(8);
		int_t yy = y + random.nextInt(4) - random.nextInt(4);
		int_t zz = z + random.nextInt(8) - random.nextInt(8);
		if (level.isEmptyTile(xx, yy, zz))
			level.setTileNoUpdate(xx, yy, zz, deadBushId);
	}

	return true;
}
