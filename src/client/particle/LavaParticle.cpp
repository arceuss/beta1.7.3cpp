#include "client/particle/LavaParticle.h"

#include "world/level/Level.h"
#include "util/Mth.h"
#include "java/Random.h"
#include <cmath>
#include <cstdlib>

// newb12: LavaParticle.java - lava particle for lava blocks
// Ported 1:1 from newb12/net/minecraft/client/particle/LavaParticle.java
LavaParticle::LavaParticle(Level &level, double x, double y, double z)
	: Particle(level, x, y, z, 0.0, 0.0, 0.0)
{
	// newb12: LavaParticle constructor (LavaParticle.java:9-21)
	xd *= 0.8f;  // newb12: this.xd *= 0.8F
	yd *= 0.8f;  // newb12: this.yd *= 0.8F
	zd *= 0.8f;  // newb12: this.zd *= 0.8F
	yd = random.nextFloat() * 0.4f + 0.05f;  // newb12: this.yd = this.random.nextFloat() * 0.4F + 0.05F
	rCol = gCol = bCol = 1.0f;  // newb12: this.rCol = this.gCol = this.bCol = 1.0F
	size = size * (random.nextFloat() * 2.0f + 0.2f);  // newb12: this.size = this.size * (this.random.nextFloat() * 2.0F + 0.2F)
	oSize = size;  // newb12: this.oSize = this.size
	lifetime = (int_t)(16.0 / (random.nextFloat() * 0.8 + 0.2));  // newb12: this.lifetime = (int)(16.0 / (Math.random() * 0.8 + 0.2))
	noPhysics = false;  // newb12: this.noPhysics = false
	tex = 49;  // newb12: this.tex = 49
}

float LavaParticle::getBrightness(float a)
{
	// newb12: LavaParticle.getBrightness() (LavaParticle.java:23-26)
	return 1.0f;  // newb12: return 1.0F
}

void LavaParticle::render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2)
{
	// newb12: LavaParticle.render() (LavaParticle.java:28-33)
	float s = (age + a) / lifetime;  // newb12: float s = (this.age + a) / this.lifetime
	size = oSize * (1.0f - s * s);  // newb12: this.size = this.oSize * (1.0F - s * s)
	Particle::render(t, a, xa, ya, za, xa2, za2);  // newb12: super.render(t, a, xa, ya, za, xa2, za2)
}

void LavaParticle::tick()
{
	// newb12: LavaParticle.tick() (LavaParticle.java:35-58)
	xo = x;  // newb12: this.xo = this.x
	yo = y;  // newb12: this.yo = this.y
	zo = z;  // newb12: this.zo = this.z
	if (age++ >= lifetime)  // newb12: if (this.age++ >= this.lifetime)
	{
		remove();  // newb12: this.remove()
		return;
	}
	
	// newb12: Spawn smoke particles randomly (LavaParticle.java:44-47)
	float odds = (float)age / lifetime;  // newb12: float odds = (float)this.age / this.lifetime
	if (random.nextFloat() > odds)  // newb12: if (this.random.nextFloat() > odds)
	{
		level.addParticle(u"smoke", x, y, z, xd, yd, zd);  // newb12: this.level.addParticle("smoke", this.x, this.y, this.z, this.xd, this.yd, this.zd)
	}
	
	yd -= 0.03;  // newb12: this.yd -= 0.03
	move(xd, yd, zd);  // newb12: this.move(this.xd, this.yd, this.zd)
	xd *= 0.999f;  // newb12: this.xd *= 0.999F
	yd *= 0.999f;  // newb12: this.yd *= 0.999F
	zd *= 0.999f;  // newb12: this.zd *= 0.999F
	if (onGround)  // newb12: if (this.onGround)
	{
		xd *= 0.7f;  // newb12: this.xd *= 0.7F
		zd *= 0.7f;  // newb12: this.zd *= 0.7F
	}
}
