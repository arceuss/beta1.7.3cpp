#include "world/item/ItemDye.h"

#include "world/entity/animal/Sheep.h"
#include "world/item/ItemInstance.h"
#include "world/level/Level.h"
#include "world/level/tile/CropsTile.h"
#include "world/level/tile/FlowerTile.h"
#include "world/level/tile/GrassTile.h"
#include "world/level/tile/SaplingTile.h"
#include "world/level/tile/TallGrassTile.h"
#include "world/level/tile/Tile.h"

namespace
{
	const jstring dyeColors[] = {
		u"black", u"red", u"green", u"brown",
		u"blue", u"purple", u"cyan", u"silver",
		u"gray", u"pink", u"lime", u"yellow",
		u"lightBlue", u"magenta", u"orange", u"white"
	};
}

ItemDye::ItemDye(int_t baseId) : Item(baseId)
{
	setHasSubtypes(true);
	setMaxDamage(0);
	setMaxStackSize(64);
}

int_t ItemDye::getIcon(const ItemInstance &stack) const
{
	return iconIndex + (stack.itemDamage % 8) * 16 + (stack.itemDamage / 8);
}

const jstring &ItemDye::getDyeColorName(int_t colour)
{
	return dyeColors[colour & 15];
}

jstring ItemDye::getDescriptionId(const ItemInstance &stack) const
{
	return Item::getDescriptionId() + u"." + dyeColors[stack.itemDamage & 15];
}

bool ItemDye::useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const
{
	// DyePowderItem.useOn: only bone meal (aux 15) acts on tiles, and only on the
	// three recognised targets. Everything else falls through unconsumed.
	(void)player;
	(void)face;
	if (stack.itemDamage != 15)
		return false;

	int_t targetId = level.getTile(x, y, z);
	if (targetId == Tile::sapling.id)
	{
		if (!level.isOnline)
		{
			Tile::sapling.growTree(level, x, y, z, level.random);
			stack.stackSize--;
		}
		return true;
	}
	if (targetId == Tile::crops.id)
	{
		if (!level.isOnline)
		{
			Tile::crops.fertilize(level, x, y, z);
			stack.stackSize--;
		}
		return true;
	}
	if (targetId == Tile::grass.id)
	{
		if (!level.isOnline)
		{
			stack.stackSize--;
			for (int_t attempt = 0; attempt < 128; attempt++)
			{
				int_t sx = x;
				int_t sy = y + 1;
				int_t sz = z;
				bool rejected = false;
				for (int_t step = 0; step < attempt / 16; step++)
				{
					// The reference walks inside the getTile argument list, so the
					// four draws happen in this order and the grass test reads the
					// cell below the freshly raised y.
					sx += itemRandom.nextInt(3) - 1;
					const int_t rise = itemRandom.nextInt(3) - 1;
					const int_t scale = itemRandom.nextInt(3);
					sy += rise * scale / 2;
					sz += itemRandom.nextInt(3) - 1;
					if (level.getTile(sx, sy - 1, sz) != Tile::grass.id || level.isBlockNormalCube(sx, sy, sz))
					{
						rejected = true;
						break;
					}
				}
				if (rejected)
					continue;
				if (level.getTile(sx, sy, sz) != 0)
					continue;
				if (itemRandom.nextInt(10) != 0)
				{
					level.setTileAndData(sx, sy, sz, Tile::tallGrass.id, 1);
					continue;
				}
				if (itemRandom.nextInt(3) != 0)
				{
					level.setTile(sx, sy, sz, Tile::flower.id);
					continue;
				}
				level.setTile(sx, sy, sz, Tile::rose.id);
			}
		}
		return true;
	}
	return false;
}

void ItemDye::saddleEntity(ItemInstance &stack, Mob &target) const
{
	Sheep *sheep = dynamic_cast<Sheep *>(&target);
	if (sheep != nullptr)
	{
		int_t color = ~stack.itemDamage & 15;
		if (!sheep->isSheared() && sheep->getFleeceColor() != color)
		{
			sheep->setFleeceColor(color);
			stack.stackSize--;
		}
	}
}
