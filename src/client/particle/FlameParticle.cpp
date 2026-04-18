#include "client/particle/FlameParticle.h"

#include "world/level/Level.h"
#include "util/Mth.h"
#include "java/Random.h"
#include <cmath>
#include <cstdlib>

// newb12: FlameParticle.java - flame particle for torches and fire
// Ported 1:1 from newb12/net/minecraft/client/particle/FlameParticle.java
FlameParticle::FlameParticle(Level &level, double x, double y, double z, double xd, double yd, double zd)
	: Particle(level, x, y, z, xd, yd, zd)
{
	// newb12: FlameParticle constructor (FlameParticle.java:9-22)
	this->xd = this->xd * 0.01f + xd;  // newb12: this.xd = this.xd * 0.01F + xd
	this->yd = this->yd * 0.01f + yd;  // newb12: this.yd = this.yd * 0.01F + yd
	this->zd = this->zd * 0.01f + zd;  // newb12: this.zd = this.zd * 0.01F + zd
	// newb12: Randomize position (FlameParticle.java:14-16)
	double newX = x + (random.nextFloat() - random.nextFloat()) * 0.05f;
	double newY = y + (random.nextFloat() - random.nextFloat()) * 0.05f;
	double newZ = z + (random.nextFloat() - random.nextFloat()) * 0.05f;
	setPos(newX, newY, newZ);  // Set the randomized position
	oSize = size;  // newb12: this.oSize = this.size
	rCol = gCol = bCol = 1.0f;  // newb12: this.rCol = this.gCol = this.bCol = 1.0F
	lifetime = (int_t)(8.0 / (random.nextFloat() * 0.8 + 0.2)) + 4;  // newb12: this.lifetime = (int)(8.0 / (Math.random() * 0.8 + 0.2)) + 4
	noPhysics = true;  // newb12: this.noPhysics = true
	tex = 48;  // newb12: this.tex = 48
}

void FlameParticle::render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2)
{
	// newb12: FlameParticle.render() (FlameParticle.java:24-29)
	float s = (age + a) / lifetime;  // newb12: float s = (this.age + a) / this.lifetime
	size = oSize * (1.0f - s * s * 0.5f);  // newb12: this.size = this.oSize * (1.0F - s * s * 0.5F)
	Particle::render(t, a, xa, ya, za, xa2, za2);  // newb12: super.render(t, a, xa, ya, za, xa2, za2)
}

float FlameParticle::getBrightness(float a)
{
	// newb12: FlameParticle.getBrightness() (FlameParticle.java:31-44)
	float l = (age + a) / lifetime;  // newb12: float l = (this.age + a) / this.lifetime
	if (l < 0.0f)  // newb12: if (l < 0.0F)
		l = 0.0f;  // newb12: l = 0.0F
	if (l > 1.0f)  // newb12: if (l > 1.0F)
		l = 1.0f;  // newb12: l = 1.0F
	
	float br = Particle::getBrightness(a);  // newb12: float br = super.getBrightness(a)
	return br * l + (1.0f - l);  // newb12: return br * l + (1.0F - l)
}

void FlameParticle::tick()
{
	// newb12: FlameParticle.tick() (FlameParticle.java:46-63)
	xo = x;  // newb12: this.xo = this.x
	yo = y;  // newb12: this.yo = this.y
	zo = z;  // newb12: this.zo = this.z
	if (age++ >= lifetime)  // newb12: if (this.age++ >= this.lifetime)
	{
		remove();  // newb12: this.remove()
		return;
	}
	
	move(xd, yd, zd);  // newb12: this.move(this.xd, this.yd, this.zd)
	xd *= 0.96f;  // newb12: this.xd *= 0.96F
	yd *= 0.96f;  // newb12: this.yd *= 0.96F
	zd *= 0.96f;  // newb12: this.zd *= 0.96F
	if (onGround)  // newb12: if (this.onGround)
	{
		xd *= 0.7f;  // newb12: this.xd *= 0.7F
		zd *= 0.7f;  // newb12: this.zd *= 0.7F
	}
}
