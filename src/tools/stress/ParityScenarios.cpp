// Deterministic renderer-parity scenes. Companion translation unit to
// Scenarios.cpp: the registry there forwards every name it does not own to
// stress::parity::make(). Each scene fixes the world, time of day, weather and
// camera and then lets the ordinary game render it, so the same frame index is
// comparable between the OpenGL 2.1 oracle and a portable backend.
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "tools/stress/ParityScenarios.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "client/Minecraft.h"
#include "client/gui/InventoryScreen.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/player/LocalPlayer.h"
#include "client/spc/SPCCommand.h"
#include "java/String.h"
#include "lwjgl/GLContext.h"
#include "util/Memory.h"
#include "util/Mth.h"
#include "world/entity/player/InventoryPlayer.h"
#include "world/item/Item.h"
#include "world/item/ItemArmor.h"
#include "world/item/ItemInstance.h"
#include "world/item/ItemPickaxe.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/biome/BiomeSource.h"
#include "world/level/dimension/Dimension.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/tile/FireTile.h"
#include "world/level/tile/GlassTile.h"
#include "world/level/tile/GlowStoneTile.h"
#include "world/level/tile/IceTile.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/PortalTile.h"
#include "world/level/tile/SignTile.h"
#include "world/level/tile/StoneTile.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/TorchTile.h"
#include "world/level/tile/WoodTile.h"
#include "world/level/tile/entity/SignTileEntity.h"

namespace stress
{

// Every build stays inside the column range the other scenarios use, so no scene
// can create a chunk the terrain builder would reject.
static const int_t PARITY_MIN_Y = 2;
static const int_t PARITY_MAX_Y = 125;

// getHeightmap is the first free y above the terrain column: the surface block is
// at (result - 1) and a camera pinned at result stands on the ground.
static int_t paritySurface(Level &level, int_t x, int_t z)
{
	return level.getHeightmap(x, z);
}

static int_t parityFitY(int_t y, int_t height)
{
	if (y + height > PARITY_MAX_Y)
		y = PARITY_MAX_Y - height;
	if (y < PARITY_MIN_Y)
		y = PARITY_MIN_Y;
	return y;
}

// A sealed box of wallId with a hollow interior. Sealing it keeps skylight,
// weather and natural liquids out, so the only light is what the scene places.
struct ParityRoom
{
	int_t x0 = 0, y0 = 0, z0 = 0;
	int_t width = 0, height = 0, depth = 0;
	int_t placed = 0;

	double centerX() const
	{
		return static_cast<double>(x0) + width * 0.5;
	}

	double centerZ() const
	{
		return static_cast<double>(z0) + depth * 0.5;
	}
};

static ParityRoom parityBuildRoomAt(Level &level, int_t cx, int_t cz, int_t floorY, int_t width, int_t height,
									int_t depth, int_t wallId, bool roof)
{
	ParityRoom room;
	room.width = width;
	room.height = height;
	room.depth = depth;
	room.x0 = cx - width / 2;
	room.z0 = cz - depth / 2;
	room.y0 = floorY;

	for (int_t x = room.x0 - 1; x <= room.x0 + width; ++x)
	{
		for (int_t z = room.z0 - 1; z <= room.z0 + depth; ++z)
		{
			if (level.setTile(x, room.y0 - 1, z, wallId))
				++room.placed;
			if (roof && level.setTile(x, room.y0 + height, z, wallId))
				++room.placed;
			for (int_t y = room.y0; y < room.y0 + height; ++y)
			{
				const bool wall = x == room.x0 - 1 || x == room.x0 + width || z == room.z0 - 1 || z == room.z0 + depth;
				if (level.setTile(x, y, z, wall ? wallId : 0))
					++room.placed;
			}
		}
	}

	if (!roof)
	{
		// Open to the sky: clear whatever the generator put overhead (a tree can
		// reach into this column) so skylight reaches the floor at a fixed value.
		const int_t top = std::min(PARITY_MAX_Y, room.y0 + height + 10);
		for (int_t x = room.x0; x < room.x0 + width; ++x)
			for (int_t z = room.z0; z < room.z0 + depth; ++z)
				for (int_t y = room.y0 + height; y <= top; ++y)
					if (level.setTile(x, y, z, 0))
						++room.placed;
	}
	return room;
}

static ParityRoom parityBuildRoom(Level &level, int_t cx, int_t cz, int_t width, int_t height, int_t depth,
								  int_t wallId, bool roof)
{
	const int_t floorY = parityFitY(paritySurface(level, cx, cz) + 1, height + 2);
	return parityBuildRoomAt(level, cx, cz, floorY, width, height, depth, wallId, roof);
}

// Standing torches (data 5) on a fixed lattice, skipping occupied floor cells so
// scene content placed earlier is never overwritten.
static int_t parityLightRoom(Level &level, const ParityRoom &room, int_t spacing)
{
	int_t torches = 0;
	for (int_t x = room.x0 + spacing / 2; x < room.x0 + room.width; x += spacing)
	{
		for (int_t z = room.z0 + spacing / 2; z < room.z0 + room.depth; z += spacing)
		{
			if (level.getTile(x, room.y0, z) != 0)
				continue;
			if (level.setTileAndData(x, room.y0, z, Tile::torch.id, 5))
				++torches;
		}
	}
	return torches;
}

// Liquid display cell: a glass pane on the camera side, the liquid column behind
// it, solid walls elsewhere. The pane blocks flow while still letting the liquid
// side faces render (those use the flowing atlas cell); the open top renders the
// still cell. The liquid goes in last so every neighbour is already final and the
// sources have nowhere to spread.
static int_t parityLiquidCell(Level &level, int_t x0, int_t y0, int_t z, int_t width, int_t height, int_t liquidId,
							  int_t wallId)
{
	for (int_t x = x0; x < x0 + width; ++x)
	{
		for (int_t y = y0; y < y0 + height; ++y)
		{
			level.setTile(x, y, z - 1, Tile::glass.id);
			level.setTile(x, y, z + 1, wallId);
		}
	}
	for (int_t y = y0; y < y0 + height; ++y)
	{
		level.setTile(x0 - 1, y, z, wallId);
		level.setTile(x0 + width, y, z, wallId);
	}

	int_t placed = 0;
	for (int_t x = x0; x < x0 + width; ++x)
		for (int_t y = y0; y < y0 + height; ++y)
			if (level.setTile(x, y, z, liquidId))
				++placed;
	return placed;
}

// Enclosed pool: a ring of wallId at pool level around a filled core, so the
// sources only have air above them.
static int_t parityLiquidPool(Level &level, int_t cx, int_t y, int_t cz, int_t radius, int_t liquidId, int_t wallId)
{
	for (int_t x = cx - radius - 1; x <= cx + radius + 1; ++x)
		for (int_t z = cz - radius - 1; z <= cz + radius + 1; ++z)
			if (x < cx - radius || x > cx + radius || z < cz - radius || z > cz + radius)
				level.setTile(x, y, z, wallId);

	int_t placed = 0;
	for (int_t x = cx - radius; x <= cx + radius; ++x)
		for (int_t z = cz - radius; z <= cz + radius; ++z)
			if (level.setTile(x, y, z, liquidId))
				++placed;
	return placed;
}

// Real obsidian frame plus PortalTile::trySpawnPortal, the same entry point flint
// and steel drives. x0 is the left interior column; the frame occupies x0-1 and
// x0+2 with the sill at y0-1 and the lintel at y0+3.
static int_t parityBuildPortal(Level &level, int_t x0, int_t y0, int_t z)
{
	for (int_t x = x0 - 1; x <= x0 + 2; ++x)
	{
		level.setTile(x, y0 - 1, z, Tile::obsidian.id);
		level.setTile(x, y0 + 3, z, Tile::obsidian.id);
	}
	for (int_t y = y0; y < y0 + 3; ++y)
	{
		level.setTile(x0 - 1, y, z, Tile::obsidian.id);
		level.setTile(x0 + 2, y, z, Tile::obsidian.id);
		level.setTile(x0, y, z, 0);
		level.setTile(x0 + 1, y, z, 0);
	}

	if (!Tile::portal.trySpawnPortal(level, x0, y0, z))
		return 0;

	int_t blocks = 0;
	for (int_t x = x0; x <= x0 + 1; ++x)
		for (int_t y = y0; y < y0 + 3; ++y)
			if (level.getTile(x, y, z) == Tile::portal.id)
				++blocks;
	return blocks;
}

// Fire that cannot spread or be rained out: netherrack floor, nothing flammable
// within the tile's own search radius.
static bool parityEternalFire(Level &level, int_t x, int_t y, int_t z)
{
	level.setTile(x, y - 1, z, Tile::netherrack.id);
	return level.setTile(x, y, z, Tile::fire.id);
}

static int_t parityCountTiles(Level &level, int_t x0, int_t y0, int_t z0, int_t x1, int_t y1, int_t z1, int_t tileId)
{
	int_t count = 0;
	for (int_t x = x0; x <= x1; ++x)
		for (int_t y = y0; y <= y1; ++y)
			for (int_t z = z0; z <= z1; ++z)
				if (level.getTile(x, y, z) == tileId)
					++count;
	return count;
}

static void parityClearWeather(Level &level)
{
	level.setWeather(false, false);
	level.setRainStrength(0.0f);
}

static void parityFixedSky(Level &level, long_t time)
{
	level.setTime(time);
	parityClearWeather(level);
}

// Concentric lattice scan: the first column whose own biome and its four
// 8-block neighbours satisfy the predicate wins, so the site depends only on the
// world seed and not on iteration accidents.
static bool parityFindPrecipitationSite(Level &level, bool wantSnow, int_t maxRadius, int_t step, int_t &outX,
										int_t &outZ)
{
	BiomeSource &source = level.getBiomeSource();
	const auto suits = [&](int_t x, int_t z) {
		const BiomeInfo &info = source.getBiomeInfo(source.getBiome(x, z));
		return wantSnow ? info.enableSnow : info.canSpawnLightningBolt();
	};
	const auto siteSuits = [&](int_t x, int_t z) {
		for (int_t ox = -8; ox <= 8; ox += 8)
			for (int_t oz = -8; oz <= 8; oz += 8)
				if (!suits(x + ox, z + oz))
					return false;
		return true;
	};

	for (int_t radius = 0; radius <= maxRadius; radius += step)
	{
		for (int_t dx = -radius; dx <= radius; dx += step)
		{
			for (int_t dz = -radius; dz <= radius; dz += step)
			{
				const int_t ax = dx < 0 ? -dx : dx;
				const int_t az = dz < 0 ? -dz : dz;
				if (radius != 0 && ax != radius && az != radius)
					continue;
				if (siteSuits(dx, dz))
				{
					outX = dx;
					outZ = dz;
					return true;
				}
			}
		}
	}
	return false;
}

// Shared plumbing: one pinned camera for the whole run and a per-scene frame count.
class ParityScene : public Scenario
{
  protected:
	double camX = 0.0, camY = 0.0, camZ = 0.0;
	float camYaw = 0.0f, camPitch = 0.0f;
	int measuredFrames = 60;

