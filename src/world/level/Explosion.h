#pragma once

#include <unordered_set>
#include <vector>
#include "java/Type.h"
#include "world/level/TilePos.h"

class Level;
class Entity;
class AABB;

class Explosion
{
public:
	bool isFlaming = false;
	Level &level;
	Entity *exploder = nullptr;
	double explosionX = 0.0, explosionY = 0.0, explosionZ = 0.0;
	float explosionSize = 0.0f;
	std::unordered_set<TilePos> destroyedBlockPositions;

	Explosion(Level &level, Entity *exploder, double x, double y, double z, float size);
	void doExplosionA();
	void doExplosionB(bool particles);

private:
	float getBlockDensity(double x, double y, double z, const AABB &bb);
};
