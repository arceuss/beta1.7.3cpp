#include "world/entity/item/EntityBoat.h"

#include <cmath>

#include "world/entity/item/EntityItem.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include "world/item/Items.h"
#include "world/level/Level.h"
#include "world/level/material/LiquidMaterial.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/SnowTile.h"
#include "world/level/tile/WoodTile.h"
#include "world/phys/AABB.h"
#include "util/Mth.h"

namespace
{
	std::shared_ptr<Entity> findSharedEntity(Level &level, Entity *self)
	{
		for (const auto &entity : level.getAllEntities())
		{
			if (entity.get() == self)
				return entity;
		}
		return nullptr;
	}
}
static bool boatSliceInWater(Level &level, AABB &box)
{
	int_t x0 = Mth::floor(box.x0);
	int_t x1 = Mth::floor(box.x1 + 1.0);
	int_t y0 = Mth::floor(box.y0);
	int_t y1 = Mth::floor(box.y1 + 1.0);
	int_t z0 = Mth::floor(box.z0);
	int_t z1 = Mth::floor(box.z1 + 1.0);
	for (int_t x = x0; x < x1; ++x)
		for (int_t y = y0; y < y1; ++y)
			for (int_t z = z0; z < z1; ++z)
			{
				int_t tileId = level.getTile(x, y, z);
				if (tileId <= 0 || tileId >= 256)
					continue;
				Tile *tile = Tile::tiles[tileId];
				if (tile == nullptr || &tile->material != &Material::water)
					continue;
				int_t data = level.getData(x, y, z);
				double surface = static_cast<double>(y + 1);
				if (data < 8)
					surface = static_cast<double>(y + 1) - static_cast<double>(data) / 8.0;
				if (surface >= box.y0)
					return true;
			}
	return false;
}

EntityBoat::EntityBoat(Level &level) : Entity(level)
{
	blocksBuilding = true;
	setSize(1.5f, 0.6f);
	heightOffset = bbHeight / 2.0f;
	makeStepSound = false;
}

EntityBoat::EntityBoat(Level &level, double x, double y, double z) : EntityBoat(level)
{
	setPos(x, y + heightOffset, z);
	xOld = xo = x;
	yOld = yo = y;
	zOld = zo = z;
}

AABB *EntityBoat::getCollideAgainstBox(Entity &entity)
{
	// Boat.getCollideAgainstBox returns the other entity's live bounding box. The
	// native collision list borrows raw pointers, so hand back a pooled copy.
	return entity.bb.copy();
}

AABB *EntityBoat::getCollideBox()
{
	return &bb;
}

double EntityBoat::getRideHeight()
{
	return static_cast<double>(bbHeight) * 0.0 - static_cast<double>(0.3f);
}

bool EntityBoat::hurt(Entity *source, int_t dmg)
{
	(void)source;
	if (level.isOnline || removed)
		return true;

	boatRockDirection = -boatRockDirection;
	boatTimeSinceHit = 10;
	boatCurrentDamage += dmg * 10;
	markHurt();

	if (boatCurrentDamage > 40)
	{
		if (rider != nullptr)
			rider->ride(nullptr);

		for (int_t i = 0; i < 3; i++)
			spawnAtLocation(ItemInstance(Tile::wood.id, 1, 0), 0.0f);
		for (int_t i = 0; i < 2; i++)
			spawnAtLocation(ItemInstance(Items::stick->getShiftedIndex(), 1, 0), 0.0f);

		remove();
	}

	return true;
}

void EntityBoat::animateHurt()
{
	boatRockDirection = -boatRockDirection;
	boatTimeSinceHit = 10;
	boatCurrentDamage = boatCurrentDamage + boatCurrentDamage * 10;
}

bool EntityBoat::interact(Player &player)
{
	if (rider != nullptr && rider->isPlayer() && rider.get() != &player)
		return true;

	if (!level.isOnline)
	{
		auto self = findSharedEntity(level, this);
		if (self != nullptr)
			player.ride(self);
	}

	return true;
}