	void aim(double x, double y, double z, float yaw, float pitch)
	{
		camX = x;
		camY = y;
		camZ = z;
		camYaw = yaw;
		camPitch = pitch;
	}

	void pin(World &world) const
	{
		pinPlayer(world.player, camX, camY, camZ, camYaw, camPitch);
	}

	void reportCamera(std::vector<std::string> &lines, const std::string &prefix) const
	{
		lines.push_back(prefix + "_camera " + std::to_string(camX) + " " + std::to_string(camY) + " " +
						std::to_string(camZ) + " yaw " + std::to_string(camYaw) + " pitch " + std::to_string(camPitch));
	}

  public:
	int defaultFrames() const override
	{
		return measuredFrames;
	}

	void onTick(World &world, long_t) override
	{
		pin(world);
	}
};

// --- terrain ---------------------------------------------------------------

// Natural terrain seen from above plus a solid stone cube: top faces, exterior
// cube faces and the fast/fancy plus ambient-occlusion paths of --fancy.
class ParityTerrainAboveScene final : public ParityScene
{
	int_t surfaceY = 0;
	int_t cubeBase = 0;
	int_t cubeBlocks = 0;

  public:
	const char *name() const override
	{
		return "parity_terrain_above";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);

		surfaceY = paritySurface(level, 0, 0);
		const int_t cubeX = 0;
		const int_t cubeZ = 18;
		cubeBase = parityFitY(paritySurface(level, cubeX, cubeZ), 5);
		for (int_t x = cubeX - 2; x <= cubeX + 2; ++x)
			for (int_t z = cubeZ - 2; z <= cubeZ + 2; ++z)
				for (int_t y = cubeBase; y < cubeBase + 5; ++y)
					if (level.setTile(x, y, z, Tile::rock.id))
						++cubeBlocks;

		aim(0.5, parityFitY(surfaceY + 22, 1), 0.5, 0.0f, 50.0f);
		pin(world);
	}

	void report(World &, std::vector<std::string> &lines) override
	{
		lines.push_back("terrain_above_surface_y " + std::to_string(surfaceY));
		lines.push_back("terrain_above_cube_base_y " + std::to_string(cubeBase));
		lines.push_back("terrain_above_cube_blocks " + std::to_string(cubeBlocks));
		reportCamera(lines, "terrain_above");
	}
};

// Natural terrain seen from below: a carved chamber whose ceiling is untouched
// world blocks, so the camera looks at real terrain undersides and the interior
// faces of the surrounding cubes.
class ParityTerrainBelowScene final : public ParityScene
{
	int_t ceilingY = 0;
	int_t floorY = 0;
	int_t carved = 0;
	int_t torches = 0;

  public:
	const char *name() const override
	{
		return "parity_terrain_below";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);

		const int_t half = 12;
		ceilingY = paritySurface(level, 0, 0) - 4;
		if (ceilingY < 12)
			ceilingY = 12;
		floorY = ceilingY - 5;

