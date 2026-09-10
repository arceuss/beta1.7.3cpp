#include "world/item/ItemSaddle.h"

#include "world/entity/Mob.h"
#include "world/entity/animal/Pig.h"
#include "world/item/ItemInstance.h"

ItemSaddle::ItemSaddle(int_t id) : Item(id)
{
	setMaxStackSize(1);
}

void ItemSaddle::saddleEntity(ItemInstance &stack, Mob &target) const
{
	Pig *pig = dynamic_cast<Pig *>(&target);
	if (pig != nullptr && !pig->isSaddled())
	{
		pig->setSaddled(true);
		stack.stackSize--;
	}
}

bool ItemSaddle::hurtEnemy(ItemInstance &stack, Entity &target, Entity &attacker) const
{
	// SaddleItem.hurtEnemy saddles the victim and always reports the use.
	(void)attacker;
	if (Mob *mob = dynamic_cast<Mob *>(&target))
		saddleEntity(stack, *mob);
	return true;
}
