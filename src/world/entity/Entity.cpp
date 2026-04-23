#include "world/entity/Entity.h"

#include "world/entity/item/EntityItem.h"
#include "world/level/Level.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/StepSound.h"

#include <cmath>
#include "util/Mth.h"

int_t Entity::entityCounter = 0;

Entity::Entity(Level &level) : level(level)
{
	setPos(0.0, 0.0, 0.0);
	// TODO
}

void Entity::resetPos()
{
	while (y > 0.0)
	{
		setPos(x, y, z);
		if (level.getCubes(*this, bb).empty())
			break;
		y++;
	}

	xd = yd = zd = 0.0;
	xRot = 0.0f;
}

void Entity::remove()
{
	removed = true;
}

void Entity::setSize(float width, float height)
{
	bbWidth = width;
	bbHeight = height;
}

void Entity::setPos(const EntityPos &pos)
{
	if (pos.move)
		setPos(pos.x, pos.y, pos.z);
	else
		setPos(x, y, z);

	if (pos.rot)
		setRot(pos.yRot, pos.xRot);
	else
		setRot(yRot, xRot);
}

void Entity::setRot(float yRot, float xRot)
{
	this->yRot = yRot;
	this->xRot = xRot;
}

void Entity::setPos(double x, double y, double z)
{
	this->x = x;
	this->y = y;
	this->z = z;

	float hw = bbWidth / 2.0f;
	float h = bbHeight;
	bb.set(x - hw, y - heightOffset + ySlideOffset, z - hw, x + hw, y - heightOffset + ySlideOffset + h, z + hw);
}

void Entity::turn(float yRot, float xRot)
{
	float ox = this->xRot;
	float oy = this->yRot;

	this->yRot += yRot * 0.15;
	this->xRot -= xRot * 0.15;
	if (this->xRot < -90.0f) this->xRot = -90.0f;
	if (this->xRot > 90.0f) this->xRot = 90.0f;

	xRotO += this->xRot - ox;
	yRotO += this->yRot - oy;
}

void Entity::interpolateTurn(float yRot, float xRot)
{
	this->yRot = this->yRot + yRot * 0.15;
	this->xRot = this->xRot - xRot * 0.15;
	if (this->xRot < -90.0f) this->xRot = -90.0f;
	if (this->xRot > 90.0f) this->xRot = 90.0f;
}

void Entity::tick()
{
	baseTick();
}

void Entity::baseTick()
{
	if (riding != nullptr && riding->removed)
		riding = nullptr;

	tickCount++;

	// Setup interpolation
	walkDistO = walkDist;
	xo = x;
	yo = y;
	zo = z;
	yRotO = yRot;
	xRotO = xRot;

	// Water check
	if (isInWater())
	{
		if (!wasInWater && !firstTick)
		{
			float splashVolume = Mth::sqrt(xd * xd * 0.2f + yd * yd + zd * zd * 0.2f) * 0.2f;
			if (splashVolume > 1.0f)
				splashVolume = 1.0f;
			level.playSoundAtEntity(*this, u"random.splash", splashVolume, 1.0f + (random.nextFloat() - random.nextFloat()) * 0.4f);

			float waterY = static_cast<float>(Mth::floor(bb.y0));
			for (int_t i = 0; i < static_cast<int_t>(1.0f + bbWidth * 20.0f); i++)
			{
				float offsetX = (random.nextFloat() * 2.0f - 1.0f) * bbWidth;
				float offsetZ = (random.nextFloat() * 2.0f - 1.0f) * bbWidth;
				level.addParticle(u"bubble", x + offsetX, waterY + 1.0f, z + offsetZ, xd, yd - random.nextFloat() * 0.2f, zd);
			}

			for (int_t i = 0; i < static_cast<int_t>(1.0f + bbWidth * 20.0f); i++)
			{
				float offsetX = (random.nextFloat() * 2.0f - 1.0f) * bbWidth;
				float offsetZ = (random.nextFloat() * 2.0f - 1.0f) * bbWidth;
				level.addParticle(u"splash", x + offsetX, waterY + 1.0f, z + offsetZ, xd, yd, zd);
			}
		}

		fallDistance = 0.0f;
		wasInWater = true;
		if (onFire > 0)
			level.playSoundAtEntity(*this, u"random.fizz", 0.7f, 1.6f + (random.nextFloat() - random.nextFloat()) * 0.4f);
		onFire = 0;
	}
	else
	{
		wasInWater = false;
	}

	// Fire check
	if (level.isOnline)
	{
		onFire = 0;
	}
	else
	{
		if (onFire > 0)
		{
			if (fireImmune)
			{
				onFire -= 4;
				if (onFire < 0) onFire = 0;
			}
			else
			{
				if (onFire % 20 == 0)
					hurt(nullptr, 1);
				onFire--;
			}
		}
	}

	// Lava check
	if (isInLava())
		lavaHurt();

	// Out of world check
	if (y < -64.0)
		outOfWorld();

	// Flag check
	if (!level.isOnline)
	{
		setSharedFlag(FLAG_ONFIRE, onFire > 0);
		setSharedFlag(FLAG_RIDING, riding != nullptr);
	}

	firstTick = false;
}

