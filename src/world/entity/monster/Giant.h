#pragma once

#include "world/entity/monster/Zombie.h"

class Giant : public Zombie
{
public:
	Giant(Level &level);
	jstring getEncodeId() const override { return u"Giant"; }
};