		for (int_t x = -half; x <= half; ++x)
		{
			for (int_t z = -half; z <= half; ++z)
			{
				level.setTile(x, floorY - 1, z, Tile::rock.id);
				for (int_t y = floorY; y < ceilingY; ++y)
					if (level.setTile(x, y, z, 0))
						++carved;
			}
		}
		// Seal the sides so a natural spring or lava pocket next to the carve
		// cannot flow in and animate the frame; the ceiling stays untouched.
		for (int_t y = floorY; y < ceilingY; ++y)
		{
			for (int_t x = -half - 1; x <= half + 1; ++x)
			{
				level.setTile(x, y, -half - 1, Tile::rock.id);
				level.setTile(x, y, half + 1, Tile::rock.id);
			}
			for (int_t z = -half - 1; z <= half + 1; ++z)
			{
				level.setTile(-half - 1, y, z, Tile::rock.id);
				level.setTile(half + 1, y, z, Tile::rock.id);
			}
		}
		for (int_t x = -half + 3; x <= half - 3; x += 6)
			for (int_t z = -half + 3; z <= half - 3; z += 6)
				if (level.setTileAndData(x, floorY, z, Tile::torch.id, 5))
					++torches;

		aim(0.5, floorY, 0.5, 0.0f, -70.0f);
		pin(world);
	}

	void report(World &, std::vector<std::string> &lines) override
	{
		lines.push_back("terrain_below_ceiling_y " + std::to_string(ceilingY));
		lines.push_back("terrain_below_floor_y " + std::to_string(floorY));
		lines.push_back("terrain_below_carved " + std::to_string(carved));
		lines.push_back("terrain_below_torches " + std::to_string(torches));
		reportCamera(lines, "terrain_below");
	}
};

// Cutout and translucent terrain in one frame: leaves (three wood types), glass,
// ice, water, lava and fire, each in a shape that cannot change while the run
// executes.
class ParityTransparentScene final : public ParityScene
{
	ParityRoom room;
	int_t waterBlocks = 0, lavaBlocks = 0;
	int_t contentZ = 0;
	int_t fireX = 0, fireZ = 0;

  public:
	const char *name() const override
	{
		return "parity_transparent";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		// Open roof: full skylight at noon lights every sample without block light,
		// which would melt the ice column the way it does in the real game.
		room = parityBuildRoom(level, 0, 0, 21, 8, 20, Tile::rock.id, false);
		measuredFrames = 90;

		contentZ = room.z0 + 15;
		for (int_t i = 0; i < 3; ++i)
			for (int_t y = room.y0; y < room.y0 + 3; ++y)
				level.setTileAndData(room.x0 + 1 + i, y, contentZ, Tile::leaves.id, i);
		for (int_t i = 0; i < 2; ++i)
			for (int_t y = room.y0; y < room.y0 + 3; ++y)
				level.setTile(room.x0 + 5 + i, y, contentZ, Tile::glass.id);
		for (int_t i = 0; i < 2; ++i)
			for (int_t y = room.y0; y < room.y0 + 3; ++y)
				level.setTile(room.x0 + 8 + i, y, contentZ, Tile::ice.id);

		waterBlocks = parityLiquidCell(level, room.x0 + 11, room.y0, contentZ, 3, 3, Tile::water.id, Tile::rock.id);
		lavaBlocks = parityLiquidCell(level, room.x0 + 17, room.y0, contentZ, 3, 3, Tile::lava.id, Tile::rock.id);

		fireX = room.x0 + 15;
		fireZ = room.z0 + 11;
		parityEternalFire(level, fireX, room.y0, fireZ);
		aim(room.x0 + 10.5, room.y0 + 2, room.z0 + 1.5, 0.0f, 10.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		Level &level = world.level;
		const int_t x1 = room.x0 + room.width - 1;
		const int_t y1 = room.y0 + room.height - 1;
		const int_t z1 = room.z0 + room.depth - 1;
		lines.push_back("transparent_room_floor_y " + std::to_string(room.y0));
		lines.push_back("transparent_content_z " + std::to_string(contentZ));
		lines.push_back("transparent_leaves " + std::to_string(parityCountTiles(level, room.x0, room.y0, room.z0, x1,
																				y1, z1, Tile::leaves.id)));
		lines.push_back("transparent_glass " +
						std::to_string(parityCountTiles(level, room.x0, room.y0, room.z0, x1, y1, z1, Tile::glass.id)));
		lines.push_back("transparent_ice " +
						std::to_string(parityCountTiles(level, room.x0, room.y0, room.z0, x1, y1, z1, Tile::ice.id)));
		lines.push_back(
			"transparent_water " + std::to_string(waterBlocks) + " now " +
			std::to_string(parityCountTiles(level, room.x0, room.y0, room.z0, x1, y1, z1, Tile::water.id) +
						   parityCountTiles(level, room.x0, room.y0, room.z0, x1, y1, z1, Tile::calmWater.id)));
		lines.push_back(
			"transparent_lava " + std::to_string(lavaBlocks) + " now " +
			std::to_string(parityCountTiles(level, room.x0, room.y0, room.z0, x1, y1, z1, Tile::lava.id) +
						   parityCountTiles(level, room.x0, room.y0, room.z0, x1, y1, z1, Tile::calmLava.id)));
		lines.push_back("transparent_fire_present " +
						std::to_string(level.getTile(fireX, room.y0, fireZ) == Tile::fire.id));
		// Evidence that the samples are sky-lit: block light stays at zero, which is
		// why the ice column survives (IceTile melts above block light 8).
		lines.push_back("transparent_light_sky " +
						std::to_string(level.getBrightness(LightLayer::Sky, room.x0 + 8, room.y0 + 1, contentZ - 1)));
		lines.push_back("transparent_light_block " +
						std::to_string(level.getBrightness(LightLayer::Block, room.x0 + 8, room.y0 + 1, contentZ - 1)));
		reportCamera(lines, "transparent");
	}
};

// Every CPU TextureFX the client registers, on screen at once: water still and
// flowing, lava still and flowing, fire, portal, plus a held compass and a clock
// in the hotbar for the two item FX.
class ParityTextureFxScene final : public ParityScene
{
	ParityRoom room;
	int_t portalBlocks = 0;
	int_t waterBlocks = 0, lavaBlocks = 0;
	int_t fireX = 0, fireZ = 0;
	int_t handItem = 0, hotbarClock = 0;

