#pragma once

#include "client/particle/Particle.h"

// Beta 1.2: PortalParticle.java - portal particle
class PortalParticle : public Particle
{
private:
	float oSize;  // Beta: oSize - original size (PortalParticle.java:7)
	double xStart;  // Beta: xStart - starting X position (PortalParticle.java:8)
	double yStart;  // Beta: yStart - starting Y position (PortalParticle.java:9)
	double zStart;  // Beta: zStart - starting Z position (PortalParticle.java:10)

public:
	PortalParticle(Level &level, double x, double y, double z, double xd, double yd, double zd);

	virtual void tick() override;
	virtual void render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2) override;
	float getBrightness(float a) override;  // Override Entity::getBrightness
};
