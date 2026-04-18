#include "world/item/ItemTool.h"

#include "world/item/ItemInstance.h"
#include "world/level/tile/Tile.h"

namespace
{
	const ToolMaterialData WOOD_MATERIAL = {0, 59, 2.0f, 0};
	const ToolMaterialData STONE_MATERIAL = {1, 131, 4.0f, 1};
}

const ToolMaterialData &getToolMaterialData(ToolMaterialType material)
{
	switch (material)
	{
	case ToolMaterialType::WOOD:
		return WOOD_MATERIAL;
	case ToolMaterialType::STONE:
	default:
		return STONE_MATERIAL;
	}
}

ItemTool::ItemTool(int_t baseId, int_t baseDamage, ToolMaterialType material, const int_t *effectiveTiles, std::size_t effectiveTileCount)
	: Item(baseId), toolMaterial(material), effectiveTiles(effectiveTiles), effectiveTileCount(effectiveTileCount)
{
	const ToolMaterialData &toolData = getToolMaterialData(material);
	setMaxStackSize(1);
	setMaxDamage(toolData.maxUses);
	this->efficiencyOnProperMaterial = toolData.efficiencyOnProperMaterial;
	this->damageVsEntity = baseDamage + toolData.damageVsEntity;
}

float ItemTool::getDestroySpeed(const ItemInstance &stack, Tile &tile) const
{
	(void)stack;
	return isEffectiveAgainst(tile) ? efficiencyOnProperMaterial : 1.0f;
}

int_t ItemTool::getAttackDamage(const ItemInstance &stack, Entity &entity) const
{
	(void)stack;
	(void)entity;
	return damageVsEntity;
}

bool ItemTool::hurtEnemy(ItemInstance &stack, Entity &target, Entity &attacker) const
{
	(void)target;
	(void)attacker;
	stack.damageItem(2);
	return true;
}

bool ItemTool::mineBlock(ItemInstance &stack, int_t tile, int_t x, int_t y, int_t z, Entity &miner) const
{
	(void)tile;
	(void)x;
	(void)y;
	(void)z;
	(void)miner;
	stack.damageItem(1);
	return true;
}

bool ItemTool::isEffectiveAgainst(Tile &tile) const
{
	for (std::size_t i = 0; i < effectiveTileCount; ++i)
	{
		if (effectiveTiles[i] == tile.id)
			return true;
	}
	return false;
}
