#include "tools/BlockSmoke.h"

#include <cmath>
#include <iostream>
#include <vector>
#include "client/particle/TerrainParticle.h"
#include "client/particle/NoteParticle.h"
#include "client/renderer/TileRenderer.h"
#include "world/item/Items.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/entity/player/Player.h"
#include "world/level/Level.h"
#include "world/phys/Vec3.h"
#include "util/Mth.h"
#include "world/level/LevelListener.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/RedStoneDustTile.h"
#include "world/level/tile/LeverTile.h"
#include "world/level/tile/ButtonTile.h"
#include "world/level/tile/PressurePlateTile.h"
#include "world/level/tile/NotGateTile.h"
#include "world/level/tile/DoorTile.h"
#include "world/level/tile/NoteTile.h"
#include "world/level/tile/DispenserTile.h"
#include "world/level/tile/entity/DispenserTileEntity.h"
#include "world/level/tile/entity/SignTileEntity.h"
#include "nbt/CompoundTag.h"
#include "world/level/tile/entity/NoteTileEntity.h"
#include "java/Random.h"
#include "java/File.h"

namespace
{
	struct CaptureListener : public LevelListener
	{
		std::vector<jstring> sounds;
		std::vector<jstring> particles;
		std::vector<jstring> streams;

		void tileChanged(int_t, int_t, int_t) override {}
		void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
		void allChanged() override {}
		void playSound(const jstring &name, double, double, double, float, float) override { sounds.push_back(name); }
		void addParticle(const jstring &name, double, double, double, double, double, double) override { particles.push_back(name); }
		void playMusic(const jstring &, double, double, double, float) override {}
		void entityAdded(std::shared_ptr<Entity>) override {}
		void entityRemoved(std::shared_ptr<Entity>) override {}
		void skyColorChanged() override {}
		void playStreamingMusic(const jstring &name, int_t, int_t, int_t) override { streams.push_back(name); }
		void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}

		void clear()
		{
			sounds.clear();
			particles.clear();
			streams.clear();
		}
	};

	struct InspectableTerrainParticle : public TerrainParticle
	{
		using TerrainParticle::TerrainParticle;
		int_t textureIndex() const { return tex; }
	};

struct InspectableNoteParticle : public NoteParticle
	{
		using NoteParticle::NoteParticle;
		float currentSize() const { return size; }
	};

	bool expect(bool condition, const char *message)
	{
		if (!condition)
		{
			std::cerr << "FAIL: " << message << std::endl;
			return false;
		}
		return true;
	}

	bool expectNear(double actual, double expected, double epsilon, const char *message)
	{
		return expect(std::abs(actual - expected) <= epsilon, message);
	}
}

