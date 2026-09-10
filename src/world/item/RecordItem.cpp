#include "world/item/RecordItem.h"

#include "world/item/ItemInstance.h"
#include "world/entity/player/Player.h"
#include "world/level/Level.h"
#include "world/level/tile/JukeboxTile.h"
#include "world/level/tile/Tile.h"

RecordItem::RecordItem(int_t baseId, const jstring &recordName) : Item(baseId), recordName(recordName)
{
	setMaxStackSize(1);
}

bool RecordItem::useOn(ItemInstance &stack, Player &player, Level &level, int_t x, int_t y, int_t z, Facing face) const
{
	(void)player;
	(void)face;
	if (level.getTile(x, y, z) != Tile::jukebox.id || level.getData(x, y, z) != 0)
		return false;
	// ItemRecord.useOn: the client only reports the use; the server owns the
	// insertion, the consumption and the playback broadcast.
	if (level.isOnline)
		return true;
	Tile::jukebox.insertRecord(level, x, y, z, getShiftedIndex());
	level.levelEvent(nullptr, 1005, x, y, z, getShiftedIndex());
	stack.stackSize--;
	return true;
}
