#include "world/entity/monster/Giant.h"

Giant::Giant(Level &level) : Zombie(level)
{
	attackDamage = 50;
	health *= 10;
	setSize(bbWidth * 6.0f, bbHeight * 6.0f);
}
