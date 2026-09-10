#include "world/entity/item/EntityMinecart.h"

#include <cmath>

#include "nbt/CompoundTag.h"
#include "util/Mth.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/Mob.h"
#include "client/player/LocalPlayer.h"
#include "world/entity/player/Player.h"
#include "world/item/ItemInstance.h"
#include "world/item/Items.h"
#include "world/item/Item.h"
#include "world/level/tile/FurnaceTile.h"
#include "world/level/Level.h"
#include "world/level/tile/RailTile.h"
#include "world/level/tile/ChestTile.h"
#include "nbt/ListTag.h"
#include "world/level/tile/Tile.h"
#include "world/phys/Vec3.h"

constexpr int_t EntityMinecart::TYPE_RIDEABLE;
constexpr int_t EntityMinecart::TYPE_CHEST;
constexpr int_t EntityMinecart::TYPE_FURNACE;

namespace
{
	constexpr int_t TRACK_OFFSETS[10][2][3] = {
		{{0, 0, -1}, {0, 0, 1}},
		{{-1, 0, 0}, {1, 0, 0}},
		{{-1, -1, 0}, {1, 0, 0}},
		{{-1, 0, 0}, {1, -1, 0}},
		{{0, 0, -1}, {0, -1, 1}},
		{{0, -1, -1}, {0, 0, 1}},
		{{0, 0, 1}, {1, 0, 0}},
		{{0, 0, 1}, {-1, 0, 0}},
		{{0, 0, -1}, {-1, 0, 0}},
		{{0, 0, -1}, {1, 0, 0}},
	};

	std::shared_ptr<Entity> findSharedEntity(Level &level, Entity *self)
	{
		for (const auto &entity : level.getAllEntities())
		{
			if (entity.get() == self)
				return entity;
		}
		return nullptr;
	}

	void spawnDrop(Level &level, double x, double y, double z, const ItemInstance &stack)
	{
		auto entity = std::make_shared<EntityItem>(level, x, y, z, stack);
		entity->throwTime = 10;
		level.addEntity(entity);
	}

	// Minecart.setEntityDead scatters cargo with the cart's own Random: one offset
	// triple per occupied slot, 10-30 items per split and Gaussian float velocities.
	// It leaves the emptied ItemStack objects in place, so a later pass over the same
	// array still sees them as non-null and redraws the three offsets.
	void scatterCargo(Level &level, Random &random, double x, double y, double z,
		std::array<ItemInstance, 27> &items)
	{
		for (ItemInstance &stack : items)
		{
			if (stack.itemID == 0)
				continue;
			float xo = random.nextFloat() * 0.8f + 0.1f;
			float yo = random.nextFloat() * 0.8f + 0.1f;
			float zo = random.nextFloat() * 0.8f + 0.1f;
			while (stack.stackSize > 0)
			{
				int_t amount = random.nextInt(21) + 10;
				if (amount > stack.stackSize)
					amount = stack.stackSize;
				stack.stackSize -= amount;
				auto entity = std::make_shared<EntityItem>(level,
					x + static_cast<double>(xo), y + static_cast<double>(yo), z + static_cast<double>(zo),
					ItemInstance(stack.itemID, amount, stack.getAuxValue()));
				float spread = 0.05f;
				entity->xd = static_cast<float>(random.nextGaussian()) * spread;
				entity->yd = static_cast<float>(random.nextGaussian()) * spread + 0.2f;
				entity->zd = static_cast<float>(random.nextGaussian()) * spread;
				level.addEntity(entity);
			}
		}
	}

	double clampHorizontalSpeed(double speed, double maxSpeed)
	{
		if (speed < -maxSpeed)
			return -maxSpeed;
		if (speed > maxSpeed)
			return maxSpeed;
		return speed;
	}
}

EntityMinecart::EntityMinecart(Level &level) : Entity(level)
{
	blocksBuilding = true;
	setSize(0.98f, 0.7f);
	heightOffset = bbHeight / 2.0f;
	makeStepSound = false;
}