  public:
	const char *name() const override
	{
		return "parity_texturefx";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		room = parityBuildRoom(level, 0, 0, 19, 8, 14, Tile::rock.id, false);
		measuredFrames = 120;

		const int_t contentZ = room.z0 + 11;
		waterBlocks = parityLiquidCell(level, room.x0 + 1, room.y0, contentZ, 3, 3, Tile::water.id, Tile::rock.id);
		lavaBlocks = parityLiquidCell(level, room.x0 + 9, room.y0, contentZ, 3, 3, Tile::lava.id, Tile::rock.id);
		fireX = room.x0 + 6;
		fireZ = contentZ;
		parityEternalFire(level, fireX, room.y0, fireZ);
		portalBlocks = parityBuildPortal(level, room.x0 + 15, room.y0, contentZ);

		InventoryPlayer &inventory = world.player.inventory;
		handItem = Items::compass->getShiftedIndex();
		hotbarClock = Items::clock->getShiftedIndex();
		inventory.setItem(0, ItemInstance(handItem, 1));
		inventory.setItem(1, ItemInstance(hotbarClock, 1));
		inventory.setItem(2, ItemInstance(Tile::cobblestone.id, 1));
		inventory.currentItem = 0;

		aim(room.x0 + 9.5, room.y0 + 2, room.z0 + 1.5, 0.0f, 8.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		Level &level = world.level;
		lines.push_back("texturefx_portal_blocks " + std::to_string(portalBlocks) + " expected 6");
		lines.push_back("texturefx_water_blocks " + std::to_string(waterBlocks));
		lines.push_back("texturefx_lava_blocks " + std::to_string(lavaBlocks));
		lines.push_back("texturefx_fire_present " +
						std::to_string(level.getTile(fireX, room.y0, fireZ) == Tile::fire.id));
		lines.push_back("texturefx_light_sky " +
						std::to_string(level.getBrightness(LightLayer::Sky, room.x0 + 2, room.y0 + 1, room.z0 + 9)));
		lines.push_back("texturefx_hand_item " + std::to_string(handItem));
		lines.push_back("texturefx_hotbar_clock " + std::to_string(hotbarClock));
		reportCamera(lines, "texturefx");
	}
};

// --- sky and clouds --------------------------------------------------------

class ParityCloudsScene final : public ParityScene
{
  public:
	enum class Vantage
	{
		Below,
		Above,
	};

  private:
	Vantage vantage;
	float cloudHeight = 0.0f;

  public:
	explicit ParityCloudsScene(Vantage vantage) : vantage(vantage) {}

	const char *name() const override
	{
		return vantage == Vantage::Below ? "parity_clouds_below" : "parity_clouds_above";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		cloudHeight = level.dimension->getCloudHeight();
		if (vantage == Vantage::Below)
			aim(0.5, parityFitY(static_cast<int_t>(cloudHeight) - 20, 1), 0.5, 45.0f, -55.0f);
		else
			aim(0.5, parityFitY(static_cast<int_t>(cloudHeight) + 16, 1), 0.5, 45.0f, 40.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		const std::string prefix = vantage == Vantage::Below ? "clouds_below" : "clouds_above";
		lines.push_back(prefix + "_cloud_height " + std::to_string(cloudHeight));
		lines.push_back(prefix + "_fancy " + std::to_string(world.minecraft.options.fancyGraphics));
		lines.push_back(prefix + "_cache_cloud_geometry " + std::to_string(LevelRenderer::cacheCloudGeometry));
		reportCamera(lines, prefix);
	}
};

// Sky dome, sun, moon, stars and the smooth-shaded sunrise fan at three fixed
// times of day. LevelRenderer::renderSky only runs below view distance 2, which
// the runner enforces for these names.
class ParitySkyScene final : public ParityScene
{
  public:
	enum class Phase
	{
		Day,
		Night,
		Dawn,
	};

  private:
	Phase phase;
	long_t worldTime = 0;

	std::string prefix() const
	{
		return phase == Phase::Day ? "sky_day" : phase == Phase::Night ? "sky_night" : "sky_dawn";
	}

  public:
	explicit ParitySkyScene(Phase phase) : phase(phase) {}

	const char *name() const override
	{
		return phase == Phase::Day ? "parity_sky_day" : phase == Phase::Night ? "parity_sky_night" : "parity_sky_dawn";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		worldTime = phase == Phase::Day ? 6000 : phase == Phase::Night ? 18000 : 23000;
		parityFixedSky(level, worldTime);

		const int_t feet = parityFitY(paritySurface(level, 0, 0) + 8, 1);
		if (phase == Phase::Dawn)
			aim(0.5, feet, 0.5, 180.0f, -10.0f);
		else
			aim(0.5, feet, 0.5, 0.0f, -80.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		Level &level = world.level;
		const std::string tag = prefix();
		lines.push_back(tag + "_time " + std::to_string(level.time));
		lines.push_back(tag + "_requested_time " + std::to_string(worldTime));
		lines.push_back(tag + "_time_of_day " + std::to_string(level.getTimeOfDay(1.0f)));
		lines.push_back(tag + "_sun_angle " + std::to_string(level.getSunAngle(1.0f)));
		lines.push_back(tag + "_star_brightness " + std::to_string(level.getStarBrightness(1.0f)));
		lines.push_back(tag + "_sky_darken " + std::to_string(level.skyDarken));
		const float *sunrise = level.dimension->getSunriseColor(level.getTimeOfDay(1.0f), 1.0f);
		lines.push_back(tag + "_sunrise_alpha " +
						(sunrise == nullptr ? std::string("none") : std::to_string(sunrise[3])));
		lines.push_back(tag + "_sky_pass_enabled " + std::to_string(world.minecraft.options.viewDistance < 2));
		reportCamera(lines, tag);
	}
};

// --- particles -------------------------------------------------------------

// Every particle kind the level listener can construct, plus the block-break and
// dispenser level events and the ambient particles Level::animateTick spawns from
// torches, fire and an open lava pool.
class ParityParticlesScene final : public ParityScene
{
	ParityRoom room;
	int_t torches = 0;
	int_t lavaBlocks = 0;
	int_t breakEvents = 0, smokeEvents = 0;
	int_t probeX = 0, probeY = 0, probeZ = 0;

	static const std::vector<jstring> &particleNames()
	{
		static const std::vector<jstring> names = {
			u"bubble", u"splash", u"reddust", u"portal",	   u"note",	 u"smoke", u"largesmoke",
			u"flame",  u"lava",	  u"explode", u"snowballpoof", u"slime", u"heart",
		};
		return names;
	}

  public:
	const char *name() const override
	{
		return "parity_particles";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		room = parityBuildRoom(level, 0, 0, 17, 8, 14, Tile::rock.id, false);
		measuredFrames = 120;

		lavaBlocks = parityLiquidPool(level, room.x0 + 8, room.y0, room.z0 + 10, 1, Tile::lava.id, Tile::rock.id);
		parityEternalFire(level, room.x0 + 3, room.y0, room.z0 + 10);
		torches = parityLightRoom(level, room, 5);

		probeX = room.x0 + 13;
		probeY = room.y0;
		probeZ = room.z0 + 10;
		level.setTile(probeX, probeY, probeZ, Tile::rock.id);

		aim(room.x0 + 8.5, room.y0 + 2, room.z0 + 1.5, 0.0f, 5.0f);
		pin(world);
	}

