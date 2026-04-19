#include "tools/BlockSmoke.h"

#include <iostream>
#include <vector>

#include "client/particle/TerrainParticle.h"
#include "world/item/Items.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/entity/player/Player.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/DispenserTileEntity.h"
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

	bool expect(bool condition, const char *message)
	{
		if (!condition)
		{
			std::cerr << "FAIL: " << message << std::endl;
			return false;
		}
		return true;
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
		ok &= expect(Tile::tiles[30]->getResource(0, random) == Items::silk->getShiftedIndex(), "cobweb should drop string");
		ok &= expect(Tile::tiles[89]->getResource(0, random) == Items::glowstoneDust->getShiftedIndex(), "glowstone should drop glowstone dust");
		ok &= expect(Tile::tiles[80]->getResource(0, random) == Items::snowball->getShiftedIndex(), "snow block should drop snowballs");
		ok &= expect(Tile::tiles[64]->getResource(0, random) == Items::doorWood->getShiftedIndex(), "wood door bottom half should drop wood door item");
		ok &= expect(Tile::tiles[64]->getResource(8, random) == 0, "wood door top half should not drop an item");

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
