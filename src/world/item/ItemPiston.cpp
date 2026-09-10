#include "world/item/ItemPiston.h"

ItemPiston::ItemPiston(int_t baseId) : TileItem(baseId)
{
}

int_t ItemPiston::getLevelDataForAuxValue(int_t auxValue) const
{
	(void)auxValue;
	return 7;
}