void EntityBoat::tick()
{
	Entity::tick();

	if (boatTimeSinceHit > 0)
		boatTimeSinceHit--;
	if (boatCurrentDamage > 0)
		boatCurrentDamage--;

	xo = x;
	yo = y;
	zo = z;

	if (level.isOnline)
	{
		if (lerpSteps > 0)
		{
			double nx = x + (lerpX - x) / lerpSteps;
			double ny = y + (lerpY - y) / lerpSteps;
			double nz = z + (lerpZ - z) / lerpSteps;
			double dyaw = lerpYaw - yRot;
			while (dyaw < -180.0)
				dyaw += 360.0;
			while (dyaw >= 180.0)
				dyaw -= 360.0;
			yRot = static_cast<float>(yRot + dyaw / lerpSteps);
			xRot = static_cast<float>(xRot + (lerpPitch - xRot) / lerpSteps);
			lerpSteps--;
			setPos(nx, ny, nz);
			setRot(yRot, xRot);
		}
		else
		{
			setPos(x + xd, y + yd, z + zd);
			if (onGround)
			{
				xd *= 0.5;
				yd *= 0.5;
				zd *= 0.5;
			}
			xd *= static_cast<double>(0.99f);
			yd *= static_cast<double>(0.95f);
			zd *= static_cast<double>(0.99f);
		}
		return;
	}

	// Buoyancy: sample 5 vertical slices for water coverage. Each slice uses the
	// reference height-aware water test, so falling water with a low surface
	// does not count the way a plain material check would.
	byte_t slices = 5;
	double waterCoverage = 0.0;
	for (int_t i = 0; i < slices; i++)
	{
		double y0 = bb.y0 + (bb.y1 - bb.y0) * i / slices - 0.125;
		double y1 = bb.y0 + (bb.y1 - bb.y0) * (i + 1) / slices - 0.125;
		AABB *sample = AABB::newTemp(bb.x0, y0, bb.z0, bb.x1, y1, bb.z1);
		if (boatSliceInWater(level, *sample))
			waterCoverage += 1.0 / slices;
	}

	if (waterCoverage < 1.0)
	{
		double buoyancy = waterCoverage * 2.0 - 1.0;
		yd += static_cast<double>(0.04f) * buoyancy;
	}
	else
	{
		if (yd < 0.0)
			yd /= 2.0;
		yd += static_cast<double>(0.007f);
	}

	if (rider != nullptr)
	{
		xd += rider->xd * 0.2;
		zd += rider->zd * 0.2;
	}

	double maxSpeed = 0.4;
	if (xd < -maxSpeed) xd = -maxSpeed;
	if (xd > maxSpeed) xd = maxSpeed;
	if (zd < -maxSpeed) zd = -maxSpeed;
	if (zd > maxSpeed) zd = maxSpeed;

	if (onGround)
	{
		xd *= 0.5;
		yd *= 0.5;
		zd *= 0.5;
	}

	move(xd, yd, zd);

	double speed = std::sqrt(xd * xd + zd * zd);
	if (speed > 0.15)
	{
		// Boat.tick uses Math.PI/Math.cos/Math.sin, not the float Mth trig tables.
		double cy = std::cos(static_cast<double>(yRot) * 3.141592653589793 / 180.0);
		double sy = std::sin(static_cast<double>(yRot) * 3.141592653589793 / 180.0);

		// Java compares the int loop counter against the double bound instead of
		// truncating it, so a fractional bound emits one more particle.
		for (int_t i = 0; static_cast<double>(i) < 1.0 + speed * 60.0; i++)
		{
			double r1 = random.nextFloat() * 2.0f - 1.0f;
			double r2 = (random.nextInt(2) * 2 - 1) * 0.7;
			if (random.nextBoolean())
			{
				double px = x - cy * r1 * 0.8 + sy * r2;
				double pz = z - sy * r1 * 0.8 - cy * r2;
				level.addParticle(u"splash", px, y - 0.125, pz, xd, yd, zd);
			}
			else
			{
				double px = x + cy + sy * r1 * 0.7;
				double pz = z + sy - cy * r1 * 0.7;
				level.addParticle(u"splash", px, y - 0.125, pz, xd, yd, zd);
			}
		}
	}

	if (!horizontalCollision || speed <= 0.15)
	{
		xd *= static_cast<double>(0.99f);
		yd *= static_cast<double>(0.95f);
		zd *= static_cast<double>(0.99f);
	}
	else if (!level.isOnline)
	{
		remove();
		for (int_t i = 0; i < 3; i++)
			spawnAtLocation(ItemInstance(Tile::wood.id, 1, 0), 0.0f);
		for (int_t i = 0; i < 2; i++)
			spawnAtLocation(ItemInstance(Items::stick->getShiftedIndex(), 1, 0), 0.0f);
	}

	xRot = 0.0f;
	double targetYaw = yRot;
	double dx = xo - x;
	double dz = zo - z;
	if (dx * dx + dz * dz > 0.001)
		targetYaw = static_cast<float>(std::atan2(dz, dx) * 180.0 / 3.141592653589793);

	double yawDelta = targetYaw - yRot;
	while (yawDelta >= 180.0)
		yawDelta -= 360.0;
	while (yawDelta < -180.0)
		yawDelta += 360.0;
	if (yawDelta > 20.0)
		yawDelta = 20.0;
	if (yawDelta < -20.0)
		yawDelta = -20.0;

	yRot = static_cast<float>(yRot + yawDelta);
	setRot(yRot, xRot);

	auto nearby = level.getEntities(this, *bb.grow(static_cast<double>(0.2f), 0.0, static_cast<double>(0.2f)));
	for (const auto &other : nearby)
	{
		if (other.get() != rider.get() && other->isPushable())
		{
			EntityBoat *otherBoat = dynamic_cast<EntityBoat *>(other.get());
			if (otherBoat != nullptr)
				other->push(*this);
		}
	}

	// Break snow under the boat
	for (int_t i = 0; i < 4; i++)
	{
		int_t sx = Mth::floor(x + (i % 2 - 0.5) * 0.8);
		int_t sy = Mth::floor(y);
		int_t sz = Mth::floor(z + (i / 2 - 0.5) * 0.8);
		if (level.getTile(sx, sy, sz) == Tile::snow.id)
			level.setTile(sx, sy, sz, 0);
	}

	if (rider != nullptr && rider->removed)
		rider = nullptr;
}

void EntityBoat::positionRider()
{
	if (rider != nullptr)
	{
		double ox = std::cos(static_cast<double>(yRot) * 3.141592653589793 / 180.0) * 0.4;
		double oz = std::sin(static_cast<double>(yRot) * 3.141592653589793 / 180.0) * 0.4;
		rider->setPos(x + ox, y + getRideHeight() + rider->getRidingHeight(), z + oz);
	}
}

void EntityBoat::lerpTo(double x, double y, double z, float yRot, float xRot, int_t steps)
{
	lerpX = x;
	lerpY = y;
	lerpZ = z;
	lerpYaw = yRot;
	lerpPitch = xRot;
	lerpSteps = steps + 4;
	xd = lerpXd;
	yd = lerpYd;
	zd = lerpZd;
}

void EntityBoat::lerpMotion(double x, double y, double z)
{
	lerpXd = this->xd = x;
	lerpYd = this->yd = y;
	lerpZd = this->zd = z;
}

float EntityBoat::getShadowHeightOffs()
{
	return 0.0f;
}

void EntityBoat::addAdditionalSaveData(CompoundTag &tag)
{
	(void)tag;
}

void EntityBoat::readAdditionalSaveData(CompoundTag &tag)
{
	(void)tag;
}
