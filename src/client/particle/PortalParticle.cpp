#include "client/particle/PortalParticle.h"

#include "world/level/Level.h"
#include "util/Mth.h"
#include "java/Random.h"
#include <cmath>
#include <cstdlib>

// Beta 1.2: PortalParticle.java - portal particle
PortalParticle::PortalParticle(Level &level, double x, double y, double z, double xd, double yd, double zd)
	: Particle(level, x, y, z, xd, yd, zd)
{
	// Beta: PortalParticle constructor (PortalParticle.java:12-28)
	this->xd = xd;  // Beta: this.xd = xd (PortalParticle.java:14)
	this->yd = yd;  // Beta: this.yd = yd (PortalParticle.java:15)
	this->zd = zd;  // Beta: this.zd = zd (PortalParticle.java:16)

	xStart = this->x = x;  // Beta: this.xStart = this.x = x (PortalParticle.java:17)
	yStart = this->y = y;  // Beta: this.yStart = this.y = y (PortalParticle.java:18)
	zStart = this->z = z;  // Beta: this.zStart = this.z = z (PortalParticle.java:19)

	// Beta: Random brightness and size (PortalParticle.java:20-21)
	float br = random.nextFloat() * 0.6f + 0.4f;  // Beta: float br = this.random.nextFloat() * 0.6F + 0.4F
	oSize = size = random.nextFloat() * 0.2f + 0.5f;  // Beta: this.oSize = this.size = this.random.nextFloat() * 0.2F + 0.5F

	// Beta: Purple-tinted color (PortalParticle.java:22-24)
	rCol = gCol = bCol = 1.0f * br;  // Beta: this.rCol = this.gCol = this.bCol = 1.0F * br
	gCol *= 0.3f;  // Beta: this.gCol *= 0.3F
	rCol *= 0.9f;  // Beta: this.rCol *= 0.9F

	lifetime = (int_t)(random.nextFloat() * 10.0) + 40;  // Beta: this.lifetime = (int)(Math.random() * 10.0) + 40 (PortalParticle.java:25)
	noPhysics = true;  // Beta: this.noPhysics = true (PortalParticle.java:26)
	tex = (int_t)(random.nextFloat() * 8.0);  // Beta: this.tex = (int)(Math.random() * 8.0) (PortalParticle.java:27)
}

void PortalParticle::render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2)
{
	// Beta: PortalParticle.render() - scales size based on age (PortalParticle.java:31-38)
	float s = (age + a) / lifetime;  // Beta: float s = (this.age + a) / this.lifetime (PortalParticle.java:32)
	s = 1.0f - s;  // Beta: s = 1.0F - s (PortalParticle.java:33)
	s *= s;  // Beta: s *= s (PortalParticle.java:34)
	s = 1.0f - s;  // Beta: s = 1.0F - s (PortalParticle.java:35)
	size = oSize * s;  // Beta: this.size = this.oSize * s (PortalParticle.java:36)
	Particle::render(t, a, xa, ya, za, xa2, za2);  // Beta: super.render(t, a, xa, ya, za, xa2, za2) (PortalParticle.java:37)
}

float PortalParticle::getBrightness(float a)
{
	// Beta: PortalParticle.getBrightness() - brightness increases over time (PortalParticle.java:41-47)
	float br = Entity::getBrightness(a);  // Beta: float br = super.getBrightness(a) (PortalParticle.java:42)
	float pos = (float)age / lifetime;  // Beta: float pos = (float)this.age / this.lifetime (PortalParticle.java:43)
	pos *= pos;  // Beta: pos *= pos (PortalParticle.java:44)
	pos *= pos;  // Beta: pos *= pos (PortalParticle.java:45)
	return br * (1.0f - pos) + pos;  // Beta: return br * (1.0F - pos) + pos (PortalParticle.java:46)
}

void PortalParticle::tick()
{
	// Beta: PortalParticle.tick() - moves in curved path (PortalParticle.java:50-63)
	xo = x;  // Beta: this.xo = this.x (PortalParticle.java:51)
	yo = y;  // Beta: this.yo = this.y (PortalParticle.java:52)
	zo = z;  // Beta: this.zo = this.z (PortalParticle.java:53)

	float pos = (float)age / lifetime;  // Beta: float pos = (float)this.age / this.lifetime (PortalParticle.java:54)
	float var3 = -pos + pos * pos * 2.0f;  // Beta: float var3 = -pos + pos * pos * 2.0F (PortalParticle.java:55)
	float var4 = 1.0f - var3;  // Beta: float var4 = 1.0F - var3 (PortalParticle.java:56)

	// Beta: Update position along curved path (PortalParticle.java:57-59)
	x = xStart + xd * var4;
	y = yStart + yd * var4 + (1.0f - pos);
	z = zStart + zd * var4;

	if (age++ >= lifetime)  // Beta: if (this.age++ >= this.lifetime) (PortalParticle.java:60)
	{
		remove();  // Beta: this.remove() (PortalParticle.java:61)
		return;
	}
}
