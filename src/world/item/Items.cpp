#include "world/item/Items.h"

#include "world/item/Item.h"
#include "world/item/ItemAxe.h"
#include "world/item/ItemHoe.h"
#include "world/item/ItemPickaxe.h"
#include "world/item/ItemSeeds.h"
#include "world/item/ItemSpade.h"
#include "world/item/ItemSword.h"
#include "world/item/ItemArmor.h"
#include "world/item/ItemFood.h"
#include "world/item/ItemSlab.h"
#include "world/item/ItemDye.h"
#include "world/item/ItemDoor.h"
#include "world/item/ItemFlintAndSteel.h"
#include "world/item/RecordItem.h"
#include "world/item/ItemSign.h"
#include "world/item/ItemRedStone.h"
#include "world/item/ItemRepeater.h"
#include "world/item/ItemMinecart.h"
#include "world/item/ItemShears.h"
#include "world/level/tile/Tile.h"

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
	Item *cookie = nullptr;
	Item *reed = nullptr;
	Item *doorWood = nullptr;
	Item *doorIron = nullptr;
	Item *minecart = nullptr;
	Item *minecartPowered = nullptr;
	Item *minecartChest = nullptr;
	Item *compass = nullptr;
	Item *clock = nullptr;
	Item *record13 = nullptr;
	Item *recordCat = nullptr;
	Item *coal = nullptr;
	Item *diamond = nullptr;
	Item *redstone = nullptr;
	Item *dyePowder = nullptr;
	Item *flint = nullptr;
	Item *leather = nullptr;
	Item *silk = nullptr;
	Item *feather = nullptr;
	Item *gunpowder = nullptr;
	Item *bowlEmpty = nullptr;
	Item *brick = nullptr;
	Item *clayItem = nullptr;
	Item *paper = nullptr;
	Item *book = nullptr;
	Item *sugar = nullptr;
	Item *glowstoneDust = nullptr;
	Item *sign = nullptr;
	Item *redstoneRepeater = nullptr;
	Item *snowball = nullptr;
	Item *slimeball = nullptr;
	Item *shears = nullptr;
	Item *swordIron = nullptr;
	Item *shovelIron = nullptr;
	ItemPickaxe *pickaxeIron = nullptr;
	Item *axeIron = nullptr;
	Item *hoeIron = nullptr;

	Item *swordDiamond = nullptr;
	Item *shovelDiamond = nullptr;
	ItemPickaxe *pickaxeDiamond = nullptr;
	Item *axeDiamond = nullptr;
	Item *hoeDiamond = nullptr;

	Item *swordGold = nullptr;
	Item *shovelGold = nullptr;
	ItemPickaxe *pickaxeGold = nullptr;
	Item *axeGold = nullptr;
	Item *hoeGold = nullptr;

	void initItems()
	{
		static bool initialized = false;
		if (initialized)
			return;
		initialized = true;

		flintAndSteel = new ItemFlintAndSteel(3);

		ingotIron = new Item(9);
		ingotIron->setIconIndex(23).setDescriptionId(u"item.ingotIron");
		ingotGold = new Item(10);
		ingotGold->setIconIndex(39).setDescriptionId(u"item.ingotGold");


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

		// Iron tools
		swordIron = new ItemSword(11, ToolMaterialType::IRON);
		swordIron->setIconIndex(66).setDescriptionId(u"item.swordIron");

		shovelIron = new ItemSpade(0, ToolMaterialType::IRON);
		shovelIron->setIconIndex(82).setDescriptionId(u"item.shovelIron");

		pickaxeIron = new ItemPickaxe(1, ToolMaterialType::IRON);
		pickaxeIron->setIconIndex(98).setDescriptionId(u"item.pickaxeIron");

		axeIron = new ItemAxe(2, ToolMaterialType::IRON);
		axeIron->setIconIndex(114).setDescriptionId(u"item.hatchetIron");

		hoeIron = new ItemHoe(36, ToolMaterialType::IRON);
		hoeIron->setIconIndex(130).setDescriptionId(u"item.hoeIron");

		// Diamond tools
		swordDiamond = new ItemSword(20, ToolMaterialType::DIAMOND);
		swordDiamond->setIconIndex(67).setDescriptionId(u"item.swordDiamond");

		shovelDiamond = new ItemSpade(21, ToolMaterialType::DIAMOND);
		shovelDiamond->setIconIndex(83).setDescriptionId(u"item.shovelDiamond");

		pickaxeDiamond = new ItemPickaxe(22, ToolMaterialType::DIAMOND);
		pickaxeDiamond->setIconIndex(99).setDescriptionId(u"item.pickaxeDiamond");

		axeDiamond = new ItemAxe(23, ToolMaterialType::DIAMOND);
		axeDiamond->setIconIndex(115).setDescriptionId(u"item.hatchetDiamond");

		hoeDiamond = new ItemHoe(37, ToolMaterialType::DIAMOND);
		hoeDiamond->setIconIndex(131).setDescriptionId(u"item.hoeDiamond");

		// Gold tools
		swordGold = new ItemSword(27, ToolMaterialType::GOLD);
		swordGold->setIconIndex(68).setDescriptionId(u"item.swordGold");

		shovelGold = new ItemSpade(28, ToolMaterialType::GOLD);
		shovelGold->setIconIndex(84).setDescriptionId(u"item.shovelGold");

		pickaxeGold = new ItemPickaxe(29, ToolMaterialType::GOLD);
		pickaxeGold->setIconIndex(100).setDescriptionId(u"item.pickaxeGold");

		axeGold = new ItemAxe(30, ToolMaterialType::GOLD);
		axeGold->setIconIndex(116).setDescriptionId(u"item.hatchetGold");

		hoeGold = new ItemHoe(38, ToolMaterialType::GOLD);
		hoeGold->setIconIndex(132).setDescriptionId(u"item.hoeGold");

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

		bread = new ItemFood(41, 5, false);
		bread->setIconIndex(41).setDescriptionId(u"item.bread");

		flint = new Item(62);
		flint->setIconIndex(6).setDescriptionId(u"item.flint");

		reed = new Item(82);
		reed->setIconIndex(27).setDescriptionId(u"item.reed");

		doorWood = new ItemDoor(68, Tile::doorWood);
		doorWood->setIconIndex(43).setDescriptionId(u"item.doorWood");

		doorIron = new ItemDoor(74, Tile::doorIron);
		doorIron->setIconIndex(44).setDescriptionId(u"item.doorIron");

		minecart = new ItemMinecart(72, 0);
		minecart->setIconIndex(135).setDescriptionId(u"item.minecart");

		minecartPowered = new ItemMinecart(87, 2);
		minecartPowered->setIconIndex(167).setDescriptionId(u"item.minecartPowered");

		minecartChest = new ItemMinecart(86, 1);
		minecartChest->setIconIndex(151).setDescriptionId(u"item.minecartChest");

		compass = new Item(89);
		compass->setIconIndex(54).setDescriptionId(u"item.compass");

		clock = new Item(91);
		clock->setIconIndex(70).setDescriptionId(u"item.clock");

		record13 = new RecordItem(2000, u"13");
		record13->setIconIndex(240).setDescriptionId(u"item.record");

		recordCat = new RecordItem(2001, u"cat");
		recordCat->setIconIndex(241).setDescriptionId(u"item.record");

		coal = new Item(7);
		coal->setIconIndex(7).setDescriptionId(u"item.coal");

		diamond = new Item(8);
		diamond->setIconIndex(55).setDescriptionId(u"item.diamond");

		redstone = new ItemRedStone(75);
		redstone->setIconIndex(56).setDescriptionId(u"item.redstone");

		dyePowder = new ItemDye(95);
		dyePowder->setIconIndex(78);

		leather = new Item(78);
		leather->setIconIndex(103).setDescriptionId(u"item.leather");

		silk = new Item(31);
		silk->setIconIndex(8).setDescriptionId(u"item.string");

		feather = new Item(32);
		feather->setIconIndex(24).setDescriptionId(u"item.feather");

		gunpowder = new Item(33);
		gunpowder->setIconIndex(40).setDescriptionId(u"item.sulphur");

		bowlEmpty = new Item(25);
		bowlEmpty->setIconIndex(71).setDescriptionId(u"item.bowl");

		brick = new Item(80);
		brick->setIconIndex(22).setDescriptionId(u"item.brick");

		clayItem = new Item(81);
		clayItem->setIconIndex(57).setDescriptionId(u"item.clay");

		paper = new Item(83);
		paper->setIconIndex(58).setDescriptionId(u"item.paper");

		book = new Item(84);
		book->setIconIndex(59).setDescriptionId(u"item.book");

		sugar = new Item(97);
		sugar->setIconIndex(13).setDescriptionId(u"item.sugar");

		glowstoneDust = new Item(92);
		glowstoneDust->setIconIndex(73).setDescriptionId(u"item.yellowDust");

		redstoneRepeater = new ItemRepeater(100);
		redstoneRepeater->setIconIndex(86).setDescriptionId(u"item.diode");

		cookie = new ItemFood(101, 1, false);
		cookie->setMaxStackSize(8).setIconIndex(92).setDescriptionId(u"item.cookie");

		snowball = new Item(76);
		snowball->setIconIndex(14).setDescriptionId(u"item.snowball");

		slimeball = new Item(85);
		slimeball->setIconIndex(30).setDescriptionId(u"item.slimeball");

		shears = new ItemShears(103);
		shears->setIconIndex(93).setDescriptionId(u"item.shears");

		sign = new ItemSign(67);
		sign->setIconIndex(42).setDescriptionId(u"item.sign");

		new ItemSlab(44 - 256);
	}
}