EntityMinecart::EntityMinecart(Level &level, double x, double y, double z, int_t minecartType)
	: EntityMinecart(level)
{
	setPos(x, y + heightOffset, z);
	xOld = xo = x;
	yOld = yo = y;
	zOld = zo = z;
	this->minecartType = minecartType;
}

AABB *EntityMinecart::getCollideAgainstBox(Entity &entity)
{
	return entity.bb.copy();
}

double EntityMinecart::getRideHeight()
{
	return static_cast<double>(bbHeight) * 0.0 - static_cast<double>(0.3f);
}

bool EntityMinecart::hurt(Entity *source, int_t dmg)
{
	(void)source;
	if (level.isOnline)
	{
		// EntityMinecart.attackEntityFrom: the client only reports the hit; the server applies damage.
		return true;
	}
	if (removed)
		return true;

	minecartRockDirection = -minecartRockDirection;
	minecartTimeSinceHit = 10;
	markHurt();
	minecartCurrentDamage += dmg * 10;
	if (minecartCurrentDamage <= 40)
		return true;

	if (rider != nullptr)
		rider->ride(nullptr);

	// Minecart.attackEntityFrom removes the cart first; the override scatters cargo.
	remove();
	spawnDrop(level, x, y, z, ItemInstance(Items::minecart->getShiftedIndex(), 1, 0));
	if (minecartType == TYPE_CHEST)
	{
		// Java repeats the scatter loop here. remove() already emptied every stack,
		// so this second pass only redraws three offsets per occupied slot.
		scatterCargo(level, random, x, y, z, cargoItems);
		spawnDrop(level, x, y, z, ItemInstance(Tile::chest.id, 1, 0));
	}
	else if (minecartType == TYPE_FURNACE)
	{
		spawnDrop(level, x, y, z, ItemInstance(Tile::furnace.id, 1, 0));
	}

	return true;
}

void EntityMinecart::animateHurt()
{
	minecartRockDirection = -minecartRockDirection;
	minecartTimeSinceHit = 10;
	minecartCurrentDamage += minecartCurrentDamage * 10;
}

void EntityMinecart::remove()
{
	// Minecart.setEntityDead scatters the whole container before the base removal,
	// so every removal path (damage, Level::removeEntity, ...) drops the cargo once.
	scatterCargo(level, random, x, y, z, cargoItems);
	Entity::remove();
}

bool EntityMinecart::interact(Player &player)
{
	if (minecartType == TYPE_RIDEABLE)
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

	if (minecartType == TYPE_CHEST)
	{
		if (level.isOnline)
		{
			// EntityMinecart.interact: the server opens the chest container.
			return true;
		}
		LocalPlayer *localPlayer = dynamic_cast<LocalPlayer *>(&player);
		if (localPlayer == nullptr)
			return false;
		auto self = std::dynamic_pointer_cast<EntityMinecart>(findSharedEntity(level, this));
		if (self == nullptr)
			return false;
		localPlayer->startChest(self);
		return true;
	}

	if (minecartType == TYPE_FURNACE)
	{
		ItemInstance *selected = player.getSelectedItem();
		if (selected != nullptr && !selected->isEmpty() && selected->itemID == Items::coal->getShiftedIndex())
		{
			selected->stackSize--;
			if (selected->isEmpty())
				player.removeSelectedItem();
			fuel += 1200;
		}
		pushX = x - player.x;
		pushZ = z - player.z;
		return true;
	}

	return false;
}