int runBlockSmoke()
{
	try
	{
		std::cerr << "block-smoke: init" << std::endl;
		Tile::initTiles();
		Items::initItems();

		bool ok = true;
		Random random;
		std::cerr << "block-smoke: static assertions" << std::endl;
		ok &= expect(Tile::lightBlock[65] == 0, "ladder should stay transparent for light sampling like beta");
		ok &= expect(Tile::lightBlock[53] == 255, "wood stairs should block light like beta");
		ok &= expect(Tile::lightBlock[67] == 255, "stone stairs should block light like beta");
		ok &= expect(Tile::lightBlock[96] == 0, "trapdoor should not block light");
		ok &= expect(Tile::lightBlock[63] == 0, "sign post should not block light");
		ok &= expect(Tile::lightBlock[68] == 0, "wall sign should not block light");
		ok &= expect(Items::sign->getShiftedIndex() == 323, "sign item should use the beta shifted id 323");
		ok &= expect(ItemInstance(Items::sign->getShiftedIndex(), 1, 0).getIcon() == 42, "sign item should use the beta sign icon");
		ok &= expect(Tile::tiles[30]->getResource(0, random) == Items::silk->getShiftedIndex(), "cobweb should drop string");
		ok &= expect(Tile::tiles[89]->getResource(0, random) == Items::glowstoneDust->getShiftedIndex(), "glowstone should drop glowstone dust");
		ok &= expect(Tile::tiles[80]->getResource(0, random) == Items::snowball->getShiftedIndex(), "snow block should drop snowballs");
		ok &= expect(Tile::tiles[64]->getResource(0, random) == Items::doorWood->getShiftedIndex(), "wood door bottom half should drop wood door item");
		ok &= expect(Tile::tiles[64]->getResource(8, random) == 0, "wood door top half should not drop an item");

		Vec3::resetPool();
		Vec3 *xRotVec = Vec3::newTemp(0.0, 1.0, 0.0);
		xRotVec->xRot(Mth::PI * 0.5f);
		ok &= expectNear(xRotVec->x, 0.0, 1.0e-6, "Vec3 xRot should preserve x");
		ok &= expectNear(xRotVec->y, 0.0, 1.0e-6, "Vec3 xRot should rotate y to zero at 90 degrees");
		ok &= expectNear(xRotVec->z, -1.0, 1.0e-6, "Vec3 xRot should rotate +Y to -Z");
		Vec3 *yRotVec = Vec3::newTemp(1.0, 0.0, 0.0);
		yRotVec->yRot(Mth::PI * 0.5f);
		ok &= expectNear(yRotVec->x, 0.0, 1.0e-6, "Vec3 yRot should rotate x to zero at 90 degrees");
		ok &= expectNear(yRotVec->y, 0.0, 1.0e-6, "Vec3 yRot should preserve y");
		ok &= expectNear(yRotVec->z, -1.0, 1.0e-6, "Vec3 yRot should rotate +X to -Z");
		Vec3 *zRotVec = Vec3::newTemp(1.0, 0.0, 0.0);
		zRotVec->zRot(Mth::PI * 0.5f);
		ok &= expectNear(zRotVec->x, 0.0, 1.0e-6, "Vec3 zRot should rotate x to zero at 90 degrees");
		ok &= expectNear(zRotVec->y, -1.0, 1.0e-6, "Vec3 zRot should rotate +X to -Y");
		ok &= expectNear(zRotVec->z, 0.0, 1.0e-6, "Vec3 zRot should preserve z");
		ok &= expect(Tile::redstoneWire.getTexture(Facing::NORTH, 0) == 164, "redstone wire should use the b173 texture index");
		ok &= expect(Tile::redstoneWire.getTexture(Facing::NORTH, 1) == 164, "powered redstone wire should keep the same tinted texture");
		ok &= expect(Tile::lever.tex == 96, "lever should use the beta lever texture");
		ok &= expect(Tile::lightEmission[75] == 0, "redstone torch off should not emit light");
		ok &= expect(Tile::lightEmission[76] == 7, "redstone torch on should emit beta redstone torch light");
		ok &= expect(Tile::buttonStone.getRenderShape() == Tile::SHAPE_BLOCK, "stone button should render as a bounded block shape");
		ok &= expect(Tile::pressurePlateStone.getRenderShape() == Tile::SHAPE_BLOCK, "pressure plate should render as a bounded block shape");
		ok &= expect(!TileRenderer::canRender(Tile::SHAPE_RED_DUST), "redstone wire item should use flat icon rendering");
		ok &= expect(!TileRenderer::canRender(Tile::SHAPE_LEVER), "lever item should use flat icon rendering");
		ok &= expect(Tile::lightBlock[55] == 0, "redstone wire should not block light");
		ok &= expect(Tile::lightBlock[69] == 0, "lever should not block light");
		ok &= expect(Tile::lightBlock[70] == 0, "stone pressure plate should not block light");
		ok &= expect(Tile::lightBlock[72] == 0, "wood pressure plate should not block light");
		ok &= expect(Tile::lightBlock[75] == 0, "redstone torch off should not block light");
		ok &= expect(Tile::lightBlock[76] == 0, "redstone torch on should not block light");
		ok &= expect(Tile::lightBlock[77] == 0, "stone button should not block light");
		ok &= expect(ItemInstance(Tile::lever.id, 1, 0).getIcon() == 96, "lever block item should use lever texture icon");
		ok &= expect(Tile::torchRedstoneIdle.descriptionId == u"tile.notGate", "redstone torch off should use localized notGate key");
		ok &= expect(Tile::torchRedstoneActive.descriptionId == u"tile.notGate", "redstone torch on should use localized notGate key");
		std::cerr << "block-smoke: level setup" << std::endl;
		Level level(File::open(u"build/block-smoke-workdir"), u"block-smoke-world", 12345);
		CaptureListener listener;
		level.addListener(listener);
		Player player(level);
		player.yRot = 0.0f;
		int_t baseY = 80;

		std::cerr << "block-smoke: lighting" << std::endl;
		level.setTile(10, baseY, 0, 1);
		ok &= expect(level.setTile(10, baseY + 1, 0, 53), "wood stair should place");
		level.setTile(10, baseY + 2, 0, 1);
		int_t eastBrightness = level.getRawBrightness(11, baseY + 1, 0, false);
		ok &= expect(level.getRawBrightness(10, baseY + 1, 0) == eastBrightness, "stairs should sample neighboring brightness like beta");
		level.setTile(12, baseY + 1, 0, 1);
		ok &= expect(level.setTile(12, baseY + 1, 1, 65), "ladder should place");
		level.setData(12, baseY + 1, 1, 3);
		int_t frontBrightness = level.getRawBrightness(12, baseY + 1, 2, false);
		ok &= expect(level.getRawBrightness(12, baseY + 1, 1) == frontBrightness, "ladder should stay transparent for brightness sampling");


		level.setTile(20, baseY + 1, 0, 0);
		std::cerr << "block-smoke: lever helper connectivity" << std::endl;
		level.setTile(18, baseY, 0, 1);
		ok &= expect(level.setTile(18, baseY + 1, 0, Tile::lever.id), "lever helper source should place");
		level.setData(18, baseY + 1, 0, 5 | 8);
		ok &= expect(RedStoneDustTile::isPowerProviderOrWire(level, 18, baseY + 1, 0, 5), "dust helper should treat powered lever as a power provider");
		
		std::cerr << "block-smoke: redstone placement" << std::endl;
		level.setTile(20, baseY, 0, 1);
		ItemInstance redstoneDust(Items::redstone->getShiftedIndex(), 1, 0);
		ok &= expect(redstoneDust.useOn(player, level, 20, baseY, 0, Facing::UP), "redstone item should be usable on a block top face");
		ok &= expect(level.getTile(20, baseY + 1, 0) == Tile::redstoneWire.id, "redstone item should place redstone wire");
		ok &= expect(redstoneDust.stackSize == 0, "redstone item should consume one dust on placement");

		std::cerr << "block-smoke: live lever wire growth" << std::endl;
		for (int_t x = 60; x <= 63; ++x)
			level.setTile(x, baseY, 0, 1);
		ok &= expect(level.setTile(60, baseY + 1, 0, Tile::lever.id), "live-growth lever source should place");
		level.setData(60, baseY + 1, 0, 5);
		ok &= expect(Tile::lever.use(level, 60, baseY + 1, 0, player), "live-growth lever source should toggle on");
		for (int_t x = 61; x <= 63; ++x)
		{
			ItemInstance growthDust(Items::redstone->getShiftedIndex(), 1, 0);
			ok &= expect(growthDust.useOn(player, level, x, baseY, 0, Facing::UP), "live-growth dust should place on a powered line");
			ok &= expect(level.getData(61, baseY + 1, 0) == 15, "live-growth first wire should stay powered");
			if (x >= 62)
				ok &= expect(level.getData(62, baseY + 1, 0) == 14, "live-growth second wire should power when added");
			if (x >= 63)
				ok &= expect(level.getData(63, baseY + 1, 0) == 13, "live-growth third wire should power when added");
		}

		std::cerr << "block-smoke: live corner wire growth" << std::endl;
		level.setTile(70, baseY, 0, 1);
		level.setTile(71, baseY, 0, 1);
		level.setTile(72, baseY, 0, 1);
		level.setTile(72, baseY, 1, 1);
		level.setTile(72, baseY, 2, 1);
		ok &= expect(level.setTile(70, baseY + 1, 0, Tile::lever.id), "live-corner lever source should place");
		level.setData(70, baseY + 1, 0, 5);
		ok &= expect(Tile::lever.use(level, 70, baseY + 1, 0, player), "live-corner lever source should toggle on");
		{
			ItemInstance firstDust(Items::redstone->getShiftedIndex(), 1, 0);
			ok &= expect(firstDust.useOn(player, level, 71, baseY, 0, Facing::UP), "live-corner first dust should place");
			ok &= expect(level.getData(71, baseY + 1, 0) == 15, "live-corner first wire should power at strength 15");
		}
		{
			ItemInstance secondDust(Items::redstone->getShiftedIndex(), 1, 0);
			ok &= expect(secondDust.useOn(player, level, 72, baseY, 0, Facing::UP), "live-corner second dust should place");
			ok &= expect(level.getData(71, baseY + 1, 0) == 15, "live-corner first wire should stay powered before the turn");
			ok &= expect(level.getData(72, baseY + 1, 0) == 14, "live-corner second wire should power at strength 14");
		}
		{
			ItemInstance cornerDust(Items::redstone->getShiftedIndex(), 1, 0);
			ok &= expect(cornerDust.useOn(player, level, 72, baseY, 1, Facing::UP), "live-corner turn dust should place");
			ok &= expect(level.getData(71, baseY + 1, 0) == 15, "live-corner first wire should stay powered after the turn");
			ok &= expect(level.getData(72, baseY + 1, 0) == 14, "live-corner second wire should stay powered after the turn");
			ok &= expect(level.getData(72, baseY + 1, 1) == 13, "live-corner turn dust should power at strength 13");
		}
		{
			ItemInstance tailDust(Items::redstone->getShiftedIndex(), 1, 0);
			ok &= expect(tailDust.useOn(player, level, 72, baseY, 2, Facing::UP), "live-corner tail dust should place");
			ok &= expect(level.getData(72, baseY + 1, 1) == 13, "live-corner turn dust should stay powered when extended");
			ok &= expect(level.getData(72, baseY + 1, 2) == 12, "live-corner tail dust should power at strength 12");
		}

		std::cerr << "block-smoke: corner wire propagation" << std::endl;
		level.setTile(80, baseY, 0, 1);
		level.setTile(81, baseY, 0, 1);
		level.setTile(82, baseY, 0, 1);
		level.setTile(82, baseY, 1, 1);
		level.setTile(82, baseY, 2, 1);
		ok &= expect(level.setTile(80, baseY + 1, 0, Tile::lever.id), "corner lever source should place");
		level.setData(80, baseY + 1, 0, 5);
		ok &= expect(level.setTile(81, baseY + 1, 0, Tile::redstoneWire.id), "corner first wire should place");
		ok &= expect(level.setTile(82, baseY + 1, 0, Tile::redstoneWire.id), "corner second wire should place");
		ok &= expect(level.setTile(82, baseY + 1, 1, Tile::redstoneWire.id), "corner turn dust should place");
		ok &= expect(level.setTile(82, baseY + 1, 2, Tile::redstoneWire.id), "corner tail dust should place");
		ok &= expect(Tile::lever.use(level, 80, baseY + 1, 0, player), "corner lever source should toggle on");
		ok &= expect(level.getData(81, baseY + 1, 0) == 15, "corner first wire should power at strength 15");
		ok &= expect(level.getData(82, baseY + 1, 0) == 14, "corner second wire should power at strength 14");
		ok &= expect(level.getData(82, baseY + 1, 1) == 13, "corner turn dust should power at strength 13");
		ok &= expect(level.getData(82, baseY + 1, 2) == 12, "corner tail dust should power at strength 12");

		std::cerr << "block-smoke: long corner wire range" << std::endl;
		for (int_t x = 90; x <= 98; ++x)
			level.setTile(x, baseY, 0, 1);
		for (int_t z = 1; z <= 7; ++z)
			level.setTile(98, baseY, z, 1);
		ok &= expect(level.setTile(90, baseY + 1, 0, Tile::lever.id), "long-corner lever source should place");
		level.setData(90, baseY + 1, 0, 5);
		for (int_t x = 91; x <= 98; ++x)
			ok &= expect(level.setTile(x, baseY + 1, 0, Tile::redstoneWire.id), "long-corner horizontal wire should place");
		for (int_t z = 1; z <= 7; ++z)
			ok &= expect(level.setTile(98, baseY + 1, z, Tile::redstoneWire.id), "long-corner vertical wire should place");
		ok &= expect(Tile::lever.use(level, 90, baseY + 1, 0, player), "long-corner lever source should toggle on");
		ok &= expect(level.getData(91, baseY + 1, 0) == 15, "long-corner first wire should power at strength 15");
		ok &= expect(level.getData(98, baseY + 1, 0) == 8, "long-corner last horizontal wire should power at strength 8");
		ok &= expect(level.getData(98, baseY + 1, 1) == 7, "long-corner first turned wire should power at strength 7");
		ok &= expect(level.getData(98, baseY + 1, 6) == 2, "long-corner near-tail wire should power at strength 2");
		ok &= expect(level.getData(98, baseY + 1, 7) == 1, "long-corner tail wire should power at strength 1");

		std::cerr << "block-smoke: torch long corner wire range" << std::endl;
		for (int_t x = 110; x <= 118; ++x)
			level.setTile(x, baseY, 0, 1);
		for (int_t z = 1; z <= 7; ++z)
			level.setTile(118, baseY, z, 1);
		ok &= expect(level.setTileAndData(110, baseY + 1, 0, Tile::torchRedstoneActive.id, 5), "torch long-corner source should place");
		for (int_t x = 111; x <= 118; ++x)
			ok &= expect(level.setTile(x, baseY + 1, 0, Tile::redstoneWire.id), "torch long-corner horizontal wire should place");
		for (int_t z = 1; z <= 7; ++z)
			ok &= expect(level.setTile(118, baseY + 1, z, Tile::redstoneWire.id), "torch long-corner vertical wire should place");
		ok &= expect(level.getData(111, baseY + 1, 0) == 15, "torch long-corner first wire should power at strength 15");
		ok &= expect(level.getData(118, baseY + 1, 0) == 8, "torch long-corner last horizontal wire should power at strength 8");
		ok &= expect(level.getData(118, baseY + 1, 1) == 7, "torch long-corner first turned wire should power at strength 7");
		ok &= expect(level.getData(118, baseY + 1, 6) == 2, "torch long-corner near-tail wire should power at strength 2");
		ok &= expect(level.getData(118, baseY + 1, 7) == 1, "torch long-corner tail wire should power at strength 1");

		std::cerr << "block-smoke: long straight wire range" << std::endl;
		for (int_t x = 150; x <= 160; ++x)
			level.setTile(x, baseY, 0, 1);
		ok &= expect(level.setTile(150, baseY + 1, 0, Tile::lever.id), "long-straight lever source should place");
		level.setData(150, baseY + 1, 0, 5);
		for (int_t x = 151; x <= 160; ++x)
			ok &= expect(level.setTile(x, baseY + 1, 0, Tile::redstoneWire.id), "long-straight wire should place");
		ok &= expect(Tile::lever.use(level, 150, baseY + 1, 0, player), "long-straight lever source should toggle on");
		ok &= expect(level.getData(151, baseY + 1, 0) == 15, "long-straight first wire should power at strength 15");
		ok &= expect(level.getData(152, baseY + 1, 0) == 14, "long-straight second wire should power at strength 14");
		ok &= expect(level.getData(157, baseY + 1, 0) == 9, "long-straight mid wire should power at strength 9");
		ok &= expect(level.getData(160, baseY + 1, 0) == 6, "long-straight tail wire should power at strength 6");
		ok &= expect(Tile::lever.use(level, 150, baseY + 1, 0, player), "long-straight lever source should toggle off");
		ok &= expect(level.getData(151, baseY + 1, 0) == 0, "long-straight first wire should depower when switched off");
		ok &= expect(level.getData(152, baseY + 1, 0) == 0, "long-straight second wire should depower when switched off");
		ok &= expect(level.getData(157, baseY + 1, 0) == 0, "long-straight mid wire should depower when switched off");
		ok &= expect(level.getData(160, baseY + 1, 0) == 0, "long-straight tail wire should depower when switched off");
		
		std::cerr << "block-smoke: ascending wire step" << std::endl;
		for (int_t x = 130; x <= 133; ++x)
			level.setTile(x, baseY, 0, 1);
		level.setTile(132, baseY + 1, 0, 1);
		level.setTile(133, baseY + 1, 0, 1);
		ok &= expect(level.setTile(130, baseY + 1, 0, Tile::lever.id), "ascending-step lever source should place");
		level.setData(130, baseY + 1, 0, 5);
		ok &= expect(level.setTile(131, baseY + 1, 0, Tile::redstoneWire.id), "ascending-step lower wire should place");
		ok &= expect(level.setTile(132, baseY + 2, 0, Tile::redstoneWire.id), "ascending-step raised wire should place");
		ok &= expect(level.setTile(133, baseY + 2, 0, Tile::redstoneWire.id), "ascending-step raised tail wire should place");
		ok &= expect(Tile::lever.use(level, 130, baseY + 1, 0, player), "ascending-step lever source should toggle on");
		ok &= expect(level.getData(131, baseY + 1, 0) == 15, "ascending-step lower wire should power at strength 15");
		ok &= expect(level.getData(132, baseY + 2, 0) == 14, "ascending-step raised wire should power at strength 14");
		ok &= expect(level.getData(133, baseY + 2, 0) == 13, "ascending-step raised tail wire should power at strength 13");
		
		std::cerr << "block-smoke: descending wire step" << std::endl;
		level.setTile(140, baseY + 1, 0, 1);
		level.setTile(141, baseY + 1, 0, 1);
		level.setTile(142, baseY, 0, 1);
		level.setTile(143, baseY, 0, 1);
		ok &= expect(level.setTileAndData(140, baseY + 2, 0, Tile::torchRedstoneActive.id, 5), "descending-step torch source should place");
		ok &= expect(level.setTile(141, baseY + 2, 0, Tile::redstoneWire.id), "descending-step raised wire should place");
		ok &= expect(level.setTile(142, baseY + 1, 0, Tile::redstoneWire.id), "descending-step lower wire should place");
		ok &= expect(level.setTile(143, baseY + 1, 0, Tile::redstoneWire.id), "descending-step lower tail wire should place");
		ok &= expect(level.getData(141, baseY + 2, 0) == 15, "descending-step raised wire should power at strength 15");
		ok &= expect(level.getData(142, baseY + 1, 0) == 14, "descending-step lower wire should power at strength 14");
		ok &= expect(level.getData(143, baseY + 1, 0) == 13, "descending-step lower tail wire should power at strength 13");
		
		std::cerr << "block-smoke: lever wire propagation" << std::endl;
		for (int_t x = 32; x <= 36; ++x)
			level.setTile(x, baseY, 0, 1);
		ok &= expect(level.setTile(32, baseY + 1, 0, Tile::lever.id), "lever source should place");
		level.setData(32, baseY + 1, 0, 5);
		for (int_t x = 33; x <= 36; ++x)
			ok &= expect(level.setTile(x, baseY + 1, 0, Tile::redstoneWire.id), "lever test wire should place");
		ok &= expect(Tile::lever.use(level, 32, baseY + 1, 0, player), "lever source should toggle on");
		ok &= expect(level.getData(33, baseY + 1, 0) == 15, "lever should power first wire at strength 15");
		ok &= expect(level.getData(34, baseY + 1, 0) == 14, "lever should power second wire at strength 14");
		ok &= expect(level.getData(35, baseY + 1, 0) == 13, "lever should power third wire at strength 13");
		ok &= expect(level.getData(36, baseY + 1, 0) == 12, "lever should power fourth wire at strength 12");
		ok &= expect(Tile::lever.use(level, 32, baseY + 1, 0, player), "lever source should toggle off");
		ok &= expect(level.getData(33, baseY + 1, 0) == 0, "lever should depower first wire when switched off");
		ok &= expect(level.getData(34, baseY + 1, 0) == 0, "lever should depower second wire when switched off");
		ok &= expect(level.getData(35, baseY + 1, 0) == 0, "lever should depower third wire when switched off");
		ok &= expect(level.getData(36, baseY + 1, 0) == 0, "lever should depower fourth wire when switched off");

		std::cerr << "block-smoke: wall lever block wire range" << std::endl;
		for (int_t x = 190; x <= 200; ++x)
			level.setTile(x, baseY + 1, 0, 1);
		ok &= expect(level.setTile(189, baseY + 1, 0, Tile::lever.id), "wall-lever source should place");
		level.setData(189, baseY + 1, 0, 2);
		for (int_t x = 190; x <= 200; ++x)
			ok &= expect(level.setTile(x, baseY + 2, 0, Tile::redstoneWire.id), "wall-lever wire should place");
		ok &= expect(Tile::lever.use(level, 189, baseY + 1, 0, player), "wall-lever source should toggle on");
		ok &= expect(level.getData(190, baseY + 2, 0) == 15, "wall-lever first wire should power at strength 15");
		ok &= expect(level.getData(191, baseY + 2, 0) == 14, "wall-lever second wire should power at strength 14");
		ok &= expect(level.getData(196, baseY + 2, 0) == 9, "wall-lever mid wire should power at strength 9");
		ok &= expect(level.getData(200, baseY + 2, 0) == 5, "wall-lever tail wire should power at strength 5");
		ok &= expect(Tile::lever.use(level, 189, baseY + 1, 0, player), "wall-lever source should toggle off");
		ok &= expect(level.getData(190, baseY + 2, 0) == 0, "wall-lever first wire should depower when switched off");
		ok &= expect(level.getData(191, baseY + 2, 0) == 0, "wall-lever second wire should depower when switched off");
		ok &= expect(level.getData(196, baseY + 2, 0) == 0, "wall-lever mid wire should depower when switched off");
		ok &= expect(level.getData(200, baseY + 2, 0) == 0, "wall-lever tail wire should depower when switched off");
		
		std::cerr << "block-smoke: torch wire propagation" << std::endl;
		for (int_t x = 40; x <= 44; ++x)
			level.setTile(x, baseY, 0, 1);
		ok &= expect(level.setTileAndData(40, baseY + 1, 0, Tile::torchRedstoneActive.id, 5), "redstone torch source should place");
		for (int_t x = 41; x <= 44; ++x)
		{
			ItemInstance lineDust(Items::redstone->getShiftedIndex(), 1, 0);
			ok &= expect(lineDust.useOn(player, level, x, baseY, 0, Facing::UP), "torch test wire should place");
		}
		ok &= expect(level.getData(41, baseY + 1, 0) == 15, "torch should power first wire at strength 15");
		ok &= expect(level.getData(42, baseY + 1, 0) == 14, "torch should power second wire at strength 14");
		ok &= expect(level.getData(43, baseY + 1, 0) == 13, "torch should power third wire at strength 13");
		ok &= expect(level.getData(44, baseY + 1, 0) == 12, "torch should power fourth wire at strength 12");
		
		std::cerr << "block-smoke: button block wire propagation" << std::endl;
		for (int_t x = 50; x <= 53; ++x)
			level.setTile(x, baseY + 1, 0, 1);
		ok &= expect(level.setTile(49, baseY + 1, 0, Tile::buttonStone.id), "button source should place");
		level.setData(49, baseY + 1, 0, 2);
		for (int_t x = 50; x <= 53; ++x)
			ok &= expect(level.setTile(x, baseY + 2, 0, Tile::redstoneWire.id), "button test wire should place");
		ok &= expect(Tile::buttonStone.use(level, 49, baseY + 1, 0, player), "button source should toggle on");
		ok &= expect(level.getData(50, baseY + 2, 0) == 15, "button should power first wire through attached block at strength 15");
		ok &= expect(level.getData(51, baseY + 2, 0) == 14, "button should power second wire through attached block at strength 14");
		ok &= expect(level.getData(52, baseY + 2, 0) == 13, "button should power third wire through attached block at strength 13");
		ok &= expect(level.getData(53, baseY + 2, 0) == 12, "button should power fourth wire through attached block at strength 12");
		
		std::cerr << "block-smoke: button bounds" << std::endl;
		level.setTile(148, baseY + 1, 0, 1);
		ok &= expect(level.setTile(149, baseY + 1, 0, Tile::buttonStone.id), "button bounds test button should place");
		level.setData(149, baseY + 1, 0, 1);
		Tile::buttonStone.updateShape(level, 149, baseY + 1, 0);
		ok &= expect(Tile::buttonStone.xx0 == 0.0 && Tile::buttonStone.xx1 == 2.0 / 16.0, "unpowered button should be 2/16 deep in-world");
		ok &= expect(Tile::buttonStone.yy0 == 6.0 / 16.0 && Tile::buttonStone.yy1 == 10.0 / 16.0, "button should match beta in-world height bounds");
		ok &= expect(Tile::buttonStone.zz0 == 5.0 / 16.0 && Tile::buttonStone.zz1 == 11.0 / 16.0, "button should match beta in-world width bounds");
		level.setData(149, baseY + 1, 0, 1 | 8);
		Tile::buttonStone.updateShape(level, 149, baseY + 1, 0);
		ok &= expect(Tile::buttonStone.xx0 == 0.0 && Tile::buttonStone.xx1 == 1.0 / 16.0, "powered button should shrink to 1/16 depth in-world");
		
		std::cerr << "block-smoke: powered door" << std::endl;
		level.setTile(24, baseY, 0, 1);
		level.setTile(24, baseY + 1, 0, 64);
		level.setData(24, baseY + 1, 0, 0);
		level.setTile(24, baseY + 2, 0, 64);
		level.setData(24, baseY + 2, 0, 8);
		level.setTile(25, baseY, 0, 1);
		level.setTile(25, baseY + 1, 0, 76);
		level.setData(25, baseY + 1, 0, 5);
		Tile::doorWood.neighborChanged(level, 24, baseY + 2, 0, 76);
		ok &= expect((level.getData(24, baseY + 1, 0) & 4) != 0, "door should open when indirectly powered");

		std::cerr << "block-smoke: powered dispenser" << std::endl;
		ok &= expect(level.setTile(28, baseY + 1, 0, 23), "powered dispenser should place");
		auto poweredDispenser = std::dynamic_pointer_cast<DispenserTileEntity>(level.getTileEntity(28, baseY + 1, 0));
		ok &= expect(poweredDispenser != nullptr, "powered dispenser should create a tile entity");
		if (poweredDispenser != nullptr)
		{
			poweredDispenser->setItem(0, ItemInstance(Items::coal->getShiftedIndex(), 1, 0));
			level.setTile(29, baseY, 0, 1);
			level.setTile(29, baseY + 1, 0, 76);
			level.setData(29, baseY + 1, 0, 5);
			size_t beforeEntities = level.entities.size();
			Tile::dispenser.neighborChanged(level, 28, baseY + 1, 0, 76);
			Tile::dispenser.tick(level, 28, baseY + 1, 0, random);
			ok &= expect(level.entities.size() > beforeEntities, "dispenser should eject an item when powered");
		}
		std::cerr << "block-smoke: door particles" << std::endl;
		Tile *doorTile = Tile::tiles[64];
		int_t mirroredFace = -1;
		int_t mirroredData = -1;
		int_t mirroredTex = 0;
		for (int_t data = 0; data < 16 && mirroredFace < 0; ++data)
		{
			for (int_t face = 0; face < 6; ++face)
			{
				int_t tex = doorTile->getTexture(static_cast<Facing>(face), data);
				if (tex < 0)
				{
					mirroredFace = face;
					mirroredData = data;
					mirroredTex = tex;
					break;
				}
			}
		}
		ok &= expect(mirroredFace >= 0, "door should still use mirrored textures internally");
		if (mirroredFace >= 0)
		{
			InspectableTerrainParticle particle(level, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, doorTile, mirroredFace, mirroredData);
			ok &= expect(particle.textureIndex() == -mirroredTex, "door crack particles should clamp mirrored textures");
		}
	
		std::cerr << "block-smoke: repeated powered door" << std::endl;
		level.setTile(14, baseY, 0, 5);
		ok &= expect(level.setTile(14, baseY + 1, 0, 64), "repeat-test door bottom should place");
		level.setData(14, baseY + 1, 0, 0);
		ok &= expect(level.setTile(14, baseY + 2, 0, 64), "repeat-test door top should place");
		level.setData(14, baseY + 2, 0, 8);
		level.setTile(15, baseY + 1, 0, 1);
		ok &= expect(level.setTile(16, baseY + 1, 0, Tile::lever.id), "repeat-test door lever should place");
		level.setData(16, baseY + 1, 0, 1);
		listener.clear();
		ok &= expect(Tile::lever.use(level, 16, baseY + 1, 0, player), "repeat-test door lever should toggle on");
		ok &= expect((level.getData(14, baseY + 1, 0) & 4) != 0, "door should open on first redstone activation");
		listener.clear();
		ok &= expect(Tile::lever.use(level, 16, baseY + 1, 0, player), "repeat-test door lever should toggle off");
		ok &= expect((level.getData(14, baseY + 1, 0) & 4) == 0, "door should close when redstone power is removed");
		listener.clear();
		ok &= expect(Tile::lever.use(level, 16, baseY + 1, 0, player), "repeat-test door lever should toggle on again");
		ok &= expect((level.getData(14, baseY + 1, 0) & 4) != 0, "door should reopen on second redstone activation");
		
		std::cerr << "block-smoke: repeated powered note block" << std::endl;
		level.setTile(8, baseY, 0, 5);
		level.setTile(8, baseY + 2, 0, 0);
		ok &= expect(level.setTile(8, baseY + 1, 0, Tile::noteBlock.id), "repeat-test note block should place");
		ok &= expect(level.setTile(9, baseY + 1, 0, Tile::lever.id), "repeat-test note lever should place");
		level.setData(9, baseY + 1, 0, 1);
		auto repeatNoteEntity = std::dynamic_pointer_cast<NoteTileEntity>(level.getTileEntity(8, baseY + 1, 0));
		ok &= expect(repeatNoteEntity != nullptr, "repeat-test note block should create a tile entity");
		listener.clear();
		ok &= expect(Tile::lever.use(level, 9, baseY + 1, 0, player), "repeat-test note lever should toggle on");
		ok &= expect(!listener.sounds.empty() && listener.sounds.back().find(u"note.") == 0, "note block should play on first redstone activation");
		ok &= expect(!listener.particles.empty() && listener.particles.back() == u"note", "note block should spawn a particle on first redstone activation");
		if (repeatNoteEntity != nullptr)
			ok &= expect(repeatNoteEntity->previousRedstoneState, "note block should latch powered state after first activation");
		listener.clear();
		ok &= expect(Tile::lever.use(level, 9, baseY + 1, 0, player), "repeat-test note lever should toggle off");
		ok &= expect(listener.particles.empty(), "note block should not trigger a note on power removal");
		if (repeatNoteEntity != nullptr)
			ok &= expect(!repeatNoteEntity->previousRedstoneState, "note block should clear powered latch after power removal");
		listener.clear();
		ok &= expect(Tile::lever.use(level, 9, baseY + 1, 0, player), "repeat-test note lever should toggle on again");
		ok &= expect(!listener.sounds.empty() && listener.sounds.back().find(u"note.") == 0, "note block should play again on second redstone activation");
		ok &= expect(!listener.particles.empty() && listener.particles.back() == u"note", "note block should spawn a particle on second redstone activation");
		
		std::cerr << "block-smoke: door" << std::endl;
		level.setTile(0, baseY, 0, 5);
		level.setTile(0, baseY + 1, 0, 0);
		level.setTile(0, baseY + 2, 0, 0);
		ItemInstance doorStack(Items::doorWood->getShiftedIndex(), 1, 0);
		ok &= expect(doorStack.useOn(player, level, 0, baseY, 0, Facing::UP), "wood door item should place a door");
		ok &= expect(level.getTile(0, baseY + 1, 0) == 64, "wood door bottom half should place");
		ok &= expect(level.getTile(0, baseY + 2, 0) == 64, "wood door top half should place");
		listener.clear();
		ok &= expect(Tile::tiles[64]->use(level, 0, baseY + 1, 0, player), "wood door should toggle on use");
		ok &= expect(!listener.sounds.empty() && listener.sounds.back() == u"random.door_open", "wood door should play open sound");

		std::cerr << "block-smoke: note block" << std::endl;
		level.setTile(2, baseY, 0, 5);
		level.setTile(2, baseY + 1, 0, 0);
		level.setTile(2, baseY + 2, 0, 0);
		ok &= expect(level.setTile(2, baseY + 1, 0, 25), "note block should place");
		listener.clear();
		ok &= expect(Tile::tiles[25]->use(level, 2, baseY + 1, 0, player), "note block should retune and play");
		ok &= expect(!listener.sounds.empty() && listener.sounds.back().find(u"note.") == 0, "note block should play a note sound");
		ok &= expect(!listener.particles.empty() && listener.particles.back() == u"note", "note block should spawn a note particle");
		listener.clear();
		InspectableNoteParticle noteParticle(level, 0.0, 0.0, 0.0, 0.5, 2.0f);
		ok &= expect(noteParticle.currentSize() > 1.4f, "note particle should use the corrected beta scale");

		std::cerr << "block-smoke: jukebox" << std::endl;
		ok &= expect(level.setTile(6, baseY + 1, 0, 84), "jukebox should place");
		listener.clear();
		ItemInstance record13(Items::record13->getShiftedIndex(), 1, 0);
		ok &= expect(record13.useOn(player, level, 6, baseY + 1, 0, Facing::UP), "record should insert into jukebox");
		ok &= expect(level.getData(6, baseY + 1, 0) == 1, "jukebox should mark itself occupied");
		ok &= expect(!listener.streams.empty() && listener.streams.back() == u"13", "jukebox should start record playback");
		listener.clear();
		ok &= expect(Tile::tiles[84]->use(level, 6, baseY + 1, 0, player), "jukebox should eject record on use");
		ok &= expect(level.getData(6, baseY + 1, 0) == 0, "jukebox should clear occupied state after eject");
		ok &= expect(!listener.streams.empty() && listener.streams.back().empty(), "jukebox eject should stop streaming music");

		std::cerr << "block-smoke: dispenser" << std::endl;
		ok &= expect(level.setTile(8, baseY + 1, 0, 23), "dispenser should place");
		auto dispenserEntity = std::dynamic_pointer_cast<DispenserTileEntity>(level.getTileEntity(8, baseY + 1, 0));
		ok &= expect(dispenserEntity != nullptr, "dispenser should create a tile entity");
		if (dispenserEntity != nullptr)
		{
			dispenserEntity->setItem(0, ItemInstance(Items::coal->getShiftedIndex(), 3, 0));
			CompoundTag tag;
			dispenserEntity->save(tag);
			DispenserTileEntity loaded;
			loaded.load(tag);
			ok &= expect(loaded.getItem(0).itemID == Items::coal->getShiftedIndex(), "dispenser should save its inventory");
			ok &= expect(loaded.getItem(0).stackSize == 3, "dispenser should preserve stack counts");
			size_t entityCountBefore = level.entities.size();
			level.setTile(8, baseY + 1, 0, 0);
			ok &= expect(level.entities.size() > entityCountBefore, "dispenser removal should drop stored items");
		}

		std::cerr << "block-smoke: sign" << std::endl;
		level.setTile(14, baseY, 0, 5);
		level.setTile(14, baseY + 1, 0, 0);
		ItemInstance signPostStack(Items::sign->getShiftedIndex(), 1, 0);
		ok &= expect(signPostStack.useOn(player, level, 14, baseY, 0, Facing::UP), "sign item should place a sign post");
		ok &= expect(level.getTile(14, baseY + 1, 0) == 63, "sign post item should place sign post tile");
		auto signEntity = std::dynamic_pointer_cast<SignTileEntity>(level.getTileEntity(14, baseY + 1, 0));
		ok &= expect(signEntity != nullptr, "sign post should create a tile entity");
		if (signEntity != nullptr)
		{
			signEntity->signText[0] = u"hello";
			CompoundTag tag;
			signEntity->save(tag);
			signEntity->signText[0] = u"";
			signEntity->load(tag);
			ok &= expect(signEntity->signText[0] == u"hello", "sign should save/load text");
		}
		level.setTile(18, baseY + 1, 0, 1);
		level.setTile(19, baseY + 1, 0, 0);
		ItemInstance signWallStack(Items::sign->getShiftedIndex(), 1, 0);
		ok &= expect(signWallStack.useOn(player, level, 18, baseY + 1, 0, Facing::EAST), "sign item should place a wall sign");
		ok &= expect(level.getTile(19, baseY + 1, 0) == 68, "wall sign item should place wall sign tile");
		ok &= expect(level.getData(19, baseY + 1, 0) == static_cast<int_t>(Facing::EAST), "wall sign should keep face data");
		ok &= expect(std::dynamic_pointer_cast<SignTileEntity>(level.getTileEntity(19, baseY + 1, 0)) != nullptr, "wall sign should create a tile entity");
		ok &= expect(Tile::tiles[63]->getResource(0, level.random) == Items::sign->getShiftedIndex(), "sign post should drop sign item");

		std::cerr << "block-smoke: cobweb" << std::endl;
		player.setPos(4.0, static_cast<double>(baseY + 5), 0.0);
		double beforeX = player.x;
		double beforeY = player.y;
		Tile::tiles[30]->entityInside(level, 4, baseY + 5, 0, player);
		player.move(1.0, 1.0, 0.0);
		ok &= expect(!player.isInWeb, "web flag should clear after movement");
		ok &= expect((player.x - beforeX) < 0.5, "cobweb should strongly reduce horizontal motion");
		ok &= expect((player.y - beforeY) < 0.2, "cobweb should strongly reduce vertical motion");

		if (ok)
		{
			std::cout << "block-smoke: PASS" << std::endl;
			return 0;
		}
		std::cerr << "block-smoke: FAIL" << std::endl;
		return 1;
	}
	catch (const std::exception &e)
	{
		std::cerr << "block-smoke exception: " << e.what() << std::endl;
		return 2;
	}
	catch (...)
	{
		std::cerr << "block-smoke unknown exception" << std::endl;
		return 3;
	}
}