	void onTick(World &world, long_t tick) override
	{
		pin(world);
		Level &level = world.level;
		const std::vector<jstring> &names = particleNames();
		const double y = camY + 1.5;
		const double z = camZ + 8.0;
		for (std::size_t i = 0; i < names.size(); ++i)
			level.addParticle(names[i], camX - 6.0 + static_cast<double>(i), y, z, 0.0, 0.0, 0.0);

		if (tick % 20 == 0)
		{
			level.levelEvent(nullptr, 2001, probeX, probeY, probeZ, Tile::rock.id);
			++breakEvents;
		}
		else if (tick % 20 == 10)
		{
			level.levelEvent(nullptr, 2000, probeX, probeY + 1, probeZ, 4);
			++smokeEvents;
		}
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		lines.push_back("particles_kinds " + std::to_string(particleNames().size()));
		lines.push_back("particles_room_floor_y " + std::to_string(room.y0));
		lines.push_back("particles_break_events " + std::to_string(breakEvents));
		lines.push_back("particles_smoke_events " + std::to_string(smokeEvents));
		lines.push_back("particles_lava_pool " + std::to_string(lavaBlocks));
		lines.push_back("particles_torches " + std::to_string(torches));
		lines.push_back("particles_live " + String::toUTF8(world.minecraft.particleEngine.countParticles()));
		reportCamera(lines, "particles");
	}
};

// --- gui, font, signs and items -------------------------------------------

// Fills the hotbar with two-dimensional icons and three-dimensional tiles and the
// armour slots so the whole HUD row renders; used by both GUI scenes.
static void parityFillPlayerGear(LocalPlayer &player)
{
	InventoryPlayer &inventory = player.inventory;
	inventory.setItem(0, ItemInstance(Tile::cobblestone.id, 1));
	inventory.setItem(1, ItemInstance(Tile::wood.id, 32));
	inventory.setItem(2, ItemInstance(Tile::glass.id, 8));
	inventory.setItem(3, ItemInstance(Tile::torch.id, 64));
	inventory.setItem(4, ItemInstance(Items::swordDiamond->getShiftedIndex(), 1));
	inventory.setItem(5, ItemInstance(Items::compass->getShiftedIndex(), 1));
	inventory.setItem(6, ItemInstance(Items::clock->getShiftedIndex(), 1));
	inventory.setItem(7, ItemInstance(Items::apple->getShiftedIndex(), 5));
	inventory.setItem(8, ItemInstance(Items::arrow->getShiftedIndex(), 12));
	inventory.currentItem = 0;

	inventory.armorInventory[0] = ItemInstance(Items::bootsIron->getShiftedIndex(), 1);
	inventory.armorInventory[1] = ItemInstance(Items::legsIron->getShiftedIndex(), 1);
	inventory.armorInventory[2] = ItemInstance(Items::plateIron->getShiftedIndex(), 1);
	inventory.armorInventory[3] = ItemInstance(Items::helmetIron->getShiftedIndex(), 1);
}

// Four fixed chat lines: colour codes, digits and a line long enough to wrap.
static int_t parityPushChat()
{
	SPCCommand::addChatMessage(u"\u00a7fparity chat line one");
	SPCCommand::addChatMessage(u"\u00a7e0123456789 \u00a7bmixed \u00a7ccolours");
	SPCCommand::addChatMessage(
		u"\u00a77beta 1.7.3 portable renderer parity harness wrapping check for the chat box width");
	SPCCommand::addChatMessage(u"\u00a7fWWWWWWWWWWWWWWWWWWWW");
	return 4;
}

// In-game HUD: hotbar (icons and tiles), selection frame, crosshair, hearts,
// armour row, vignette and chat text.
class ParityGuiHudScene final : public ParityScene
{
	int_t chatLines = 0;

  public:
	const char *name() const override
	{
		return "parity_gui_hud";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		measuredFrames = 90;
		parityFillPlayerGear(world.player);
		chatLines = parityPushChat();
		aim(0.5, paritySurface(level, 0, 0), 0.5, 30.0f, 10.0f);
		pin(world);
	}

	void onTick(World &world, long_t tick) override
	{
		pin(world);
		// SPCCommand keeps a message for MESSAGE_DISPLAY_TICKS gui ticks; refresh
		// well inside that window so the final frame always has chat on screen.
		if (tick != 0 && tick % 100 == 0)
			chatLines += parityPushChat();
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		InventoryPlayer &inventory = world.player.inventory;
		std::string hotbar;
		for (int_t slot = 0; slot < 9; ++slot)
		{
			const ItemInstance *item = inventory.getItem(slot);
			hotbar += (slot == 0 ? "" : ",") + std::to_string(item == nullptr ? 0 : item->itemID.load());
		}
		lines.push_back("gui_hud_hotbar " + hotbar);
		lines.push_back("gui_hud_armor_value " + std::to_string(inventory.getArmorValue()));
		lines.push_back("gui_hud_chat_pushed " + std::to_string(chatLines));
		lines.push_back("gui_hud_chat_live " + std::to_string(SPCCommand::messages.size()));
		lines.push_back("gui_hud_health " + std::to_string(world.player.health));
		reportCamera(lines, "gui_hud");
	}
};

// Inventory screen: container background, three-dimensional items in slots, the
// rotating player model, armour slots and screen labels over the live HUD.
class ParityGuiInventoryScene final : public ParityScene
{
	int_t filledSlots = 0;

  public:
	const char *name() const override
	{
		return "parity_gui_inventory";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		measuredFrames = 90;
		parityFillPlayerGear(world.player);

		InventoryPlayer &inventory = world.player.inventory;
		static const int_t contents[] = {
			Tile::rock.id,	   Tile::wood.id,		Tile::glass.id,		Tile::ice.id,	Tile::leaves.id,
			Tile::obsidian.id, Tile::netherrack.id, Tile::glowstone.id, Tile::brick.id,
		};
		for (int_t slot = 9; slot < 36; ++slot)
			inventory.setItem(slot, ItemInstance(contents[(slot - 9) % 9], 1 + (slot - 9) % 32));
		for (int_t slot = 0; slot < 36; ++slot)
		{
			const ItemInstance *item = inventory.getItem(slot);
			if (item != nullptr && !item->isEmpty())
				++filledSlots;
		}

		aim(0.5, paritySurface(level, 0, 0), 0.5, 30.0f, 10.0f);
		pin(world);
	}

	void onTick(World &world, long_t) override
	{
		pin(world);
		// Java skips entity preparation for the first two world frames. Open the
		// screen only after its player preview has a live texture dispatcher.
		if (!world.minecraft.screen && EntityRenderDispatcher::instance.textures == &world.minecraft.textures)
			world.minecraft.setScreen(Util::make_shared<InventoryScreen>(world.minecraft));
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		if (!world.minecraft.screen)
			throw std::runtime_error("Inventory parity capture requires world-render warmup");
		lines.push_back("gui_inventory_filled_slots " + std::to_string(filledSlots));
		lines.push_back("gui_inventory_screen_open " + std::to_string(world.minecraft.screen != nullptr));
		lines.push_back("gui_inventory_armor_value " + std::to_string(world.player.inventory.getArmorValue()));
		reportCamera(lines, "gui_inventory");
	}
};

// Signs: two standing posts at different rotations and one wall sign, each with
// four text lines, so the font renders in world space with depth testing.
class ParitySignScene final : public ParityScene
{
	int_t baseY = 0;
	int_t postA = 0, postB = 0, wallSign = 0;
	int_t textsSet = 0;

	static void setText(Level &level, int_t x, int_t y, int_t z, const jstring &l0, const jstring &l1,
						const jstring &l2, const jstring &l3, int_t &counter)
	{
		std::shared_ptr<SignTileEntity> sign = std::dynamic_pointer_cast<SignTileEntity>(level.getTileEntity(x, y, z));
		if (sign == nullptr)
			return;
		sign->signText[0] = l0;
		sign->signText[1] = l1;
		sign->signText[2] = l2;
		sign->signText[3] = l3;
		level.tileEntityChanged(x, y, z, sign);
		++counter;
	}

