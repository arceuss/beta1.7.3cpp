#include "world/item/ItemSoup.h"

#include "world/entity/player/Player.h"
#include "world/item/ItemInstance.h"
#include "world/item/Items.h"
#include "world/level/Level.h"

ItemSoup::ItemSoup(int_t baseId, int_t healAmount)
	: ItemFood(baseId, healAmount, false)
{
}

void ItemSoup::use(ItemInstance &stack, Level &level, Player &player) const
{
	// ItemSoup.use eats through ItemFood and then replaces the whole stack with a
	// single bowl, whatever count was left.
	ItemFood::use(stack, level, player);
	stack = ItemInstance(Items::bowlEmpty->getShiftedIndex(), 1, 0);
}