void Entity::lavaHurt()
{
	if (!fireImmune)
	{
		hurt(nullptr, 4);
		onFire = 600;
	}
}

void Entity::outOfWorld()
{
	remove();
}

bool Entity::isFree(double xd, double yd, double zd, double skin)
{
	AABB *tbb = bb.grow(skin, skin, skin)->cloneMove(xd, yd, zd);
	auto cubes = level.getCubes(*this, *tbb);
	return !cubes.empty() ? false : !level.containsAnyLiquid(*tbb);
}

bool Entity::isFree(double xd, double yd, double zd)
{
	AABB *tbb = bb.cloneMove(xd, yd, zd);
	auto cubes = level.getCubes(*this, *tbb);
	return !cubes.empty() ? false : !level.containsAnyLiquid(*tbb);
}

void Entity::move(double xd, double yd, double zd)
{
	if (noPhysics)
	{
		bb.move(xd, yd, zd);
		x = (bb.x0 + bb.x1) / 2.0;
		y = bb.y0 + heightOffset - ySlideOffset;
		z = (bb.z0 + bb.z1) / 2.0;
		return;
	}

	if (isInWeb)
	{
		isInWeb = false;
		xd *= 0.25;
		yd *= 0.05;
		zd *= 0.25;
		this->xd = 0.0;
		this->yd = 0.0;
		this->zd = 0.0;
	}

	// xd *= 20.0f;
	// yd *= 6.0f;
	// zd *= 20.0f;

	double ox = x;
	double oz = z;
	double oxd = xd;
	double oyd = yd;
	double ozd = zd;

	AABB *oldBb = bb.copy();

	// Keep us from moving off platforms when sneaking
	bool sneaking = onGround && isSneaking();
	if (sneaking)
	{
		double safeStep = 0.05;

		while (xd != 0.0 && level.getCubes(*this, *bb.cloneMove(xd, -1.0, 0.0)).empty())
		{
			if (xd < safeStep && xd >= -safeStep)
				xd = 0.0;
			else if (xd > 0.0)
				xd -= safeStep;
			else
				xd += safeStep;
			oxd = xd;
		}

		while (zd != 0.0 && level.getCubes(*this, *bb.cloneMove(0.0, -1.0, zd)).empty())
		{
			if (zd < safeStep && zd >= -safeStep)
				zd = 0.0;
			else if (zd > 0.0)
				zd -= safeStep;
			else
				zd += safeStep;
			ozd = zd;
		}
	}

	// Clip against collisions
	const auto &cubes = level.getCubes(*this, *bb.expand(xd, yd, zd));

	for (auto &cube : cubes)
		yd = cube->clipYCollide(bb, yd);
	bb.move(0.0, yd, 0.0);
	if (!slide && oyd != yd)
		xd = yd = zd = 0;
	bool grounded = onGround || (oyd != yd && oyd < 0.0);

	for (auto &cube : cubes)
		xd = cube->clipXCollide(bb, xd);
	bb.move(xd, 0.0, 0.0);
	if (!slide && oxd != xd)
		xd = yd = zd = 0;

	for (auto &cube : cubes)
		zd = cube->clipZCollide(bb, zd);
	bb.move(0.0, 0.0, zd);
	if (!slide && ozd != zd)
		xd = yd = zd = 0;

	// Check for steps
	if (footSize > 0.0f && grounded && ySlideOffset < 0.05f && (oxd != xd || ozd != zd))
	{
		double ooxd = xd;
		double ooyd = yd;
		double oozd = zd;

		xd = oxd;
		yd = footSize;
		zd = ozd;

		AABB *obb = bb.copy();
		bb.set(*obb);

		const auto &cubes = level.getCubes(*this, *bb.expand(xd, yd, zd));

		for (auto &cube : cubes)
			yd = cube->clipYCollide(bb, yd);
		bb.move(0.0, yd, 0.0);
		if (!slide && oyd != yd)
			xd = yd = zd = 0;

		for (auto &cube : cubes)
			xd = cube->clipXCollide(bb, xd);
		bb.move(xd, 0.0, 0.0);
		if (!slide && oxd != xd)
			xd = yd = zd = 0;

		for (auto &cube : cubes)
			zd = cube->clipZCollide(bb, zd);
		bb.move(0.0, 0.0, zd);
		if (!slide && ozd != zd)
			xd = yd = zd = 0;

		// Step back down
		yd = -footSize;
		for (auto &cube : level.getCubes(*this, *bb.expand(0.0, yd, 0.0)))
			yd = cube->clipYCollide(bb, yd);
		bb.move(0.0, yd, 0.0);

		if (ooxd * ooxd + oozd * oozd >= xd * xd + zd * zd)
		{
			xd = ooxd;
			yd = ooyd;
			zd = oozd;
			bb.set(*obb);
		}
		else
		{
			ySlideOffset += (bb.y0 - std::floor(bb.y0)) + 0.01;
		}
	}

	// Update body
	x = (bb.x0 + bb.x1) / 2.0;
	y = bb.y0 + heightOffset - ySlideOffset;
	z = (bb.z0 + bb.z1) / 2.0;

	horizontalCollision = oxd != xd || ozd != zd;
	verticalCollision = oyd != yd;
	onGround = oyd != yd && oyd < 0.0;
	collision = horizontalCollision || verticalCollision;
	
	checkFallDamage(oyd, onGround);

	if (oxd != xd)
		this->xd = 0.0;
	if (oyd != yd)
		this->yd = 0.0;
	if (ozd != zd)
		this->zd = 0.0;

	// Footsteps
	float fdx = x - ox;
	float fdz = z - oz;

	if (makeStepSound && !sneaking)
	{
		walkDist += Mth::sqrt(fdx * fdx + fdz * fdz) * 0.6;

		int_t sx = Mth::floor(x);
		int_t sy = Mth::floor(y - 0.2 - heightOffset);
		int_t sz = Mth::floor(z);

		int_t stile = level.getTile(sx, sy, sz);
		if (walkDist > nextStep && stile > 0)
		{
			nextStep++;

			if (Tile::tiles[stile]->soundType != nullptr)
			{
				StepSound *ss = Tile::tiles[stile]->soundType;
				// Check for snow on top of block (Java Entity.java:450-451)
				int_t aboveTile = level.getTile(sx, Mth::floor(y), sz);
				if (aboveTile == Tile::snow.id)
					ss = Tile::snow.soundType;
				level.playSoundAtEntity(*this, ss->getStepResourcePath(), ss->getVolume() * 0.15f, ss->getPitch());
			}
			Tile::tiles[stile]->stepOn(level, sx, sy, sz, *this);
		}
	}

	// Check tile collisions
	int_t x0 = Mth::floor(bb.x0);
	int_t x1 = Mth::floor(bb.x1);
	int_t y0 = Mth::floor(bb.y0);
	int_t y1 = Mth::floor(bb.y1);
	int_t z0 = Mth::floor(bb.z0);
	int_t z1 = Mth::floor(bb.z1);

	if (level.hasChunksAt(x0, y0, z0, x1, y1, z1))
	{
		for (int_t x = x0; x <= x1; x++)
		{
			for (int_t y = y0; y <= y1; y++)
			{
				for (int_t z = z0; z <= z1; z++)
				{
					int_t tile = level.getTile(x, y, z);
					if (tile > 0)
						Tile::tiles[tile]->entityInside(level, x, y, z, *this);
				}
			}
		}
	}

	ySlideOffset *= 0.4f;

	// Fire collisions
	
	// xd /= 20.0f;
	// yd /= 6.0f;
	// zd /= 20.0f;
}