  public:
	const char *name() const override
	{
		return "parity_sign";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);

		baseY = parityFitY(paritySurface(level, 0, 0), 6);
		for (int_t x = -6; x <= 6; ++x)
		{
			for (int_t z = -2; z <= 12; ++z)
			{
				level.setTile(x, baseY - 1, z, Tile::rock.id);
				for (int_t y = baseY; y < baseY + 5; ++y)
					level.setTile(x, y, z, 0);
			}
		}

		// Standing signs face the player who placed them: metadata 8 is a sign
		// facing -Z, which is what a camera looking along +Z sees head on.
		level.setTileAndData(0, baseY, 8, Tile::signPost.id, 8);
		level.setTileAndData(-3, baseY, 8, Tile::signPost.id, 12);
		// Wall signs with metadata 2 need solid support at z + 1.
		level.setTile(3, baseY + 1, 9, Tile::rock.id);
		level.setTileAndData(3, baseY + 1, 8, Tile::signWall.id, 2);

		setText(level, 0, baseY, 8, u"parity sign", u"standing 8", u"0123456789", u"WWWWWWWWWWWWWWW", textsSet);
		setText(level, -3, baseY, 8, u"rotated 12", u"beta 1.7.3", u"world space", u"font test", textsSet);
		setText(level, 3, baseY + 1, 8, u"wall sign", u"metadata 2", u"depth tested", u"abcdefghijklmno", textsSet);

		postA = level.getTile(0, baseY, 8);
		postB = level.getTile(-3, baseY, 8);
		wallSign = level.getTile(3, baseY + 1, 8);

		aim(0.0, baseY, 4.0, 0.0f, 8.0f);
		pin(world);
	}

	void report(World &, std::vector<std::string> &lines) override
	{
		lines.push_back("sign_base_y " + std::to_string(baseY));
		lines.push_back("sign_post_a_tile " + std::to_string(postA));
		lines.push_back("sign_post_b_tile " + std::to_string(postB));
		lines.push_back("sign_wall_tile " + std::to_string(wallSign));
		lines.push_back("sign_texts_set " + std::to_string(textsSet) + " expected 3");
		reportCamera(lines, "sign");
	}
};

// First-person held item. The variants are the three distinct HeldItemRenderer
// paths (tile, full-3D item sprite, flat icon) plus the bare hand model.
class ParityHeldItemScene final : public ParityScene
{
	std::string variant = "block";
	int_t itemId = 0;
	long_t swings = 0;

  public:
	const char *name() const override
	{
		return "parity_held_item";
	}

	void setup(World &world, const Params &params) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		measuredFrames = 90;

		variant = params.stringOr("item", "block");
		if (variant == "block")
			itemId = Tile::cobblestone.id;
		else if (variant == "tool")
			itemId = Items::swordDiamond->getShiftedIndex();
		else if (variant == "flat")
			itemId = Items::compass->getShiftedIndex();
		else
			itemId = 0;

		InventoryPlayer &inventory = world.player.inventory;
		inventory.setItem(0, itemId == 0 ? ItemInstance() : ItemInstance(itemId, 1));
		inventory.currentItem = 0;

		aim(0.5, paritySurface(level, 0, 0), 0.5, 25.0f, 5.0f);
		pin(world);
	}

	void onTick(World &world, long_t tick) override
	{
		pin(world);
		if (tick % 20 == 0)
		{
			world.player.swing();
			++swings;
		}
	}

	void report(World &, std::vector<std::string> &lines) override
	{
		lines.push_back("held_item_variant " + variant);
		lines.push_back("held_item_id " + std::to_string(itemId));
		lines.push_back("held_item_swings " + std::to_string(swings));
		reportCamera(lines, "held_item");
	}
};

// Breaking overlay and selection outline together: the game-mode digs the block
// the camera is looking at until a fixed progress, then holds it, so the crack
// stage, the depth-biased overlay and the two-pixel outline are all frozen.
class ParityBreakOutlineScene final : public ParityScene
{
	double targetProgress = 0.55;
	int_t baseY = 0;
	int_t targetX = 0, targetY = 0, targetZ = 0;
	long_t digTicks = 0, frozenTicks = 0;

  public:
	const char *name() const override
	{
		return "parity_break_outline";
	}

	void setup(World &world, const Params &params) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		measuredFrames = 120;
		targetProgress = params.doubleOr("target", 0.55);

		baseY = parityFitY(paritySurface(level, 0, 0), 7);
		for (int_t x = -5; x <= 5; ++x)
		{
			for (int_t z = -2; z <= 8; ++z)
			{
				level.setTile(x, baseY - 1, z, Tile::rock.id);
				for (int_t y = baseY; y < baseY + 6; ++y)
					level.setTile(x, y, z, 0);
			}
		}
		for (int_t x = -3; x <= 3; ++x)
			for (int_t y = baseY; y < baseY + 4; ++y)
				level.setTile(x, y, 3, Tile::rock.id);

		targetX = 0;
		targetY = baseY + 1;
		targetZ = 3;

		// A wooden pickaxe can harvest stone, so progress accrues at a slow fixed
		// rate and the block never breaks before the hold kicks in.
		world.player.inventory.setItem(0, ItemInstance(Items::pickaxeWood->getShiftedIndex(), 1));
		world.player.inventory.currentItem = 0;

		aim(0.5, baseY, 0.5, 0.0f, 0.0f);
		pin(world);
	}

	void onTick(World &world, long_t) override
	{
		pin(world);
		if (world.minecraft.levelRenderer.destroyProgress < static_cast<float>(targetProgress))
		{
			// The real input path: continueDestroyBlock on the picked block plus
			// its crack particles.
			world.minecraft.handleMouseDown(0, true);
			++digTicks;
		}
		else
		{
			++frozenTicks;
		}
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		const float progress = world.minecraft.levelRenderer.destroyProgress;
		const HitResult &hit = world.minecraft.hitResult;
		lines.push_back("break_target_progress " + std::to_string(targetProgress));
		lines.push_back("break_base_y " + std::to_string(baseY));
		lines.push_back("break_destroy_progress " + std::to_string(progress));
		lines.push_back("break_crack_stage " + std::to_string(static_cast<int_t>(progress * 10.0f)));
		lines.push_back("break_dig_ticks " + std::to_string(digTicks));
		lines.push_back("break_frozen_ticks " + std::to_string(frozenTicks));
		lines.push_back("break_hit_type " + std::to_string(static_cast<int_t>(hit.type)));
		lines.push_back("break_hit_pos " + std::to_string(hit.x) + " " + std::to_string(hit.y) + " " +
						std::to_string(hit.z));
		lines.push_back("break_target_tile " + std::to_string(world.level.getTile(targetX, targetY, targetZ)));
		reportCamera(lines, "break");
	}
};

// --- weather ---------------------------------------------------------------

// Rain or snow at full strength. The site is picked from the biome map, because
// the snow pass only draws over columns whose biome enables snow and the rain
// pass only over columns that allow lightning.
class ParityPrecipitationScene final : public ParityScene
{
  public:
	enum class Kind
	{
		Rain,
		Snow,
	};

