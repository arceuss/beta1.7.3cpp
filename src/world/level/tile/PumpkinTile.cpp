#include "world/level/tile/PumpkinTile.h"

#include "util/Mth.h"
#include "world/entity/player/Player.h"
#include "world/level/Level.h"
#include "world/level/material/Material.h"

PumpkinTile::PumpkinTile(int_t id, int_t tex, bool lit) : Tile(id, tex, Material::pumpkin()), lit(lit)
{
	setTicking(true);
}

int_t PumpkinTile::getTexture(Facing face, int_t data)
{
	if (face == Facing::UP || face == Facing::DOWN)
		return tex;

	int_t frontTexture = tex + 17;
	if (lit)
		frontTexture++;

	if ((data == 2 && face == Facing::NORTH) ||
		(data == 3 && face == Facing::EAST) ||
		(data == 0 && face == Facing::SOUTH) ||
		(data == 1 && face == Facing::WEST))
	{
		return frontTexture;
	}

	return tex + 16;
}

bool PumpkinTile::mayPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: canPlaceBlockAt - a replaceable ground cover target above a normal cube
	int_t tile = level.getTile(x, y, z);
	Tile *target = (tile == 0) ? nullptr : Tile::tiles[tile];
	return (target == nullptr || target->material.isGroundCover()) && level.isBlockNormalCube(x, y - 1, z);
}

void PumpkinTile::setPlacedBy(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	// b173: onBlockPlacedBy - the yaw is reduced in float precision before the
	// half-sector bias is added, exactly as the reference does
	int_t dir = Mth::floor(static_cast<double>(player.yRot * 4.0f / 360.0f) + 2.5) & 3;
	level.setData(x, y, z, dir);
}