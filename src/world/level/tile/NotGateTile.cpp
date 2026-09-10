#include "world/level/tile/NotGateTile.h"

#include "world/level/Level.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/RedStoneDustTile.h"
#include "java/Random.h"

struct RedstoneUpdateInfo
{
	int_t x;
	int_t y;
	int_t z;
	long_t updateTime;
};

std::vector<RedstoneUpdateInfo> NotGateTile::torchUpdates;

// b173: BlockRedstoneTorch(76, 99, true) / BlockRedstoneTorch(75, 115, false)
NotGateTile::NotGateTile(int_t id, int_t tex, bool torchActive)
	: TorchTile(id, tex), torchActive(torchActive)
{
	setTicking(true);
	updateCachedProperties();
}

int_t NotGateTile::getTexture(Facing face, int_t data)
{
	// vanilla delegates the top face to redstone dust so the wire tip shows
	// through the torch head
	if (face == Facing::UP)
		return Tile::redstoneWire.getTexture(face, data);
	return TorchTile::getTexture(face, data);
}

bool NotGateTile::getSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	if (!torchActive)
		return false;

	// The torch does not power the block it is attached to.
	// Metadata encodes attachment direction; the torch skips signal toward that block.
	int_t data = level.getData(x, y, z);
	if (data == 5 && dir == 1) return false; // standing on ground → not powering up
	if (data == 3 && dir == 3) return false; // on south face → not powering south
	if (data == 4 && dir == 2) return false; // on north face → not powering north
	if (data == 1 && dir == 5) return false; // on east face → not powering east
	if (data == 2 && dir == 4) return false; // on west face → not powering west
	return true;
}

bool NotGateTile::getDirectSignal(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	// b173: isIndirectlyPoweringTo — only provides indirect power upward (dir 0 = down)
	return dir == 0 ? getSignal(level, x, y, z, dir) : false;
}

void NotGateTile::onPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: onBlockAdded — if metadata is 0, call super; if active, notify all 6 neighbors
	if (level.getData(x, y, z) == 0)
		TorchTile::onPlace(level, x, y, z);

	if (torchActive)
	{
		level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
		level.notifyBlocksOfNeighborChange(x, y + 1, z, id);
		level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
		level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
	}
}

void NotGateTile::onRemove(Level &level, int_t x, int_t y, int_t z)
{
	if (torchActive)
	{
		level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
		level.notifyBlocksOfNeighborChange(x, y + 1, z, id);
		level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
		level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
	}
}

void NotGateTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	bool shouldTurnOff = isAttachedBlockPowered(level, x, y, z);

	// vanilla prunes only the expired prefix of the toggle log: it stops at the
	// first entry inside the 100 tick window instead of sweeping the whole list,
	// which matters once the world time moves backwards.
	while (!torchUpdates.empty() && level.time - torchUpdates.front().updateTime > 100)
		torchUpdates.erase(torchUpdates.begin());

	if (torchActive)
	{
		if (shouldTurnOff)
		{
			// the off-state replacement happens first; the burnout bookkeeping,
			// sound and smoke follow it
			level.setTileAndData(x, y, z, Tile::torchRedstoneIdle.id, level.getData(x, y, z));
			if (checkForBurnout(level, x, y, z, true))
			{
				const float fa = level.random.nextFloat();
				const float fb = level.random.nextFloat();
				level.playSoundEffect(
					static_cast<double>(x) + 0.5,
					static_cast<double>(y) + 0.5,
					static_cast<double>(z) + 0.5,
					u"random.fizz",
					0.5f,
					2.6f + (fa - fb) * 0.8f
				);
				// the smoke offsets come from the tick random, not level.random
				for (int_t i = 0; i < 5; i++)
				{
					double px = static_cast<double>(x) + random.nextDouble() * 0.6 + 0.2;
					double py = static_cast<double>(y) + random.nextDouble() * 0.6 + 0.2;
					double pz = static_cast<double>(z) + random.nextDouble() * 0.6 + 0.2;
					level.addParticle(u"smoke", px, py, pz, 0.0, 0.0, 0.0);
				}
			}
		}
	}
	else if (!shouldTurnOff && !checkForBurnout(level, x, y, z, false))
	{
		level.setTileAndData(x, y, z, Tile::torchRedstoneActive.id, level.getData(x, y, z));
	}
}

void NotGateTile::neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile)
{
	TorchTile::neighborChanged(level, x, y, z, tile);
	level.scheduleBlockUpdate(x, y, z, id, getTickDelay());
}

bool NotGateTile::isAttachedBlockPowered(Level &level, int_t x, int_t y, int_t z)
{
	// b173: func_30002_h — check if the block the torch is attached to is indirectly powered
	int_t data = level.getData(x, y, z);
	if (data == 5) return level.getSignal(x, y - 1, z, 0); // standing on ground → check below
	if (data == 3) return level.getSignal(x, y, z - 1, 2);   // on south face → check north (z-1)
	if (data == 4) return level.getSignal(x, y, z + 1, 3);   // on north face → check south (z+1)
	if (data == 1) return level.getSignal(x - 1, y, z, 4);   // on east face → check west (x-1)
	if (data == 2) return level.getSignal(x + 1, y, z, 5);   // on west face → check east (x+1)
	return false;
}

bool NotGateTile::checkForBurnout(Level &level, int_t x, int_t y, int_t z, bool logUpdate)
{
	if (logUpdate)
	{
		torchUpdates.push_back({x, y, z, level.time});
	}

	int_t count = 0;
	for (const auto &entry : torchUpdates)
	{
		if (entry.x == x && entry.y == y && entry.z == z)
			count++;
	}

	return count >= 8;
}

int_t NotGateTile::getResource(int_t data, Random &random)
{
	(void)data;
	(void)random;
	// both states drop the active torch
	return Tile::torchRedstoneActive.id;
}

void NotGateTile::animateTick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	if (!torchActive)
		return;

	// vanilla jitters all three base coordinates before picking the
	// orientation offset: three nextFloat draws, in x/y/z order
	int_t data = level.getData(x, y, z);
	double px = static_cast<double>(static_cast<float>(x) + 0.5f) + static_cast<double>(random.nextFloat() - 0.5f) * 0.2;
	double py = static_cast<double>(static_cast<float>(y) + 0.7f) + static_cast<double>(random.nextFloat() - 0.5f) * 0.2;
	double pz = static_cast<double>(static_cast<float>(z) + 0.5f) + static_cast<double>(random.nextFloat() - 0.5f) * 0.2;
	double off = static_cast<double>(0.22f);
	double side = static_cast<double>(0.27f);

	if (data == 1)
	{
		level.addParticle(u"reddust", px - side, py + off, pz, 0.0, 0.0, 0.0);
	}
	else if (data == 2)
	{
		level.addParticle(u"reddust", px + side, py + off, pz, 0.0, 0.0, 0.0);
	}
	else if (data == 3)
	{
		level.addParticle(u"reddust", px, py + off, pz - side, 0.0, 0.0, 0.0);
	}
	else if (data == 4)
	{
		level.addParticle(u"reddust", px, py + off, pz + side, 0.0, 0.0, 0.0);
	}
	else
	{
		level.addParticle(u"reddust", px, py, pz, 0.0, 0.0, 0.0);
	}
}