  private:
	Kind kind;
	bool thunder = false;
	int_t siteX = 0, siteZ = 0;
	int_t searchRadius = 0;
	int_t biome = 0;

	std::string prefix() const
	{
		return kind == Kind::Rain ? "rain" : "snow";
	}

  public:
	explicit ParityPrecipitationScene(Kind kind) : kind(kind) {}

	const char *name() const override
	{
		return kind == Kind::Rain ? "parity_rain" : "parity_snow";
	}

	void setup(World &world, const Params &params) override
	{
		Level &level = world.level;
		level.setTime(6000);
		measuredFrames = 240;

		const bool wantSnow = kind == Kind::Snow;
		searchRadius = static_cast<int_t>(params.intOr("search", wantSnow ? 3072 : 512));
		if (!parityFindPrecipitationSite(level, wantSnow, searchRadius, 64, siteX, siteZ))
			throw std::runtime_error(std::string(name()) + ": no " + (wantSnow ? "snow" : "rain") + " biome within " +
									 std::to_string(searchRadius) +
									 " blocks of spawn for this seed; widen --search or pick another --seed");
		biome = static_cast<int_t>(level.getBiomeSource().getBiome(siteX, siteZ));

		thunder = params.intOr("thunder", 0) != 0;
		level.setWeather(true, thunder);
		// Rain strength ramps at 0.01 per tick from zero; start saturated so the
		// precipitation pass and the darkened fog are both fully in effect.
		level.setRainStrength(1.0f);

		aim(siteX + 0.5, parityFitY(paritySurface(level, siteX, siteZ), 3), siteZ + 0.5, 0.0f, 0.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		Level &level = world.level;
		const BiomeInfo &info = level.getBiomeSource().getBiomeInfo(level.getBiomeSource().getBiome(siteX, siteZ));
		const std::string tag = prefix();
		lines.push_back(tag + "_site " + std::to_string(siteX) + " " + std::to_string(siteZ));
		lines.push_back(tag + "_search_radius " + std::to_string(searchRadius));
		lines.push_back(tag + "_biome " + std::to_string(biome));
		lines.push_back(tag + "_biome_enable_snow " + std::to_string(info.enableSnow));
		lines.push_back(tag + "_biome_lightning " + std::to_string(info.canSpawnLightningBolt()));
		lines.push_back(tag + "_rain_strength " + std::to_string(level.getRainStrength(1.0f)));
		lines.push_back(tag + "_thunder_requested " + std::to_string(thunder));
		lines.push_back(tag + "_thunder_strength " + std::to_string(level.getThunderStrength(1.0f)));
		lines.push_back(tag + "_is_raining " + std::to_string(level.isRaining()));
		reportCamera(lines, tag);
	}
};

// --- fog -------------------------------------------------------------------

// Eye inside water or lava: the exponential fog modes, the clear-colour override
// and the first-person liquid overlay. Glowstone in the far wall gives the fog
// gradient something to fall off against.
class ParitySubmergedScene final : public ParityScene
{
  public:
	enum class Liquid
	{
		Water,
		Lava,
	};

  private:
	Liquid liquid;
	ParityRoom room;
	int_t liquidBlocks = 0;
	int_t glowstone = 0;

	std::string prefix() const
	{
		return liquid == Liquid::Water ? "fog_underwater" : "fog_lava";
	}

  public:
	explicit ParitySubmergedScene(Liquid liquid) : liquid(liquid) {}

	const char *name() const override
	{
		return liquid == Liquid::Water ? "parity_fog_underwater" : "parity_fog_lava";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);
		measuredFrames = 90;
		room = parityBuildRoom(level, 0, 0, 13, 7, 13, Tile::rock.id, true);

		for (int_t x = room.x0; x < room.x0 + room.width; x += 3)
			for (int_t y = room.y0; y < room.y0 + 5; y += 2)
				if (level.setTile(x, y, room.z0 + room.depth, Tile::glowstone.id))
					++glowstone;

		const int_t liquidId = liquid == Liquid::Water ? Tile::water.id : Tile::lava.id;
		for (int_t x = room.x0; x < room.x0 + room.width; ++x)
			for (int_t z = room.z0; z < room.z0 + room.depth; ++z)
				for (int_t y = room.y0; y < room.y0 + 5; ++y)
					if (level.setTile(x, y, z, liquidId))
						++liquidBlocks;

		// Feet at y0 + 1 put the eye (feet + 1.74) two blocks below the surface.
		aim(room.centerX(), room.y0 + 1, room.z0 + 1.5, 0.0f, 0.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		const std::string tag = prefix();
		const Material &material = liquid == Liquid::Water ? Material::water : Material::lava;
		lines.push_back(tag + "_room_floor_y " + std::to_string(room.y0));
		lines.push_back(tag + "_liquid_blocks " + std::to_string(liquidBlocks));
		lines.push_back(tag + "_glowstone " + std::to_string(glowstone));
		lines.push_back(tag + "_eye_submerged " + std::to_string(world.player.isUnderLiquid(material)));
		lines.push_back(tag + "_player_on_fire " + std::to_string(world.player.isOnFire()));
		reportCamera(lines, tag);
	}
};

// Open-air linear fog with stone pillars marching away from the camera on three
// bearings. Equal-distance pillars off to the sides versus straight ahead are
// what separates planar fog from the eye-radial mode the compatibility path asks
// for when GL_NV_fog_distance exists, so the report records which one was live.
class ParityFogDistanceScene final : public ParityScene
{
	int_t pillars = 0;
	int_t maxDistance = 192;

  public:
	const char *name() const override
	{
		return "parity_fog_distance";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		parityFixedSky(level, 6000);

		static const double bearings[] = {-40.0, 0.0, 40.0};
		for (double bearing : bearings)
		{
			const double radians = bearing * 3.141592653589793 / 180.0;
			for (int_t distance = 16; distance <= maxDistance; distance += 16)
			{
				const int_t px = Mth::floor(std::sin(radians) * distance);
				const int_t pz = Mth::floor(std::cos(radians) * distance);
				const int_t base = parityFitY(paritySurface(level, px, pz), 4);
				for (int_t y = base; y < base + 4; ++y)
					if (level.setTile(px, y, pz, Tile::rock.id))
						++pillars;
			}
		}

		aim(0.5, parityFitY(paritySurface(level, 0, 0) + 2, 3), 0.5, 0.0f, 0.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		const int_t viewDistance = world.minecraft.options.viewDistance;
		const float renderDistance = static_cast<float>(256 >> viewDistance);
		lines.push_back("fog_distance_pillar_blocks " + std::to_string(pillars));
		lines.push_back("fog_distance_max_pillar_distance " + std::to_string(maxDistance));
		lines.push_back("fog_distance_view_distance " + std::to_string(viewDistance));
		lines.push_back("fog_distance_render_distance " + std::to_string(renderDistance));
		lines.push_back("fog_distance_linear_start " + std::to_string(renderDistance * 0.25f));
		lines.push_back("fog_distance_linear_end " + std::to_string(renderDistance));
		lines.push_back("fog_distance_nv_eye_radial " +
						std::to_string(lwjgl::GLContext::getCapabilities()["GL_NV_fog_distance"]));
		lines.push_back("fog_distance_dimension_foggy " + std::to_string(world.level.dimension->foggy));
		reportCamera(lines, "fog_distance");
	}
};

// Nether fog. The runner builds this run's Level with the Hell dimension, so the
// provider really is HellDimension: no sky pass, fog start pinned to zero, hell
// fog colour and its own brightness ramp. The chamber is carved so the frame does
// not depend on where the generator happened to put caves.
class ParityFogNetherScene final : public ParityScene
{
	ParityRoom room;
	int_t glowstone = 0;
	int_t lavaBlocks = 0;

