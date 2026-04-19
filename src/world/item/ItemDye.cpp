#include "world/item/ItemDye.h"

#include "world/item/ItemInstance.h"

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
	setMaxDamage(0);
	setMaxStackSize(64);
}

int_t ItemDye::getIcon(const ItemInstance &stack) const
{
	return iconIndex + (stack.itemDamage % 8) * 16 + (stack.itemDamage / 8);
}

jstring ItemDye::getDescriptionId(const ItemInstance &stack) const
{
	int_t dye = stack.itemDamage & 15;
	return u"item.dyePowder." + dyeColors[dye];
}