void Entity::checkFallDamage(double yd, bool onGround)
{
	if (onGround)
	{
		if (fallDistance > 0.0f)
		{
			causeFallDamage(fallDistance);
			fallDistance = 0.0f;
		}
	}
	else if (yd < 0.0)
	{
		fallDistance = static_cast<float>(fallDistance - yd);
	}
}

AABB *Entity::getCollideBox()
{
	return nullptr;
}

void Entity::burn(int_t a0)
{

}

void Entity::causeFallDamage(float distance)
{

}

bool Entity::isInWater()
{
	return level.handleMaterialAcceleration(*bb.grow(0.0, -0.4, 0.0)->grow(-0.001, -0.001, -0.001), Material::water, *this);
}

bool Entity::isUnderLiquid(const Material &material)
{
	double eyeY = y + static_cast<double>(getHeadHeight());
	int_t bx = Mth::floor(x);
	int_t by = Mth::floor(static_cast<float>(Mth::floor(eyeY)));
	int_t bz = Mth::floor(z);
	int_t t = level.getTile(bx, by, bz);
	if (t != 0 && Tile::tiles[t] != nullptr && &Tile::tiles[t]->material == &material)
	{
		int_t meta = level.getData(bx, by, bz);
		float pct = LiquidTile::getHeight(meta) - 1.0f / 9.0f;
		float surface = static_cast<float>(by + 1) - pct;
		return eyeY < static_cast<double>(surface);
	}
	return false;
}

