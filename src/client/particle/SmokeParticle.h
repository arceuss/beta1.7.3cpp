#pragma once

#include "client/particle/Particle.h"

// Beta 1.2: SmokeParticle.java - smoke particle for fire and other effects
class SmokeParticle : public Particle
{
private:
	float oSize;  // Beta: oSize - original size (SmokeParticle.java:7)

public:
	SmokeParticle(Level &level, double x, double y, double z, double xa, double ya, double za);
	SmokeParticle(Level &level, double x, double y, double z, double xa, double ya, double za, float scale);
	
	virtual void tick() override;
	virtual void render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2) override;
};