  public:
	const char *name() const override
	{
		return "parity_fog_nether";
	}

	void setup(World &world, const Params &) override
	{
		Level &level = world.level;
		level.setTime(6000);
		room = parityBuildRoomAt(level, 0, 0, 40, 21, 6, 21, Tile::netherrack.id, true);

		for (int_t x = room.x0 + 2; x < room.x0 + room.width; x += 5)
			for (int_t z = room.z0 + 2; z < room.z0 + room.depth; z += 5)
				if (level.setTile(x, room.y0 + room.height, z, Tile::glowstone.id))
					++glowstone;

		lavaBlocks =
			parityLiquidPool(level, room.x0 + 15, room.y0, room.z0 + 15, 2, Tile::lava.id, Tile::netherrack.id);

		aim(room.x0 + 5.5, room.y0, room.z0 + 2.5, 0.0f, 5.0f);
		pin(world);
	}

	void report(World &world, std::vector<std::string> &lines) override
	{
		Level &level = world.level;
		Dimension &dimension = *level.dimension;
		Vec3 *fog = dimension.getFogColor(level.getTimeOfDay(1.0f), 1.0f);
		lines.push_back("fog_nether_dimension_id " + std::to_string(dimension.id));
		lines.push_back("fog_nether_foggy " + std::to_string(dimension.foggy));
		lines.push_back("fog_nether_ultra_warm " + std::to_string(dimension.ultraWarm));
		lines.push_back("fog_nether_has_ceiling " + std::to_string(dimension.hasCeiling));
		lines.push_back("fog_nether_time_of_day " + std::to_string(level.getTimeOfDay(1.0f)));
		lines.push_back("fog_nether_fog_color " + std::to_string(fog->x) + " " + std::to_string(fog->y) + " " +
						std::to_string(fog->z));
		lines.push_back("fog_nether_chamber_floor_y " + std::to_string(room.y0));
		lines.push_back("fog_nether_glowstone " + std::to_string(glowstone));
		lines.push_back("fog_nether_lava_pool " + std::to_string(lavaBlocks));
		lines.push_back("fog_nether_brightness_ramp_0 " + std::to_string(dimension.brightnessRamp[0]));
		reportCamera(lines, "fog_nether");
	}
};

// --- registry --------------------------------------------------------------

namespace parity
{

struct Entry
{
	const char *name;
	std::unique_ptr<Scenario> (*factory)();
	std::vector<std::string> params;
	int_t dimension;
	int maxViewDistance;
};

static const std::vector<Entry> &table()
{
	static const std::vector<Entry> entries = {
		{"parity_terrain_above",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityTerrainAboveScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_terrain_below",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityTerrainBelowScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_transparent",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityTransparentScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_texturefx",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityTextureFxScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_clouds_below",
		 []() -> std::unique_ptr<Scenario> {
			 return Util::make_unique<ParityCloudsScene>(ParityCloudsScene::Vantage::Below);
		 },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_clouds_above",
		 []() -> std::unique_ptr<Scenario> {
			 return Util::make_unique<ParityCloudsScene>(ParityCloudsScene::Vantage::Above);
		 },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_sky_day",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParitySkyScene>(ParitySkyScene::Phase::Day); },
		 {},
		 Dimension::Id_Normal,
		 1},
		{"parity_sky_night",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParitySkyScene>(ParitySkyScene::Phase::Night); },
		 {},
		 Dimension::Id_Normal,
		 1},
		{"parity_sky_dawn",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParitySkyScene>(ParitySkyScene::Phase::Dawn); },
		 {},
		 Dimension::Id_Normal,
		 1},
		{"parity_particles",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityParticlesScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_gui_hud",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityGuiHudScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_gui_inventory",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityGuiInventoryScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_sign",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParitySignScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_held_item",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityHeldItemScene>(); },
		 {"item"},
		 Dimension::Id_Normal,
		 3},
		{"parity_break_outline",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityBreakOutlineScene>(); },
		 {"target"},
		 Dimension::Id_Normal,
		 3},
		{"parity_rain",
		 []() -> std::unique_ptr<Scenario> {
			 return Util::make_unique<ParityPrecipitationScene>(ParityPrecipitationScene::Kind::Rain);
		 },
		 {"thunder", "search"},
		 Dimension::Id_Normal,
		 3},
		{"parity_snow",
		 []() -> std::unique_ptr<Scenario> {
			 return Util::make_unique<ParityPrecipitationScene>(ParityPrecipitationScene::Kind::Snow);
		 },
		 {"thunder", "search"},
		 Dimension::Id_Normal,
		 3},
		{"parity_fog_underwater",
		 []() -> std::unique_ptr<Scenario> {
			 return Util::make_unique<ParitySubmergedScene>(ParitySubmergedScene::Liquid::Water);
		 },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_fog_lava",
		 []() -> std::unique_ptr<Scenario> {
			 return Util::make_unique<ParitySubmergedScene>(ParitySubmergedScene::Liquid::Lava);
		 },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_fog_distance",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityFogDistanceScene>(); },
		 {},
		 Dimension::Id_Normal,
		 3},
		{"parity_fog_nether",
		 []() -> std::unique_ptr<Scenario> { return Util::make_unique<ParityFogNetherScene>(); },
		 {},
		 Dimension::Id_Hell,
		 3},
	};
	return entries;
}

static const Entry *find(const std::string &name)
{
	for (const Entry &entry : table())
		if (name == entry.name)
			return &entry;
	return nullptr;
}

const std::vector<std::string> &names()
{
	static const std::vector<std::string> ordered = []() {
		std::vector<std::string> result;
		result.reserve(table().size());
		for (const Entry &entry : table())
			result.emplace_back(entry.name);
		return result;
	}();
	return ordered;
}

std::unique_ptr<Scenario> make(const std::string &name)
{
	const Entry *entry = find(name);
	if (entry == nullptr)
		return nullptr;
	return entry->factory();
}

const std::vector<std::string> *params(const std::string &name)
{
	const Entry *entry = find(name);
	return entry == nullptr ? nullptr : &entry->params;
}

int_t dimension(const std::string &name)
{
	const Entry *entry = find(name);
	return entry == nullptr ? Dimension::Id_Normal : entry->dimension;
}

bool needsFullSettle(const std::string &name)
{
	return find(name) != nullptr;
}

int maxViewDistance(const std::string &name)
{
	const Entry *entry = find(name);
	return entry == nullptr ? 3 : entry->maxViewDistance;
}

} // namespace parity
} // namespace stress