float Entity::getHeadHeight()
{
	return 0.0f;
}

bool Entity::isInLava()
{
	return level.isMaterialInBB(*bb.grow(-0.1, -0.4, -0.1), Material::lava);
}

void Entity::moveRelative(float x, float z, float acc)
{
	float targetSqr = Mth::sqrt(x * x + z * z);
	if (!(targetSqr < 0.01F))
	{
		if (targetSqr < 1.0F) targetSqr = 1.0F;

		targetSqr = acc / targetSqr;
		x *= targetSqr;
		z *= targetSqr;

		float s = Mth::sin(yRot * Mth::PI / 180.0F);
		float c = Mth::cos(yRot * Mth::PI / 180.0F);
		xd += x * c - z * s;
		zd += z * c + x * s;
	}
}

float Entity::getBrightness(float a)
{
	int_t x = Mth::floor(this->x);
	double byo = (bb.y1 - bb.y0) * 0.66;
	int_t y = Mth::floor(this->y - heightOffset + byo);
	int_t z = Mth::floor(this->z);
	return level.hasChunksAt(Mth::floor(bb.x0), Mth::floor(bb.y0), Mth::floor(bb.z0), Mth::floor(bb.x1), Mth::floor(bb.y1), Mth::floor(bb.z1)) ? level.getBrightness(x, y, z) : 0.0f;
}

