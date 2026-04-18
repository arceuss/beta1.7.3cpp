#include "client/particle/ExplodeParticle.h"

#include "world/level/Level.h"
#include "util/Mth.h"
#include "java/Random.h"
#include <cmath>
#include <cstdlib>

// newb12: ExplodeParticle.java - explosion particle for mob deaths and explosions
// Ported 1:1 from newb12/net/minecraft/client/particle/ExplodeParticle.java
ExplodeParticle::ExplodeParticle(Level &level, double x, double y, double z, double xa, double ya, double za)
	: Particle(level, x, y, z, xa, ya, za)
{
	// newb12: ExplodeParticle constructor (ExplodeParticle.java:7-15)
	// newb12: this.xd = xa + (float)(Math.random() * 2.0 - 1.0) * 0.05F
	xd = xa + (float)(random.nextFloat() * 2.0 - 1.0) * 0.05f;
	// newb12: this.yd = ya + (float)(Math.random() * 2.0 - 1.0) * 0.05F
	yd = ya + (float)(random.nextFloat() * 2.0 - 1.0) * 0.05f;
	// newb12: this.zd = za + (float)(Math.random() * 2.0 - 1.0) * 0.05F
	zd = za + (float)(random.nextFloat() * 2.0 - 1.0) * 0.05f;
	// newb12: this.rCol = this.gCol = this.bCol = this.random.nextFloat() * 0.3F + 0.7F
	float col = random.nextFloat() * 0.3f + 0.7f;
	rCol = col;
	gCol = col;
	bCol = col;
	// newb12: this.size = this.random.nextFloat() * this.random.nextFloat() * 6.0F + 1.0F
	size = random.nextFloat() * random.nextFloat() * 6.0f + 1.0f;
	// newb12: this.lifetime = (int)(16.0 / (this.random.nextFloat() * 0.8 + 0.2)) + 2
	lifetime = (int_t)(16.0 / (random.nextFloat() * 0.8 + 0.2)) + 2;
}

void ExplodeParticle::render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2)
{
	// newb12: ExplodeParticle.render() (ExplodeParticle.java:17-20)
	Particle::render(t, a, xa, ya, za, xa2, za2);  // newb12: super.render(t, a, xa, ya, za, xa2, za2)
}

void ExplodeParticle::tick()
{
	// newb12: ExplodeParticle.tick() (ExplodeParticle.java:22-41)
	xo = x;  // newb12: this.xo = this.x
	yo = y;  // newb12: this.yo = this.y
	zo = z;  // newb12: this.zo = this.z
	if (age++ >= lifetime)  // newb12: if (this.age++ >= this.lifetime)
	{
		remove();  // newb12: this.remove()
		return;
	}
	
	// newb12: Update texture based on age (ExplodeParticle.java:31)
	tex = 7 - age * 8 / lifetime;
	
	// newb12: Apply gravity (ExplodeParticle.java:32)
	yd += 0.004;  // newb12: this.yd += 0.004
	move(xd, yd, zd);  // newb12: this.move(this.xd, this.yd, this.zd)
	
	// newb12: Apply friction (ExplodeParticle.java:34-36)
	xd *= 0.9f;  // newb12: this.xd *= 0.9F
	yd *= 0.9f;  // newb12: this.yd *= 0.9F
	zd *= 0.9f;  // newb12: this.zd *= 0.9F
	
	if (onGround)  // newb12: if (this.onGround)
	{
		xd *= 0.7f;  // newb12: this.xd *= 0.7F
		zd *= 0.7f;  // newb12: this.zd *= 0.7F
	}
}
