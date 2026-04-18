#include "world/level/tile/Tile.h"
#include "world/level/tile/FurnaceTile.h"
#include "world/level/tile/SlabTile.h"

#include <iostream>
#include <stdexcept>

#include "world/level/Level.h"
#include "world/level/LevelSource.h"
#include "world/entity/Entity.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/player/Player.h"
#include "world/item/ItemInstance.h"

// Tile properties
std::array<Tile *, 256> Tile::tiles;

std::array<bool, 256> Tile::shouldTick = {};
std::array<bool, 256> Tile::solid = {};
std::array<bool, 256> Tile::isEntityTile = {};
std::array<int_t, 256> Tile::lightBlock = {};
std::array<bool, 256> Tile::translucent = {};
std::array<int_t, 256> Tile::lightEmission = {};

// Step sounds
StepSound Tile::soundPowderFootstep(u"stone", 1.0f, 1.0f);
StepSound Tile::soundWoodFootstep(u"wood", 1.0f, 1.0f);
StepSound Tile::soundGravelFootstep(u"gravel", 1.0f, 1.0f);
StepSound Tile::soundGrassFootstep(u"grass", 1.0f, 1.0f);
StepSound Tile::soundStoneFootstep(u"stone", 1.0f, 1.0f);
StepSound Tile::soundMetalFootstep(u"stone", 1.0f, 1.5f);
StepSoundStone Tile::soundGlassFootstep(u"stone", 1.0f, 1.0f);
StepSound Tile::soundClothFootstep(u"cloth", 1.0f, 1.0f);
StepSoundSand Tile::soundSandFootstep(u"sand", 1.0f, 1.0f);

// Tiles
#include "world/level/tile/StoneTile.h"
#include "world/level/tile/GrassTile.h"
#include "world/level/tile/DirtTile.h"
#include "world/level/tile/WoodTile.h"
#include "world/level/tile/SandTile.h"
#include "world/level/tile/SandStoneTile.h"
#include "world/level/tile/GravelTile.h"
#include "world/level/tile/TreeTile.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/tile/FlowerTile.h"
#include "world/level/tile/CropsTile.h"
#include "world/level/tile/FarmlandTile.h"
#include "world/level/tile/TallGrassTile.h"
#include "world/level/tile/DeadBushTile.h"
#include "world/level/tile/MushroomTile.h"
#include "world/level/tile/ReedTile.h"
#include "world/level/tile/CactusTile.h"
#include "world/level/tile/PumpkinTile.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/IceTile.h"
#include "world/level/tile/WorkbenchTile.h"
#include "world/level/tile/TransparentTile.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/material/LiquidMaterial.h"

StoneTile Tile::rock = StoneTile(1, 1);
GrassTile Tile::grass = GrassTile(2);
DirtTile Tile::dirt = DirtTile(3, 2);
WoodTile Tile::wood = WoodTile(5, 4);

SandTile Tile::sand = SandTile(12, 18);
GravelTile Tile::gravel = GravelTile(13, 19);

TreeTile Tile::treeTrunk = TreeTile(17);
LeafTile Tile::leaves = LeafTile(18, 52);
TallGrassTile Tile::tallGrass = TallGrassTile(31, 39);
DeadBushTile Tile::deadBush = DeadBushTile(32, 55);
FlowerTile Tile::flower = FlowerTile(37, 13);
FlowerTile Tile::rose = FlowerTile(38, 12);
MushroomTile Tile::brownMushroom = MushroomTile(39, 29);
MushroomTile Tile::redMushroom = MushroomTile(40, 28);