void EntityMinecart::tick()
{
	// Minecart.tick does not call super.tick(): the cart never runs the base
	// water/fire/void handling and never draws from its Random there.
	if (minecartTimeSinceHit > 0)
		minecartTimeSinceHit--;
	if (minecartCurrentDamage > 0)
		minecartCurrentDamage--;

	if (level.isOnline && lerpSteps > 0)
	{
		double nx = x + (lerpX - x) / lerpSteps;
		double ny = y + (lerpY - y) / lerpSteps;
		double nz = z + (lerpZ - z) / lerpSteps;
		double dyaw = static_cast<double>(lerpYaw) - static_cast<double>(yRot);
		while (dyaw < -180.0)
			dyaw += 360.0;
		while (dyaw >= 180.0)
			dyaw -= 360.0;
		yRot = static_cast<float>(static_cast<double>(yRot) + dyaw / lerpSteps);
		xRot = static_cast<float>(static_cast<double>(xRot) + (static_cast<double>(lerpPitch) - static_cast<double>(xRot)) / lerpSteps);
		lerpSteps--;
		setPos(nx, ny, nz);
		setRot(yRot, xRot);
		return;
	}

	xo = x;
	yo = y;
	zo = z;
	yd -= static_cast<double>(0.04f);

	int_t tileX = Mth::floor(x);
	int_t tileY = Mth::floor(y);
	int_t tileZ = Mth::floor(z);
	if (RailTile::isRail(level, tileX, tileY - 1, tileZ))
		tileY--;

	double maxSpeed = 0.4;
	bool emittingSmoke = false;
	double slopeAccel = 1.0 / 128.0;
	int_t tileId = level.getTile(tileX, tileY, tileZ);
	if (RailTile::isRail(tileId))
	{
		Vec3 *railPos = getPosOnTrack(x, y, z);
		int_t data = level.getData(tileX, tileY, tileZ);
		y = static_cast<double>(tileY) + static_cast<double>(heightOffset);
		bool boosting = false;
		bool braking = false;
		if (tileId == 27)
		{
			boosting = (data & 8) != 0;
			braking = !boosting;
		}
		if (tileId == 27 || tileId == 28)
			data &= 7;

		if (data >= 2 && data <= 5)
			y = static_cast<double>(tileY + 1) + static_cast<double>(heightOffset);
		if (data == 2)
			xd -= slopeAccel;
		if (data == 3)
			xd += slopeAccel;
		if (data == 4)
			zd += slopeAccel;
		if (data == 5)
			zd -= slopeAccel;

		const int_t (*track)[3] = TRACK_OFFSETS[data];
		double trackXDir = track[1][0] - track[0][0];
		double trackZDir = track[1][2] - track[0][2];
		double trackLen = std::sqrt(trackXDir * trackXDir + trackZDir * trackZDir);
		double along = xd * trackXDir + zd * trackZDir;
		if (along < 0.0)
		{
			trackXDir = -trackXDir;
			trackZDir = -trackZDir;
		}

		double speed = std::sqrt(xd * xd + zd * zd);
		xd = speed * trackXDir / trackLen;
		zd = speed * trackZDir / trackLen;
		if (braking)
		{
			double brakeSpeed = std::sqrt(xd * xd + zd * zd);
			if (brakeSpeed < 0.03)
			{
				xd = 0.0;
				yd = 0.0;
				zd = 0.0;
			}
			else
			{
				xd *= 0.5;
				yd = 0.0;
				zd *= 0.5;
			}
		}

		double offset = 0.0;
		double startX = tileX + 0.5 + track[0][0] * 0.5;
		double startZ = tileZ + 0.5 + track[0][2] * 0.5;
		double endX = tileX + 0.5 + track[1][0] * 0.5;
		double endZ = tileZ + 0.5 + track[1][2] * 0.5;
		trackXDir = endX - startX;
		trackZDir = endZ - startZ;
		if (trackXDir == 0.0)
		{
			x = tileX + 0.5;
			offset = z - tileZ;
		}
		else if (trackZDir == 0.0)
		{
			z = tileZ + 0.5;
			offset = x - tileX;
		}
		else
		{
			double relX = x - startX;
			double relZ = z - startZ;
			offset = (relX * trackXDir + relZ * trackZDir) * 2.0;
		}

		x = startX + trackXDir * offset;
		z = startZ + trackZDir * offset;
		setPos(x, y, z);

		double moveX = xd;
		double moveZ = zd;
		if (rider != nullptr)
		{
			moveX *= 0.75;
			moveZ *= 0.75;
		}
		moveX = clampHorizontalSpeed(moveX, maxSpeed);
		moveZ = clampHorizontalSpeed(moveZ, maxSpeed);
		move(moveX, 0.0, moveZ);
		if (track[0][1] != 0 && Mth::floor(x) - tileX == track[0][0] && Mth::floor(z) - tileZ == track[0][2])
			setPos(x, y + track[0][1], z);
		else if (track[1][1] != 0 && Mth::floor(x) - tileX == track[1][0] && Mth::floor(z) - tileZ == track[1][2])
			setPos(x, y + track[1][1], z);

		if (rider != nullptr)
		{
			xd *= static_cast<double>(0.997f);
			yd = 0.0;
			zd *= static_cast<double>(0.997f);
		}
		else
		{
			if (minecartType == TYPE_FURNACE)
			{
				// Java uses MathHelper.sqrt_double here, which narrows through float.
				double pushLen = static_cast<double>(Mth::sqrt(pushX * pushX + pushZ * pushZ));
				if (pushLen > 0.01)
				{
					emittingSmoke = true;
					pushX /= pushLen;
					pushZ /= pushLen;
					double accel = 0.04;
					xd *= static_cast<double>(0.8f);
					yd = 0.0;
					zd *= static_cast<double>(0.8f);
					xd += pushX * accel;
					zd += pushZ * accel;
				}
				else
				{
					xd *= static_cast<double>(0.9f);
					yd = 0.0;
					zd *= static_cast<double>(0.9f);
				}
			}
			xd *= static_cast<double>(0.96f);
			yd = 0.0;
			zd *= static_cast<double>(0.96f);
		}

		Vec3 *newRailPos = getPosOnTrack(x, y, z);
		if (newRailPos != nullptr && railPos != nullptr)
		{
			double slopeDelta = (railPos->y - newRailPos->y) * 0.05;
			double curSpeed = std::sqrt(xd * xd + zd * zd);
			if (curSpeed > 0.0)
			{
				xd = xd / curSpeed * (curSpeed + slopeDelta);
				zd = zd / curSpeed * (curSpeed + slopeDelta);
			}
			setPos(x, newRailPos->y, z);
		}

		int_t newTileX = Mth::floor(x);
		int_t newTileZ = Mth::floor(z);
		if (newTileX != tileX || newTileZ != tileZ)
		{
			double curSpeed = std::sqrt(xd * xd + zd * zd);
			xd = curSpeed * (newTileX - tileX);
			zd = curSpeed * (newTileZ - tileZ);
		}

		if (minecartType == TYPE_FURNACE)
		{
			double pushLen = static_cast<double>(Mth::sqrt(pushX * pushX + pushZ * pushZ));
			if (pushLen > 0.01 && xd * xd + zd * zd > 0.001)
			{
				pushX /= pushLen;
				pushZ /= pushLen;
				if (pushX * xd + pushZ * zd < 0.0)
				{
					pushX = 0.0;
					pushZ = 0.0;
				}
				else
				{
					pushX = xd;
					pushZ = zd;
				}
			}
		}

		if (boosting)
		{
			double boostSpeed = std::sqrt(xd * xd + zd * zd);
			if (boostSpeed > 0.01)
			{
				xd += xd / boostSpeed * 0.06;
				zd += zd / boostSpeed * 0.06;
			}
			else if (data == 1)
			{
				if (level.isBlockNormalCube(tileX - 1, tileY, tileZ))
					xd = 0.02;
				else if (level.isBlockNormalCube(tileX + 1, tileY, tileZ))
					xd = -0.02;
			}
			else if (data == 0)
			{
				if (level.isBlockNormalCube(tileX, tileY, tileZ - 1))
					zd = 0.02;
				else if (level.isBlockNormalCube(tileX, tileY, tileZ + 1))
					zd = -0.02;
			}
		}
	}
	else
	{
		xd = clampHorizontalSpeed(xd, maxSpeed);
		zd = clampHorizontalSpeed(zd, maxSpeed);
		if (onGround)
		{
			xd *= 0.5;
			yd *= 0.5;
			zd *= 0.5;
		}
		move(xd, yd, zd);
		if (!onGround)
		{
			xd *= static_cast<double>(0.95f);
			yd *= static_cast<double>(0.95f);
			zd *= static_cast<double>(0.95f);
		}
	}

	xRot = 0.0f;
	double dx = xo - x;
	double dz = zo - z;
	if (dx * dx + dz * dz > 0.001)
	{
		// Minecart.tick uses Math.PI, not the float Mth::PI.
		yRot = static_cast<float>(std::atan2(dz, dx) * 180.0 / 3.141592653589793);
		if (flipped)
			yRot += 180.0f;
	}

	double yawDelta = yRot - yRotO;
	while (yawDelta >= 180.0)
		yawDelta -= 360.0;
	while (yawDelta < -180.0)
		yawDelta += 360.0;
	if (yawDelta < -170.0 || yawDelta >= 170.0)
	{
		yRot += 180.0f;
		flipped = !flipped;
	}
	setRot(yRot, xRot);

	const auto &nearby = level.getEntities(this, *bb.grow(static_cast<double>(0.2f), 0.0, static_cast<double>(0.2f)));
	for (const auto &other : nearby)
	{
		// Minecart.tick only hands the collision to other minecarts.
		if (other.get() != rider.get() && other->isPushable() &&
			dynamic_cast<EntityMinecart *>(other.get()) != nullptr)
			other->push(*this);
	}

	if (rider != nullptr && rider->removed)
		rider = nullptr;

	if (emittingSmoke && random.nextInt(4) == 0)
	{
		fuel--;
		if (fuel < 0)
			pushX = pushZ = 0.0;
		level.addParticle(u"largesmoke", x, y + 0.8, z, 0.0, 0.0, 0.0);
	}
}