void Entity::absMoveTo(double x, double y, double z, float yRot, float xRot)
{
	xo = this->x = x;
	yo = this->y = y + heightOffset;
	zo = this->z = z;
	yRotO = this->yRot = yRot;
	xRotO = this->xRot = xRot;
	ySlideOffset = 0.0f;

	double dyRot = (yRotO - yRot);
	if (dyRot < -180.0)
		yRotO += 360.0;
	if (dyRot >= 180.0)
		yRotO -= 360.0;

	setPos(x, y, z);
	setRot(yRot, xRot);
}

void Entity::moveTo(double x, double y, double z, float yRot, float xRot)
{
	xOld = xo = this->x = x;
	yOld = yo = this->y = y + heightOffset;
	zOld = zo = this->z = z;
	this->yRot = yRot;
	this->xRot = xRot;
	setPos(x, y, z);
}

float Entity::distanceTo(const Entity &entity)
{
	float dx = x - entity.x;
	float dy = y - entity.y;
	float dz = z - entity.z;
	return Mth::sqrt(dx * dx + dy * dy + dz * dz);
}

double Entity::distanceToSqr(double x, double y, double z)
{
	float dx = this->x - x;
	float dy = this->y - y;
	float dz = this->z - z;
	return dx * dx + dy * dy + dz * dz;
}

double Entity::distanceTo(double x, double y, double z)
{
	float dx = this->x - x;
	float dy = this->y - y;
	float dz = this->z - z;
	return Mth::sqrt(dx * dx + dy * dy + dz * dz);
}

double Entity::distanceToSqr(const Entity &entity)
{
	float dx = x - entity.x;
	float dy = y - entity.y;
	float dz = z - entity.z;
	return dx * dx + dy * dy + dz * dz;
}

void Entity::playerTouch(Player &player)
{

}

void Entity::push(Entity &entity)
{
	if (entity.rider.get() == this || entity.riding.get() == this)
		return;
	double dx = entity.x - x;
	double dz = entity.z - z;
	double distSqr = dx * dx + dz * dz;
	if (distSqr < 1.0e-4)
		return;

	double dist = std::sqrt(distSqr);
	dx /= dist;
	dz /= dist;
	double scale = 1.0 / dist;
	if (scale > 1.0)
		scale = 1.0;
	dx *= scale * 0.05 * (1.0f - pushthrough);
	dz *= scale * 0.05 * (1.0f - pushthrough);
	push(-dx, 0.0, -dz);
	entity.push(dx, 0.0, dz);
}

void Entity::push(double x, double y, double z)
{
	xd += x;
	yd += y;
	zd += z;
}

void Entity::markHurt()
{
	hurtMarked = true;
}

bool Entity::hurt(Entity *source, int_t dmg)
{
	return false;
}

bool Entity::intersects(double x0, double y0, double z0, double x1, double y1, double z1)
{
	return false;
}

bool Entity::isPickable()
{
	return false;
}

bool Entity::isPushable()
{
	return false;
}

bool Entity::isShootable()
{
	return false;
}

void Entity::awardKillScore(Entity &source, int_t dmg)
{

}

bool Entity::shouldRender(Vec3 &v)
{
	double dx = x - v.x;
	double dy = y - v.y;
	double dz = z - v.z;
	double d = dx * dx + dy * dy + dz * dz;
	return shouldRenderAtSqrDistance(d);
}

bool Entity::shouldRenderAtSqrDistance(double distance)
{
	double size = bb.getSize();
	size *= 64 * viewScale;
	return distance < (size * size);
}

jstring Entity::getTexture()
{
	return u"";
}

bool Entity::isCreativeModeAllowed()
{
	return false;
}

bool Entity::save(CompoundTag &tag)
{
	jstring id = getEncodeId();
	if (removed || id.empty())
		return false;

	tag.putString(u"id", id);
	saveWithoutId(tag);
	return true;
}

