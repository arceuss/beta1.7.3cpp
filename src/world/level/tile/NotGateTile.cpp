#include "world/level/tile/NotGateTile.h"

#include "world/level/Level.h"
#include "world/level/tile/Tile.h"
#include "java/Random.h"

struct RedstoneUpdateInfo
{
	int_t x;
	int_t y;
	int_t z;
	long_t updateTime;
};

std::vector<RedstoneUpdateInfo> NotGateTile::torchUpdates;

// b173: BlockRedstoneTorch(76, 86, true) / BlockRedstoneTorchIdle(75, 86, false)
NotGateTile::NotGateTile(int_t id, int_t tex, bool torchActive)
	: TorchTile(id, tex), torchActive(torchActive)
{
	setTicking(true);
	updateCachedProperties();
}

bool NotGateTile::isDirectSignalTo(Level &level, int_t x, int_t y, int_t z, int_t dir)
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

bool NotGateTile::isIndirectSignalTo(Level &level, int_t x, int_t y, int_t z, int_t dir)
{
	// b173: isIndirectlyPoweringTo — only provides indirect power upward (dir 0 = down)
	return dir == 0 ? isDirectSignalTo(level, x, y, z, dir) : false;
}

void NotGateTile::onPlace(Level &level, int_t x, int_t y, int_t z)
{
	// b173: onBlockAdded — if metadata is 0, call super; if active, notify all 6 neighbors
	if (level.getData(x, y, z) == 0)
		TorchTile::onPlace(level, x, y, z);

	if (torchActive)
	{
		level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
		level.notifyBlocksOfNeighborChange(x, y + 1, z, id);
		level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
		level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
	}
}

void NotGateTile::onRemove(Level &level, int_t x, int_t y, int_t z)
{
	if (torchActive)
	{
		level.notifyBlocksOfNeighborChange(x - 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x + 1, y, z, id);
		level.notifyBlocksOfNeighborChange(x, y - 1, z, id);
		level.notifyBlocksOfNeighborChange(x, y + 1, z, id);
		level.notifyBlocksOfNeighborChange(x, y, z - 1, id);
		level.notifyBlocksOfNeighborChange(x, y, z + 1, id);
	}
}

void NotGateTile::tick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	(void)random;

	bool shouldTurnOff = isAttachedBlockPowered(level, x, y, z);

	// Prune old burnout entries (> 100 ticks ago)
	long_t now = level.time;
	for (auto it = torchUpdates.begin(); it != torchUpdates.end(); )
	{
		if (now - it->updateTime > 100)
			it = torchUpdates.erase(it);
		else
			++it;
	}

	if (torchActive)
	{
		if (shouldTurnOff)
		{
			// Switch to idle block, preserving metadata (orientation)
			int_t data = level.getData(x, y, z);
			if (checkForBurnout(level, x, y, z, true))
			{
				// Burnout: fizz sound + smoke particles
				level.playSoundEffect(
					static_cast<double>(x) + 0.5,
					static_cast<double>(y) + 0.5,
					static_cast<double>(z) + 0.5,
					u"random.fizz",
					0.5f,
					2.4f + (level.random.nextFloat() - level.random.nextFloat()) * 0.8f
				);
				for (int_t i = 0; i < 5; i++)
				{
					double px = static_cast<double>(x) + level.random.nextDouble() * 0.6 + 0.2;
					double py = static_cast<double>(y) + level.random.nextDouble() * 0.6 + 0.2;
					double pz = static_cast<double>(z) + level.random.nextDouble() * 0.6 + 0.2;
					level.addParticle(u"smoke", px, py, pz, 0.0, 0.0, 0.0);
				}
			}
			level.setTileAndData(x, y, z, Tile::tiles[75]->id, data);
		}
	}
	else
	{
		if (!shouldTurnOff && !checkForBurnout(level, x, y, z, false))
		{
			// Switch to active block, preserving metadata (orientation)
			int_t data = level.getData(x, y, z);
			level.setTileAndData(x, y, z, Tile::tiles[76]->id, data);
		}
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
	if (data == 5) return level.isBlockIndirectlyProvidingPowerTo(x, y - 1, z, 0); // standing on ground → check below
	if (data == 3) return level.isBlockIndirectlyProvidingPowerTo(x, y, z - 1, 2);   // on south face → check north (z-1)
	if (data == 4) return level.isBlockIndirectlyProvidingPowerTo(x, y, z + 1, 3);   // on north face → check south (z+1)
	if (data == 1) return level.isBlockIndirectlyProvidingPowerTo(x - 1, y, z, 4);   // on east face → check west (x-1)
	if (data == 2) return level.isBlockIndirectlyProvidingPowerTo(x + 1, y, z, 5);   // on west face → check east (x+1)
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
	// Both active and idle torches drop the active torch item (block 76)
	return Tile::tiles[76]->id;
}

void NotGateTile::animateTick(Level &level, int_t x, int_t y, int_t z, Random &random)
{
	if (!torchActive)
		return;

	// Spawn reddust particles matching b173 randomDisplayTick
	int_t data = level.getData(x, y, z);
	double px = static_cast<double>(x) + 0.5;
	double py = static_cast<double>(y) + 0.7;
	double pz = static_cast<double>(z) + 0.5;
	double off = 0.22;
	double side = 0.27;

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