Vec3 *EntityMinecart::getPosOffs(double px, double py, double pz, double offset) const
{
	int_t tileX = Mth::floor(px);
	int_t tileY = Mth::floor(py);
	int_t tileZ = Mth::floor(pz);
	if (RailTile::isRail(level, tileX, tileY - 1, tileZ))
		tileY--;

	int_t tileId = level.getTile(tileX, tileY, tileZ);
	if (!RailTile::isRail(tileId))
		return nullptr;

	int_t data = level.getData(tileX, tileY, tileZ);
	if (tileId == 27 || tileId == 28)
		data &= 7;
		double railY = tileY;
	if (data >= 2 && data <= 5)
		railY = tileY + 1;

	const int_t (*track)[3] = TRACK_OFFSETS[data];
	double trackXDir = track[1][0] - track[0][0];
	double trackZDir = track[1][2] - track[0][2];
	double trackLen = std::sqrt(trackXDir * trackXDir + trackZDir * trackZDir);
	trackXDir /= trackLen;
	trackZDir /= trackLen;
	px += trackXDir * offset;
	pz += trackZDir * offset;
	if (track[0][1] != 0 && Mth::floor(px) - tileX == track[0][0] && Mth::floor(pz) - tileZ == track[0][2])
		railY += track[0][1];
	else if (track[1][1] != 0 && Mth::floor(px) - tileX == track[1][0] && Mth::floor(pz) - tileZ == track[1][2])
		railY += track[1][1];
	return getPosOnTrack(px, railY, pz);
}