Tile Tile::cobblestone = Tile(4, 16, Material::stone);
Tile Tile::bedrock = Tile(7, 17, Material::stone);
static LiquidTileDynamic waterTile(8, 205, Material::water);
LiquidTile &Tile::water = waterTile;
static LiquidTileStatic calmWaterTile(9, 205, Material::water);
LiquidTile &Tile::calmWater = calmWaterTile;
static LiquidTileDynamic lavaTile(10, 237, Material::lava);
LiquidTile &Tile::lava = lavaTile;
static LiquidTileStatic calmLavaTile(11, 237, Material::lava);
LiquidTile &Tile::calmLava = calmLavaTile;
Tile Tile::goldOre = Tile(14, 32, Material::stone);
Tile Tile::ironOre = Tile(15, 33, Material::stone);
Tile Tile::coalOre = Tile(16, 34, Material::stone);
Tile Tile::lapisOre = Tile(21, 160, Material::stone);
static SandStoneTile sandstoneTile(24, 192);
Tile &Tile::sandstone = sandstoneTile;
SlabTile Tile::slabDouble = SlabTile(43, true);
SlabTile Tile::slabSingle = SlabTile(44, false);
Tile Tile::mossyCobblestone = Tile(48, 36, Material::stone);
Tile Tile::obsidian = Tile(49, 37, Material::stone);
Tile Tile::diamondOre = Tile(56, 50, Material::stone);
WorkbenchTile Tile::workBench = WorkbenchTile(58);
CropsTile Tile::crops = CropsTile(59, 88);
FarmlandTile Tile::farmland = FarmlandTile(60);
FurnaceTile Tile::furnace = FurnaceTile(61, false);
FurnaceTile Tile::furnaceLit = FurnaceTile(62, true);
Tile Tile::redstoneOre = Tile(73, 51, Material::stone);
SnowTile Tile::snow = SnowTile(78, 66);
IceTile Tile::ice = IceTile(79, 67);
CactusTile Tile::cactus = CactusTile(81, 70);
Tile Tile::clay = Tile(82, 72, Material::clay);
ReedTile Tile::reed = ReedTile(83, 73);
PumpkinTile Tile::pumpkin = PumpkinTile(86, 102);

void Tile::initTiles()
{
	rock.setDestroyTime(1.5f).setSoundType(soundStoneFootstep);
	grass.setDestroyTime(0.6f).setSoundType(soundGrassFootstep);
	dirt.setDestroyTime(0.5f).setSoundType(soundGravelFootstep);
	wood.setDestroyTime(2.0f).setSoundType(soundWoodFootstep);

	sand.setDestroyTime(0.5f).setSoundType(soundSandFootstep);
	gravel.setDestroyTime(0.6f).setSoundType(soundGravelFootstep);

	treeTrunk.setDestroyTime(2.0f).setSoundType(soundWoodFootstep);
	leaves.setDestroyTime(0.2f).setLightBlock(1).setSoundType(soundGrassFootstep);
	tallGrass.setDestroyTime(0.0f).setSoundType(soundGrassFootstep);
	deadBush.setDestroyTime(0.0f).setSoundType(soundGrassFootstep);
	flower.setDestroyTime(0.0f).setSoundType(soundGrassFootstep);
	rose.setDestroyTime(0.0f).setSoundType(soundGrassFootstep);
	brownMushroom.setDestroyTime(0.0f).setLightEmission(2).setSoundType(soundGrassFootstep);
	redMushroom.setDestroyTime(0.0f).setSoundType(soundGrassFootstep);
	bedrock.setDestroyTime(-1.0f).setSoundType(soundStoneFootstep);
	reed.setDestroyTime(0.0f).setSoundType(soundGrassFootstep);
	pumpkin.setDestroyTime(1.0f).setSoundType(soundWoodFootstep);
	cactus.setDestroyTime(0.4f).setSoundType(soundClothFootstep);

	cobblestone.setDestroyTime(2.0f).setSoundType(soundStoneFootstep);
	sandstone.setDestroyTime(0.8f).setSoundType(soundStoneFootstep);
	slabDouble.setDestroyTime(2.0f).setSoundType(soundStoneFootstep);
	slabSingle.setDestroyTime(2.0f).setSoundType(soundStoneFootstep);
	mossyCobblestone.setDestroyTime(2.0f).setSoundType(soundStoneFootstep);
	obsidian.setDestroyTime(10.0f).setSoundType(soundStoneFootstep);
	workBench.setDestroyTime(2.5f).setSoundType(soundWoodFootstep);
	crops.setDestroyTime(0.0f).setSoundType(soundGrassFootstep);
	farmland.setDestroyTime(0.6f).setSoundType(soundGravelFootstep);
	furnace.setDestroyTime(3.5f).setSoundType(soundStoneFootstep);
	furnaceLit.setDestroyTime(3.5f).setSoundType(soundStoneFootstep).setLightEmission(14);

	goldOre.setDestroyTime(3.0f).setSoundType(soundStoneFootstep);
	ironOre.setDestroyTime(3.0f).setSoundType(soundStoneFootstep);
	coalOre.setDestroyTime(3.0f).setSoundType(soundStoneFootstep);
	lapisOre.setDestroyTime(3.0f).setSoundType(soundStoneFootstep);
	diamondOre.setDestroyTime(3.0f).setSoundType(soundStoneFootstep);
	redstoneOre.setDestroyTime(3.0f).setSoundType(soundStoneFootstep);
	snow.setDestroyTime(0.1f).setSoundType(soundClothFootstep);
	ice.setDestroyTime(0.5f).setLightBlock(3).setSoundType(soundGlassFootstep);
	clay.setDestroyTime(0.6f).setSoundType(soundGravelFootstep);

	Tile::lightBlock[8] = 3;
	Tile::lightBlock[9] = 3;
	Tile::lightBlock[10] = 255;
	Tile::lightBlock[11] = 255;

	Tile::lightEmission[10] = 15;
	Tile::lightEmission[11] = 15;
}

