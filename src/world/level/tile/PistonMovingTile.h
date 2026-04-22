#pragma once

#include <memory>

#include "world/level/tile/Tile.h"

class TileEntity;

class PistonMovingTile : public Tile
{
public:
	PistonMovingTile(int_t id);

	Shape getRenderShape() override;
	bool isSolidRender() override;
	bool mayPlace(Level &level, int_t x, int_t y, int_t z) override;
	void setPlacedOnFace(Level &level, int_t x, int_t y, int_t z, Facing face) override;
	int_t getResourceCount(Random &random) override;
	void neighborChanged(Level &level, int_t x, int_t y, int_t z, int_t tile) override;
	void onRemove(Level &level, int_t x, int_t y, int_t z) override;
	bool use(Level &level, int_t x, int_t y, int_t z, Player &player) override;
	AABB *getAABB(Level &level, int_t x, int_t y, int_t z) override;
	void updateShape(LevelSource &level, int_t x, int_t y, int_t z) override;

	static std::shared_ptr<TileEntity> createTileEntity(int_t blockId, int_t blockData, int_t facing, bool extending, bool renderHead);

private:
	class PistonTileEntity *getPistonTileEntity(LevelSource &level, int_t x, int_t y, int_t z);
	AABB *getPushedAABB(Level &level, int_t x, int_t y, int_t z, int_t blockId, float progress, int_t dir);
};