Vec3 *EntityMinecart::getPosOnTrack(double px, double py, double pz) const
{
	int_t tileX = Mth::floor(px);
	int_t tileY = Mth::floor(py);
	int_t tileZ = Mth::floor(pz);
	if (RailTile::isRail(level, tileX, tileY - 1, tileZ))
		tileY--;

	int_t tileId = level.getTile(tileX, tileY, tileZ);
	if (!RailTile::isRail(tileId))
		return nullptr;

	int_t data = level.getData(tileX, tileY, tileZ);
	double railY = tileY;
	if (tileId == 27 || tileId == 28)
		data &= 7;
	if (data >= 2 && data <= 5)
		railY = tileY + 1;

	const int_t (*track)[3] = TRACK_OFFSETS[data];
	double offset = 0.0;
	double startX = tileX + 0.5 + track[0][0] * 0.5;
	double startY = tileY + 0.5 + track[0][1] * 0.5;
	double startZ = tileZ + 0.5 + track[0][2] * 0.5;
	double endX = tileX + 0.5 + track[1][0] * 0.5;
	double endY = tileY + 0.5 + track[1][1] * 0.5;
	double endZ = tileZ + 0.5 + track[1][2] * 0.5;
	double dx = endX - startX;
	double dy = (endY - startY) * 2.0;
	double dz = endZ - startZ;
	if (dx == 0.0)
	{
		px = tileX + 0.5;
		offset = pz - tileZ;
	}
	else if (dz == 0.0)
	{
		pz = tileZ + 0.5;
		offset = px - tileX;
	}
	else
	{
		double relX = px - startX;
		double relZ = pz - startZ;
		offset = (relX * dx + relZ * dz) * 2.0;
	}

	px = startX + dx * offset;
	railY = startY + dy * offset;
	pz = startZ + dz * offset;
	if (dy < 0.0)
		railY += 1.0;
	if (dy > 0.0)
		railY += 0.5;
	return Vec3::newTemp(px, railY, pz);
}

