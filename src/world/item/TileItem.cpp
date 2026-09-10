#include "world/item/TileItem.h"

#include "world/entity/player/Player.h"
#include "world/item/ItemInstance.h"
#include "world/level/Level.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/StepSound.h"
#include "world/level/tile/Tile.h"

TileItem::TileItem(int_t baseId) : Item(baseId), tileId(baseId + 256)
{
	setIconIndex(Tile::tiles[tileId]->getTexture(Facing::NORTH));
}

bool TileItem::useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const
{
	if (level.getTile(x, y, z) == Tile::snow.id)
	{
		face = Facing::DOWN;
	}
	else
	{
		if (face == Facing::DOWN) y--;
		if (face == Facing::UP) y++;
		if (face == Facing::NORTH) z--;
		if (face == Facing::SOUTH) z++;
		if (face == Facing::WEST) x--;
		if (face == Facing::EAST) x++;
	}

	if (stack.stackSize == 0)
		return false;
	if (y == 127 && Tile::tiles[tileId]->material.isSolid())
		return false;

	if (!level.mayPlace(tileId, x, y, z, false, face))
		return false;

	Tile &tile = *Tile::tiles[tileId];
	if (level.setTileAndData(x, y, z, tileId, getLevelDataForAuxValue(stack.itemDamage)))
	{
		tile.setPlacedOnFace(level, x, y, z, face);
		tile.setPlacedBy(level, x, y, z, player);
		StepSound *sound = tile.soundType;
		level.playSoundEffect(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f,
			sound->getStepResourcePath(), (sound->getVolume() + 1.0f) / 2.0f, sound->getPitch() * 0.8f);
		stack.stackSize--;
	}
	return true;
}

jstring TileItem::getDescriptionId(const ItemInstance &stack) const
{
	(void)stack;
	return Tile::tiles[tileId]->descriptionId;
}

jstring TileItem::getDescriptionId() const
{
	return Tile::tiles[tileId]->descriptionId;
}