// Impl
const jstring Tile::TILE_DESCRIPTION_PREFIX = u"tile.";

Tile::Tile(int_t id, const Material &material) : material(material)
{
	if (tiles.at(id) != nullptr)
		throw std::runtime_error("Slot " + std::to_string(id) + " is already occupied");
	tiles[id] = this;

	this->id = id;
	
	setShape(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	
	updateCachedProperties();
	isEntityTile[id] = false;
	soundType = &soundPowderFootstep;
}

Tile::Tile(int_t id, int_t tex, const Material &material) : Tile(id, material)
{
	this->tex = tex;
}

Tile &Tile::setLightBlock(int_t lightBlock)
{
	Tile::lightBlock[id] = lightBlock;
	return *this;
}

Tile &Tile::setLightEmission(int_t lightEmission)
{
	Tile::lightEmission[id] = lightEmission;
	return *this;
}

Tile &Tile::setExplodeable(float resistance)
{
	explosionResistance = resistance * 3.0f;
	return *this;
}

bool Tile::isTranslucent()
{
	return false;
}

bool Tile::isCubeShaped()
{
	return true;
}

Tile::Shape Tile::getRenderShape()
{
	return SHAPE_BLOCK;
}

Tile &Tile::setDestroyTime(float time)
{
	destroySpeed = time;
	return *this;
}

Tile &Tile::setSoundType(StepSound &sound)
{
	soundType = &sound;
	return *this;
}

void Tile::setTicking(bool ticking)

{
	shouldTick[id] = ticking;
}

void Tile::updateCachedProperties()
{
	solid[id] = isSolidRender();
	lightBlock[id] = isSolidRender() ? 255 : 0;
	translucent[id] = isTranslucent();
}

void Tile::setShape(float x0, float y0, float z0, float x1, float y1, float z1)
{
	xx0 = x0;
	yy0 = y0;
	zz0 = z0;
	xx1 = x1;
	yy1 = y1;
	zz1 = z1;
}

float Tile::getBrightness(LevelSource &level, int_t x, int_t y, int_t z)
{
	return level.getBrightness(x, y, z);
}

bool Tile::isFaceVisible(LevelSource &level, int_t x, int_t y, int_t z, Facing face)
{
	if (face == Facing::DOWN)
		y--;
	if (face == Facing::UP)
		y++;
	if (face == Facing::NORTH)
		z--;
	if (face == Facing::SOUTH)
		z++;
	if (face == Facing::WEST)
		x--;
	if (face == Facing::EAST)
		x++;
	return !level.isSolidTile(x, y, z);
}

bool Tile::shouldRenderFace(LevelSource &level, int_t x, int_t y, int_t z, Facing face)
{
	if (face == Facing::DOWN && yy0 > 0.0)
		return true;
	else if (face == Facing::UP && yy1 < 1.0)
		return true;
	else if (face == Facing::NORTH && zz0 > 0.0)
		return true;
	else if (face == Facing::SOUTH && zz1 < 1.0)
		return true;
	else if (face == Facing::WEST && xx0 > 0.0)
		return true;
	else if (face == Facing::EAST && xx1 < 1.0)
		return true;
	else
		return !level.isSolidTile(x, y, z);
}

int_t Tile::getTexture(LevelSource &level, int_t x, int_t y, int_t z, Facing face)
{
	return getTexture(face, level.getData(x, y, z));
}

int_t Tile::getTexture(Facing face, int_t data)
{
	return getTexture(face);
}

int_t Tile::getTexture(Facing face)
{
	return tex;
}

AABB *Tile::getTileAABB(Level &level, int_t x, int_t y, int_t z)
{
	return AABB::newTemp(x + xx0, y + yy0, z + zz0, x + xx1, y + yy1, z + zz1);
}

void Tile::addAABBs(Level &level, int_t x, int_t y, int_t z, AABB &bb, std::vector<AABB *> &aabbList)
{
	AABB *aabb = getAABB(level, x, y, z);
	if (aabb != nullptr && aabb->intersects(bb))
		aabbList.push_back(aabb);
}

AABB *Tile::getAABB(Level &level, int_t x, int_t y, int_t z)
{
	return AABB::newTemp(x + xx0, y + yy0, z + zz0, x + xx1, y + yy1, z + zz1);
}

bool Tile::isSolidRender()
{
	return true;
}

bool Tile::mayPick(int_t data, bool canPickLiquid)
{
	return mayPick();
}

bool Tile::mayPick()
{
	return true;
}

void Tile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{

}

void Tile::animateTick(Level &level, int_t x, int_t y, int_t z, Random &random)
{

}

void Tile::destroy(Level &level, int_t x, int_t y, int_t z, int_t data)
{

}

void Tile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{

}

void Tile::addLights(Level &level, int_t x, int_t y, int_t z)
{

}

int_t Tile::getTickDelay()
{
	return 10;
}

void Tile::onPlace(Level &level, int_t x, int_t y, int_t z)
{

}

void Tile::onRemove(Level &level, int_t x, int_t y, int_t z)
{

}

int_t Tile::getResourceCount(Random &random)
{
	return 1;
}

int_t Tile::getResource(int_t data, Random &random)
{
	return id;
}

float Tile::getDestroyProgress(Player &player)
{
	if (destroySpeed < 0.0f)
		return 0.0f;

	if (!player.canDestroy(*this))
		return 1.0f / destroySpeed / 100.0f;

	return player.getDestroySpeed(*this) / destroySpeed / 30.0f;
}

void Tile::spawnResources(Level &level, int_t x, int_t y, int_t z, int_t data)
{
	spawnResources(level, x, y, z, data, 1.0f);
}

void Tile::spawnResources(Level &level, int_t x, int_t y, int_t z, int_t data, float chance)
{
	if (level.isOnline)
		return;

	int_t resourceCount = getResourceCount(level.random);
	for (int_t i = 0; i < resourceCount; ++i)
	{
		if (level.random.nextFloat() > chance)
			continue;

		int_t resource = getResource(data, level.random);
		if (resource <= 0)
			continue;

		float spread = 0.7f;
		double xo = level.random.nextFloat() * spread + (1.0f - spread) * 0.5f;
		double yo = level.random.nextFloat() * spread + (1.0f - spread) * 0.5f;
		double zo = level.random.nextFloat() * spread + (1.0f - spread) * 0.5f;

		ItemInstance stack(resource, 1, getSpawnResourcesAuxValue(data));
		auto entity = std::make_shared<EntityItem>(level, x + xo, y + yo, z + zo, stack);
		entity->throwTime = 10;
		level.addEntity(entity);
		std::cerr << "[Drop] Spawned item=" << resource
			<< " aux=" << stack.itemDamage
			<< " fromTile=" << id
			<< " data=" << data
			<< " blockPos=(" << x << "," << y << "," << z << ")"
			<< " entityPos=(" << (x + xo) << "," << (y + yo) << "," << (z + zo) << ")"
			<< std::endl;
	}
}

int_t Tile::getSpawnResourcesAuxValue(int_t data)
{
	return 0;
}

HitResult Tile::clip(Level &level, int_t x, int_t y, int_t z, Vec3 &from, Vec3 &to)
{
	updateShape(level, x, y, z);

	Vec3 *localFrom = from.add(-x, -y, -z);
	Vec3 *localTo = to.add(-x, -y, -z);
	Vec3 *cxx0 = localFrom->clipX(*localTo, xx0);
	Vec3 *cxx1 = localFrom->clipX(*localTo, xx1);
	Vec3 *cyy0 = localFrom->clipY(*localTo, yy0);
	Vec3 *cyy1 = localFrom->clipY(*localTo, yy1);
	Vec3 *czz0 = localFrom->clipZ(*localTo, zz0);
	Vec3 *czz1 = localFrom->clipZ(*localTo, zz1);

	if (!containsX(cxx0)) cxx0 = nullptr;
	if (!containsX(cxx1)) cxx1 = nullptr;
	if (!containsY(cyy0)) cyy0 = nullptr;
	if (!containsY(cyy1)) cyy1 = nullptr;
	if (!containsZ(czz0)) czz0 = nullptr;
	if (!containsZ(czz1)) czz1 = nullptr;

	Vec3 *pick = nullptr;
	if (cxx0 != nullptr && (pick == nullptr || localFrom->distanceTo(*cxx0) < localFrom->distanceTo(*pick))) pick = cxx0;
	if (cxx1 != nullptr && (pick == nullptr || localFrom->distanceTo(*cxx1) < localFrom->distanceTo(*pick))) pick = cxx1;
	if (cyy0 != nullptr && (pick == nullptr || localFrom->distanceTo(*cyy0) < localFrom->distanceTo(*pick))) pick = cyy0;
	if (cyy1 != nullptr && (pick == nullptr || localFrom->distanceTo(*cyy1) < localFrom->distanceTo(*pick))) pick = cyy1;
	if (czz0 != nullptr && (pick == nullptr || localFrom->distanceTo(*czz0) < localFrom->distanceTo(*pick))) pick = czz0;
	if (czz1 != nullptr && (pick == nullptr || localFrom->distanceTo(*czz1) < localFrom->distanceTo(*pick))) pick = czz1;
	if (pick == nullptr)
		return HitResult();

	Facing face = Facing::NONE;
	if (pick == cxx0)
		face = Facing::WEST;
	if (pick == cxx1)
		face = Facing::EAST;
	if (pick == cyy0)
		face = Facing::DOWN;
	if (pick == cyy1)
		face = Facing::UP;
	if (pick == czz0)
		face = Facing::NORTH;
	if (pick == czz1)
		face = Facing::SOUTH;

	return HitResult(x, y, z, face, *pick->add(x, y, z));
}

bool Tile::containsX(Vec3 *vec)
{
	if (vec == nullptr)
		return false;
	return vec->y >= yy0 && vec->y <= yy1 && vec->z >= zz0 && vec->z <= zz1;
}

bool Tile::containsY(Vec3 *vec)
{
	if (vec == nullptr)
		return false;
	return vec->x >= xx0 && vec->x <= xx1 && vec->z >= zz0 && vec->z <= zz1;
}

bool Tile::containsZ(Vec3 *vec)
{
	if (vec == nullptr)
		return false;
	return vec->x >= xx0 && vec->x <= xx1 && vec->y >= yy0 && vec->y <= yy1;
}

int_t Tile::getRenderLayer()
{
	return 0;
}

void Tile::stepOn(Level &level, int_t x, int_t y, int_t z, Entity &entity)
{

}

void Tile::setPlacedOnFace(Level &level, int_t x, int_t y, int_t z, Facing face)
{
	(void)level;
	(void)x;
	(void)y;
	(void)z;
	(void)face;
}

void Tile::setPlacedBy(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	(void)level;
	(void)x;
	(void)y;
	(void)z;
	(void)player;
}

void Tile::prepareRender(Level &level, int_t x, int_t y, int_t z)
{

}

void Tile::attack(Level &level, int_t x, int_t y, int_t z, Player &player)
{

}

bool Tile::use(Level &level, int_t x, int_t y, int_t z, Player &player)
{
	(void)level;
	(void)x;
	(void)y;
	(void)z;
	(void)player;
	return false;
}

void Tile::updateShape(LevelSource &level, int_t x, int_t y, int_t z)
{

}

int_t Tile::getColor(LevelSource &level, int_t x, int_t y, int_t z)
{
	return 0xFFFFFF;
}

void Tile::entityInside(Level &level, int_t x, int_t y, int_t z, Entity &entity)
{

}

void Tile::updateDefaultShape()
{

}

void Tile::playerDestroy(Level &level, int_t x, int_t y, int_t z, int_t data)
{
	spawnResources(level, x, y, z, data);
}