ItemInstance &EntityMinecart::getItem(int_t slot)
{
	return cargoItems[slot];
}

const ItemInstance &EntityMinecart::getItem(int_t slot) const
{
	return cargoItems[slot];
}

void EntityMinecart::setItem(int_t slot, const ItemInstance &item)
{
	cargoItems[slot] = item;
	// Minecart.setItem clamps to the container limit (64), not the item's own
	// max stack size, so an oversized stack of a normally unstackable item keeps
	// whatever count it was given below 64.
	if (cargoItems[slot].itemID != 0 && cargoItems[slot].stackSize > getInventoryStackLimit())
		cargoItems[slot].stackSize = getInventoryStackLimit();
}

ItemInstance EntityMinecart::removeItem(int_t slot, int_t count)
{
	if (cargoItems[slot].isEmpty() || count <= 0)
		return ItemInstance();
	if (cargoItems[slot].stackSize <= count)
	{
		ItemInstance whole = cargoItems[slot];
		cargoItems[slot] = ItemInstance();
		return whole;
	}
	ItemInstance split = cargoItems[slot].remove(count);
	if (cargoItems[slot].stackSize == 0)
		cargoItems[slot] = ItemInstance();
	return split;
}

int_t EntityMinecart::getContainerSize() const
{
	return static_cast<int_t>(cargoItems.size());
}

bool EntityMinecart::canUse(Player &player) const
{
	return !removed && player.distanceToSqr(x, y, z) <= 64.0;
}

jstring EntityMinecart::getName() const
{
	return u"Minecart";
}

void EntityMinecart::addAdditionalSaveData(CompoundTag &tag)
{
	tag.putInt(u"Type", minecartType);
	if (minecartType == TYPE_CHEST)
	{
		auto items = std::make_shared<ListTag>();
		for (int_t i = 0; i < static_cast<int_t>(cargoItems.size()); ++i)
		{
			if (cargoItems[i].isEmpty())
				continue;
			auto entry = std::make_shared<CompoundTag>();
			entry->putByte(u"Slot", static_cast<byte_t>(i));
			cargoItems[i].save(*entry);
			items->add(entry);
		}
		tag.put(u"Items", items);
	}
	else if (minecartType == TYPE_FURNACE)
	{
		tag.putDouble(u"PushX", pushX);
		tag.putDouble(u"PushZ", pushZ);
		tag.putShort(u"Fuel", static_cast<short_t>(fuel));
	}
}

void EntityMinecart::readAdditionalSaveData(CompoundTag &tag)
{
	minecartType = tag.getInt(u"Type");
	if (minecartType == TYPE_CHEST)
	{
		cargoItems = {};
		auto items = tag.getList(u"Items");
		if (items != nullptr)
		{
			for (int_t i = 0; i < items->size(); ++i)
			{
				auto entry = std::dynamic_pointer_cast<CompoundTag>(items->get(i));
				if (entry == nullptr)
					continue;
				int_t slot = entry->getByte(u"Slot") & 255;
				if (slot >= 0 && slot < static_cast<int_t>(cargoItems.size()))
					cargoItems[slot].load(*entry);
			}
		}
	}
	else if (minecartType == TYPE_FURNACE)
	{
		pushX = tag.getDouble(u"PushX");
		pushZ = tag.getDouble(u"PushZ");
		fuel = tag.getShort(u"Fuel");
	}
}

