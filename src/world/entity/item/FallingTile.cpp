#include "world/entity/item/FallingTile.h"

#include "nbt/CompoundTag.h"
#include "util/Mth.h"
#include "world/item/ItemInstance.h"
#include "world/level/Level.h"
#include "world/level/tile/SandTile.h"

FallingTile::FallingTile(Level &level) : Entity(level)
{
	makeStepSound = false;
}

FallingTile::FallingTile(Level &level, double x, double y, double z, int_t tile) : FallingTile(level)
{
	this->tile = tile;
	blocksBuilding = true;
	setSize(0.98f, 0.98f);
	heightOffset = bbHeight / 2.0f;
	setPos(x, y, z);
	xd = 0.0;
	yd = 0.0;
	zd = 0.0;
	xo = x;
	yo = y;
	zo = z;
}

bool FallingTile::isPickable()
{
	return !removed;
}

void FallingTile::tick()
{
	if (tile == 0)
	{
		remove();
		return;
	}

	xo = x;
	yo = y;
	zo = z;
	time++;
	yd -= 0.04f;
	move(xd, yd, zd);
	xd *= 0.98f;
	yd *= 0.98f;
	zd *= 0.98f;

	// The source cell is cleared after the move, from the post-move coordinates, and the
	// same coordinates decide the landing.
	int_t xTile = Mth::floor(x);
	int_t yTile = Mth::floor(y);
	int_t zTile = Mth::floor(z);
	if (level.getTile(xTile, yTile, zTile) == tile)
		level.setTile(xTile, yTile, zTile, 0);

	if (onGround)
	{
		xd *= 0.7f;
		zd *= 0.7f;
		yd *= -0.5;
		remove();

		// Entities are ignored for the placement test, the cell below must not be free,
		// and the write itself may still fail. Short-circuit order matters: setTile only
		// runs once the first two conditions hold.
		bool placed = level.mayPlace(tile, xTile, yTile, zTile, true, Facing::UP)
			&& !SandTile::isFree(level, xTile, yTile - 1, zTile)
			&& level.setTile(xTile, yTile, zTile, tile);
		if (!placed && !level.isOnline)
			spawnAtLocation(ItemInstance(tile, 1, 0), 0.0f);
	}
	else if (time > 100 && !level.isOnline)
	{
		spawnAtLocation(ItemInstance(tile, 1, 0), 0.0f);
		remove();
	}
}

void FallingTile::addAdditionalSaveData(CompoundTag &tag)
{
	tag.putByte(u"Tile", static_cast<byte_t>(tile));
}

void FallingTile::readAdditionalSaveData(CompoundTag &tag)
{
	tile = tag.getByte(u"Tile") & 255;
}

float FallingTile::getShadowHeightOffs()
{
	return 0.0f;
}
