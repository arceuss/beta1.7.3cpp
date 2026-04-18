#include "world/item/Items.h"

#include "world/item/Item.h"
#include "world/item/ItemAxe.h"
#include "world/item/ItemHoe.h"
#include "world/item/ItemPickaxe.h"
#include "world/item/ItemSeeds.h"
#include "world/item/ItemSpade.h"
#include "world/item/ItemSword.h"
#include "world/item/ItemSlab.h"

namespace Items
{
	Item *flintAndSteel = nullptr;
	Item *ingotIron = nullptr;
	Item *ingotGold = nullptr;
	Item *swordWood = nullptr;
	Item *shovelWood = nullptr;
	ItemPickaxe *pickaxeWood = nullptr;
	Item *axeWood = nullptr;
	Item *stick = nullptr;
	Item *hoeWood = nullptr;
	Item *swordStone = nullptr;
	Item *shovelStone = nullptr;
	ItemPickaxe *pickaxeStone = nullptr;
	Item *axeStone = nullptr;
	Item *hoeStone = nullptr;
	Item *seeds = nullptr;
	Item *wheat = nullptr;
	Item *bread = nullptr;
	Item *reed = nullptr;
	Item *coal = nullptr;
	Item *diamond = nullptr;
	Item *redstone = nullptr;
	Item *dyePowder = nullptr;
	Item *flint = nullptr;

	void initItems()
	{
		static bool initialized = false;
		if (initialized)
			return;
		initialized = true;

		flintAndSteel = new Item(3);
		flintAndSteel->setMaxStackSize(1).setMaxDamage(64).setIconIndex(5).setDescriptionId(u"item.flintAndSteel");

		ingotIron = new Item(9);
		ingotIron->setIconIndex(23).setDescriptionId(u"item.ingotIron");
		ingotGold = new Item(10);
		ingotGold->setIconIndex(54).setDescriptionId(u"item.ingotGold");


		swordWood = new ItemSword(12, ToolMaterialType::WOOD);
		swordWood->setIconIndex(64).setDescriptionId(u"item.swordWood");

		shovelWood = new ItemSpade(13, ToolMaterialType::WOOD);
		shovelWood->setIconIndex(80).setDescriptionId(u"item.shovelWood");

		pickaxeWood = new ItemPickaxe(14, ToolMaterialType::WOOD);
		pickaxeWood->setIconIndex(96).setDescriptionId(u"item.pickaxeWood");

		axeWood = new ItemAxe(15, ToolMaterialType::WOOD);
		axeWood->setIconIndex(112).setDescriptionId(u"item.hatchetWood");

		swordStone = new ItemSword(16, ToolMaterialType::STONE);
		swordStone->setIconIndex(65).setDescriptionId(u"item.swordStone");

		shovelStone = new ItemSpade(17, ToolMaterialType::STONE);
		shovelStone->setIconIndex(81).setDescriptionId(u"item.shovelStone");

		pickaxeStone = new ItemPickaxe(18, ToolMaterialType::STONE);
		pickaxeStone->setIconIndex(97).setDescriptionId(u"item.pickaxeStone");

		axeStone = new ItemAxe(19, ToolMaterialType::STONE);
		axeStone->setIconIndex(113).setDescriptionId(u"item.hatchetStone");

		stick = new Item(24);
		stick->setIconIndex(53).setDescriptionId(u"item.stick");

		hoeWood = new ItemHoe(34, ToolMaterialType::WOOD);
		hoeWood->setIconIndex(128).setDescriptionId(u"item.hoeWood");

		hoeStone = new ItemHoe(35, ToolMaterialType::STONE);
		hoeStone->setIconIndex(129).setDescriptionId(u"item.hoeStone");

		seeds = new ItemSeeds(39, 59);
		seeds->setIconIndex(9).setDescriptionId(u"item.seeds");

		wheat = new Item(40);
		wheat->setIconIndex(25).setDescriptionId(u"item.wheat");

		bread = new Item(41);
		bread->setIconIndex(41).setDescriptionId(u"item.bread");

		flint = new Item(62);
		flint->setIconIndex(6).setDescriptionId(u"item.flint");

		reed = new Item(82);
		reed->setIconIndex(27).setDescriptionId(u"item.reed");

		coal = new Item(7);
		coal->setIconIndex(7).setDescriptionId(u"item.coal");

		diamond = new Item(8);
		diamond->setIconIndex(55).setDescriptionId(u"item.diamond");

		redstone = new Item(75);
		redstone->setIconIndex(56).setDescriptionId(u"item.redstone");

		dyePowder = new Item(95);
		dyePowder->setIconIndex(78).setDescriptionId(u"item.dyePowder");

		new ItemSlab(44 - 256);
	}
}