void EntityMinecart::push(Entity &entity)
{
	if (level.isOnline)
		return;
	if (&entity == rider.get())
		return;

	// A moving, empty, rideable cart mounts any non-player mob it touches.
	if (dynamic_cast<Mob *>(&entity) != nullptr && !entity.isPlayer() &&
		minecartType == TYPE_RIDEABLE && xd * xd + zd * zd > 0.01 &&
		rider == nullptr && entity.riding == nullptr)
	{
		auto self = findSharedEntity(level, this);
		if (self != nullptr)
			entity.ride(self);
	}

	double dx = entity.x - x;
	double dz = entity.z - z;
	double distSqr = dx * dx + dz * dz;
	if (distSqr < static_cast<double>(1.0E-4f))
		return;

	// Minecart.push narrows the separation through MathHelper.sqrt_double.
	double dist = static_cast<double>(Mth::sqrt(distSqr));
	dx /= dist;
	dz /= dist;
	double scale = 1.0 / dist;
	if (scale > 1.0)
		scale = 1.0;
	dx *= scale;
	dz *= scale;
	dx *= static_cast<double>(0.1f);
	dz *= static_cast<double>(0.1f);
	dx *= static_cast<double>(1.0f - pushthrough);
	dz *= static_cast<double>(1.0f - pushthrough);
	dx *= 0.5;
	dz *= 0.5;

	EntityMinecart *otherCart = dynamic_cast<EntityMinecart *>(&entity);
	if (otherCart == nullptr)
	{
		push(-dx, 0.0, -dz);
		entity.push(dx / 4.0, 0.0, dz / 4.0);
		return;
	}

	// Beta mixes the other cart's z velocity with its previous *x position* here.
	// Both supplied references agree on the term, so it is reproduced verbatim.
	double relX = entity.x - x;
	double relZ = entity.z - z;
	double reject = relX * entity.zd + relZ * entity.xo;
	reject *= reject;
	if (reject > 5.0)
		return;

	double sumXd = entity.xd + xd;
	double sumZd = entity.zd + zd;
	if (otherCart->minecartType == TYPE_FURNACE && minecartType != TYPE_FURNACE)
	{
		xd *= static_cast<double>(0.2f);
		zd *= static_cast<double>(0.2f);
		push(entity.xd - dx, 0.0, entity.zd - dz);
		entity.xd *= static_cast<double>(0.7f);
		entity.zd *= static_cast<double>(0.7f);
	}
	else if (otherCart->minecartType != TYPE_FURNACE && minecartType == TYPE_FURNACE)
	{
		entity.xd *= static_cast<double>(0.2f);
		entity.zd *= static_cast<double>(0.2f);
		entity.push(xd + dx, 0.0, zd + dz);
		xd *= static_cast<double>(0.7f);
		zd *= static_cast<double>(0.7f);
	}
	else
	{
		xd *= static_cast<double>(0.2f);
		zd *= static_cast<double>(0.2f);
		sumXd /= 2.0;
		sumZd /= 2.0;
		push(sumXd - dx, 0.0, sumZd - dz);
		entity.xd *= static_cast<double>(0.2f);
		entity.zd *= static_cast<double>(0.2f);
		entity.push(sumXd + dx, 0.0, sumZd + dz);
	}
}

void EntityMinecart::lerpTo(double x, double y, double z, float yRot, float xRot, int_t steps)
{
	// Minecart.lerpTo stores the packet Y unchanged: heightOffset is already in it.
	lerpX = x;
	lerpY = y;
	lerpZ = z;
	lerpYaw = yRot;
	lerpPitch = xRot;
	lerpSteps = steps + 2;
	xd = lerpXd;
	yd = lerpYd;
	zd = lerpZd;
}

void EntityMinecart::lerpMotion(double x, double y, double z)
{
	lerpXd = xd = x;
	lerpYd = yd = y;
	lerpZd = zd = z;
}