void Entity::saveWithoutId(CompoundTag &tag)
{
	std::shared_ptr<ListTag> listTag = Util::make_shared<ListTag>();
	listTag->add(Util::make_shared<DoubleTag>(x));
	listTag->add(Util::make_shared<DoubleTag>(y));
	listTag->add(Util::make_shared<DoubleTag>(z));
	tag.put(u"Pos", listTag);

	listTag = Util::make_shared<ListTag>();
	listTag->add(Util::make_shared<DoubleTag>(xd));
	listTag->add(Util::make_shared<DoubleTag>(yd));
	listTag->add(Util::make_shared<DoubleTag>(zd));
	tag.put(u"Motion", listTag);

	listTag = Util::make_shared<ListTag>();
	listTag->add(Util::make_shared<FloatTag>(yRot));
	listTag->add(Util::make_shared<FloatTag>(xRot));
	tag.put(u"Rotation", listTag);

	tag.putFloat(u"FallDistance", fallDistance);
	tag.putShort(u"Fire", onFire);
	tag.putShort(u"Air", airSupply);
	tag.putBoolean(u"OnGround", onGround);

	addAdditionalSaveData(tag);
}

void Entity::load(CompoundTag &tag)
{
	std::shared_ptr<ListTag> posTag = tag.getList(u"Pos");
	std::shared_ptr<ListTag> motionTag = tag.getList(u"Motion");
	std::shared_ptr<ListTag> rotationTag = tag.getList(u"Rotation");

	setPos(0.0, 0.0, 0.0);
	xd = (reinterpret_cast<DoubleTag*>(motionTag->get(0).get()))->data;
	yd = (reinterpret_cast<DoubleTag*>(motionTag->get(1).get()))->data;
	zd = (reinterpret_cast<DoubleTag*>(motionTag->get(2).get()))->data;
	xo = xOld = x = (reinterpret_cast<DoubleTag*>(posTag->get(0).get()))->data;
	yo = yOld = y = (reinterpret_cast<DoubleTag*>(posTag->get(1).get()))->data;
	zo = zOld = z = (reinterpret_cast<DoubleTag*>(posTag->get(2).get()))->data;
	yRotO = yRot = (reinterpret_cast<FloatTag*>(rotationTag->get(0).get()))->data;
	xRotO = xRot = (reinterpret_cast<FloatTag*>(rotationTag->get(1).get()))->data;

	fallDistance = tag.getFloat(u"FallDistance");
	onFire = tag.getShort(u"Fire");
	airSupply = tag.getShort(u"Air");
	onGround = tag.getBoolean(u"OnGround");

	setPos(x, y, z);

	readAdditionalSaveData(tag);
}

void Entity::readAdditionalSaveData(CompoundTag &tag)
{

}

void Entity::addAdditionalSaveData(CompoundTag &tag)
{

}

float Entity::getShadowHeightOffs()
{
	return bbHeight / 2.0f;
}

void Entity::spawnAtLocation(const ItemInstance &stack, float offset)
{
	if (stack.isEmpty())
		return;
	auto entity = std::make_shared<EntityItem>(level, x, y + offset, z, stack);
	entity->throwTime = 10;
	level.addEntity(entity);
}

bool Entity::isAlive()
{
	return !removed;
}

bool Entity::isInWall()
{
	return false;
}

bool Entity::interact(Player &player)
{
	return false;
}

AABB *Entity::getCollideAgainstBox(Entity &entity)
{
	return nullptr;
}

