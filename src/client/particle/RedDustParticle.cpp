#include "client/particle/RedDustParticle.h"

#include "world/level/Level.h"
#include "util/Mth.h"
#include "java/Random.h"
#include <cmath>

// Beta 1.2: RedDustParticle.java - redstone dust particle
RedDustParticle::RedDustParticle(Level &level, double x, double y, double z)
	: RedDustParticle(level, x, y, z, 1.0f)
{
	// Beta: Delegates to scale constructor (RedDustParticle.java:9-11)
}

RedDustParticle::RedDustParticle(Level &level, double x, double y, double z, float scale)
	: Particle(level, x, y, z, 0.0, 0.0, 0.0)
{
	// Beta: RedDustParticle constructor (RedDustParticle.java:13-26)
	xd *= 0.1f;  // Beta: this.xd *= 0.1F (RedDustParticle.java:15)
	yd *= 0.1f;  // Beta: this.yd *= 0.1F (RedDustParticle.java:16)
	zd *= 0.1f;  // Beta: this.zd *= 0.1F (RedDustParticle.java:17)
	
	// Beta: Red color with random variation (RedDustParticle.java:18-19)
	rCol = (float)(random.nextFloat() * 0.3f) + 0.7f;  // Beta: this.rCol = (float)(Math.random() * 0.3F) + 0.7F
	gCol = bCol = (float)(random.nextFloat() * 0.1f);  // Beta: this.gCol = this.bCol = (float)(Math.random() * 0.1F)
	
	size *= 0.75f;  // Beta: this.size *= 0.75F (RedDustParticle.java:20)
	size *= scale;  // Beta: this.size *= scale (RedDustParticle.java:21)
	oSize = size;  // Beta: this.oSize = this.size (RedDustParticle.java:22)
	
	// Beta: Calculate lifetime (RedDustParticle.java:23-24)
	lifetime = (int_t)(8.0 / (random.nextFloat() * 0.8 + 0.2));
	lifetime = (int_t)(lifetime * scale);
	
	noPhysics = false;  // Beta: this.noPhysics = false (RedDustParticle.java:25)
}

void RedDustParticle::tick()
{
	// Beta: RedDustParticle.tick() (RedDustParticle.java:44-66)
	xo = x;  // Beta: this.xo = this.x (RedDustParticle.java:45)
	yo = y;  // Beta: this.yo = this.y (RedDustParticle.java:46)
	zo = z;  // Beta: this.zo = this.z (RedDustParticle.java:47)
	
	if (age++ >= lifetime)  // Beta: if (this.age++ >= this.lifetime) (RedDustParticle.java:48)
	{
		remove();  // Beta: this.remove() (RedDustParticle.java:49)
		return;
	}
	
	// Beta: Update texture based on age (RedDustParticle.java:52)
	tex = 7 - age * 8 / lifetime;
	
	move(xd, yd, zd);  // Beta: this.move(this.xd, this.yd, this.zd) (RedDustParticle.java:53)
	
	if (y == yo)  // Beta: if (this.y == this.yo) (RedDustParticle.java:54)
	{
		xd *= 1.1;  // Beta: this.xd *= 1.1 (RedDustParticle.java:55)
		zd *= 1.1;  // Beta: this.zd *= 1.1 (RedDustParticle.java:56)
	}
	
	xd *= 0.96f;  // Beta: this.xd *= 0.96F (RedDustParticle.java:59)
	yd *= 0.96f;  // Beta: this.yd *= 0.96F (RedDustParticle.java:60)
	zd *= 0.96f;  // Beta: this.zd *= 0.96F (RedDustParticle.java:61)
	
	if (onGround)  // Beta: if (this.onGround) (RedDustParticle.java:62)
	{
		xd *= 0.7f;  // Beta: this.xd *= 0.7F (RedDustParticle.java:63)
		zd *= 0.7f;  // Beta: this.zd *= 0.7F (RedDustParticle.java:64)
	}
}

void RedDustParticle::render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2)
{
	// Beta: RedDustParticle.render() - scales size based on age (RedDustParticle.java:29-41)
	float l = (age + a) / lifetime * 32.0f;  // Beta: float l = (this.age + a) / this.lifetime * 32.0F (RedDustParticle.java:30)
	if (l < 0.0f)  // Beta: if (l < 0.0F) (RedDustParticle.java:31)
		l = 0.0f;  // Beta: l = 0.0F (RedDustParticle.java:32)
	if (l > 1.0f)  // Beta: if (l > 1.0F) (RedDustParticle.java:35)
		l = 1.0f;  // Beta: l = 1.0F (RedDustParticle.java:36)
	
	size = oSize * l;  // Beta: this.size = this.oSize * l (RedDustParticle.java:39)
	Particle::render(t, a, xa, ya, za, xa2, za2);  // Beta: super.render(t, a, xa, ya, za, xa2, za2) (RedDustParticle.java:40)
}
