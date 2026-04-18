#pragma once

#include "client/particle/Particle.h"

// newb12: FlameParticle.java - flame particle for torches and fire
// Ported 1:1 from newb12/net/minecraft/client/particle/FlameParticle.java
class FlameParticle : public Particle
{
private:
	float oSize;  // newb12: private float oSize

public:
	FlameParticle(Level &level, double x, double y, double z, double xd, double yd, double zd);

	virtual void render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2) override;
	float getBrightness(float a);  // Override Entity::getBrightness (not virtual in base)
	virtual void tick() override;
};