void Entity::rideTick()
{
	if (riding == nullptr || riding->removed)
	{
		ride(nullptr);
		return;
	}

	// Zero rider motion (Java: updateRidden zeros motion before full tick)
	xd = 0.0;
	yd = 0.0;
	zd = 0.0;

	// Run the rider's full tick (Java: this.onUpdate())
	tick();

	if (riding != nullptr)
	{
		riding->positionRider();

		// Sync rider rotation to vehicle rotation changes
		xRideRotA += (riding->yRot - riding->yRotO);
		yRideRotA += (riding->xRot - riding->xRotO);

		// Wrap to [-180, 180)
		while (xRideRotA >= 180.0) xRideRotA -= 360.0;
		while (xRideRotA < -180.0) xRideRotA += 360.0;
		while (yRideRotA >= 180.0) yRideRotA -= 360.0;
		while (yRideRotA < -180.0) yRideRotA += 360.0;

		// Clamp half-delta to ±10 (Java: float var5 = 10.0F)
		double yawDelta = xRideRotA * 0.5;
		double pitchDelta = yRideRotA * 0.5;
		constexpr double maxDelta = 10.0;
		if (yawDelta > maxDelta) yawDelta = maxDelta;
		if (yawDelta < -maxDelta) yawDelta = -maxDelta;
		if (pitchDelta > maxDelta) pitchDelta = maxDelta;
		if (pitchDelta < -maxDelta) pitchDelta = -maxDelta;

		xRideRotA -= yawDelta;
		yRideRotA -= pitchDelta;

		yRot += static_cast<float>(yawDelta);
		xRot += static_cast<float>(pitchDelta);
	}
}

void Entity::positionRider()
{
	if (rider != nullptr)
		rider->setPos(x, y + getRideHeight() + rider->getRidingHeight(), z);
}

double Entity::getRidingHeight()
{
	return heightOffset;
}

double Entity::getRideHeight()
{
	return bbHeight * 0.75;
}

void Entity::ride(std::shared_ptr<Entity> entity)
{
	auto findSelf = [&]() -> std::shared_ptr<Entity> {
		for (const auto &candidate : level.getAllEntities())
		{
			if (candidate.get() == this)
				return candidate;
		}
		for (const auto &player : level.players)
		{
			if (player.get() == this)
				return std::static_pointer_cast<Entity>(player);
		}
		return nullptr;
	};
	auto syncPositionHistory = [&]() {
		xo = xOld = x;
		yo = yOld = y;
		zo = zOld = z;
	};

	xRideRotA = 0.0;
	yRideRotA = 0.0;

	if (entity == nullptr)
	{
		if (riding != nullptr)
		{
			setPos(riding->x, riding->bb.y0 + riding->bbHeight + heightOffset, riding->z);
			syncPositionHistory();
			riding->rider = nullptr;
		}
		riding = nullptr;
		return;
	}

	if (riding == entity)
	{
		entity->rider = nullptr;
		riding = nullptr;
		setPos(entity->x, entity->bb.y0 + entity->bbHeight + heightOffset, entity->z);
		syncPositionHistory();
		return;
	}

	if (riding != nullptr)
		riding->rider = nullptr;
	if (entity->rider != nullptr)
		entity->rider->riding = nullptr;

	auto self = findSelf();
	if (self == nullptr)
		return;

	riding = entity;
	entity->rider = self;
}

void Entity::lerpTo(double x, double y, double z, float yRot, float xRot, int_t steps)
{

}

float Entity::getPickRadius()
{
	return 0.0f;
}

Vec3 *Entity::getLookAngle()
{
	return nullptr;
}

void Entity::handleInsidePortal()
{

}

void Entity::lerpMotion(double x, double y, double z)
{

}

void Entity::handleEntityEvent(byte_t event)
{

}

void Entity::animateHurt()
{

}

void Entity::prepareCustomTextures()
{

}

// TODO
// getEquipmentSlots

void Entity::setEquippedSlot(int_t slot, int_t itemId, int_t auxValue)
{

}

bool Entity::isOnFire()
{
	return onFire > 0 || getSharedFlag(FLAG_ONFIRE);
}

bool Entity::isRiding()
{
	return riding != nullptr || getSharedFlag(FLAG_RIDING);
}

bool Entity::isSneaking()
{
	return false;
}

void Entity::setSneaking(bool sneaking)
{

}

bool Entity::getSharedFlag(int_t flag)
{
	return false;
}

void Entity::setSharedFlag(int_t flag, bool value)
{

}
