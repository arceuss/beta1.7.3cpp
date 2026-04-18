#pragma once

#include "client/particle/Particle.h"

// newb12: LavaParticle.java - lava particle for lava blocks
// Ported 1:1 from newb12/net/minecraft/client/particle/LavaParticle.java
class LavaParticle : public Particle
{
private:
	float oSize;  // newb12: private float oSize

public:
	LavaParticle(Level &level, double x, double y, double z);

	float getBrightness(float a);  // Override Entity::getBrightness (not virtual in base)
	virtual void render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2) override;
	virtual void tick() override;
};
