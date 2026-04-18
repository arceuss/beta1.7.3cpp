#include "client/particle/SmokeParticle.h"

#include "world/level/Level.h"
#include "util/Mth.h"
#include "java/Random.h"
#include <cmath>

// Beta 1.2: SmokeParticle.java - smoke particle for fire and other effects
SmokeParticle::SmokeParticle(Level &level, double x, double y, double z, double xa, double ya, double za)
	: SmokeParticle(level, x, y, z, xa, ya, za, 1.0f)
{
	// Beta: Delegates to scale constructor (SmokeParticle.java:9-11)
}

SmokeParticle::SmokeParticle(Level &level, double x, double y, double z, double xa, double ya, double za, float scale)
	: Particle(level, x, y, z, 0.0, 0.0, 0.0)
{
	// Beta: SmokeParticle constructor (SmokeParticle.java:13-28)
	xd *= 0.1f;  // Beta: this.xd *= 0.1F (SmokeParticle.java:15)
	yd *= 0.1f;  // Beta: this.yd *= 0.1F (SmokeParticle.java:16)
	zd *= 0.1f;  // Beta: this.zd *= 0.1F (SmokeParticle.java:17)
	xd += xa;  // Beta: this.xd += xa (SmokeParticle.java:18)
	yd += ya;  // Beta: this.yd += ya (SmokeParticle.java:19)
	zd += za;  // Beta: this.zd += za (SmokeParticle.java:20)
	
	// Beta: Random gray color (SmokeParticle.java:21)
	float gray = (float)(random.nextFloat() * 0.3f);
	rCol = gray;
	gCol = gray;
	bCol = gray;
	
	size *= 0.75f;  // Beta: this.size *= 0.75F (SmokeParticle.java:22)
	size *= scale;  // Beta: this.size *= scale (SmokeParticle.java:23)
	oSize = size;  // Beta: this.oSize = this.size (SmokeParticle.java:24)
	
	// Beta: Calculate lifetime (SmokeParticle.java:25-26)
	lifetime = (int_t)(8.0 / (random.nextFloat() * 0.8 + 0.2));
	lifetime = (int_t)(lifetime * scale);
	
	noPhysics = false;  // Beta: this.noPhysics = false (SmokeParticle.java:27)
}

void SmokeParticle::tick()
{
	// Beta: SmokeParticle.tick() (SmokeParticle.java:46-69)
	xo = x;  // Beta: this.xo = this.x (SmokeParticle.java:47)
	yo = y;  // Beta: this.yo = this.y (SmokeParticle.java:48)
	zo = z;  // Beta: this.zo = this.z (SmokeParticle.java:49)
	
	if (age++ >= lifetime)  // Beta: if (this.age++ >= this.lifetime) (SmokeParticle.java:50)
	{
		remove();  // Beta: this.remove() (SmokeParticle.java:51)
		return;
	}
	
	// Beta: Update texture based on age (SmokeParticle.java:54)
	tex = 7 - age * 8 / lifetime;
	
	yd += 0.004;  // Beta: this.yd += 0.004 (SmokeParticle.java:55)
	move(xd, yd, zd);  // Beta: this.move(this.xd, this.yd, this.zd) (SmokeParticle.java:56)
	
	if (y == yo)  // Beta: if (this.y == this.yo) (SmokeParticle.java:57)
	{
		xd *= 1.1;  // Beta: this.xd *= 1.1 (SmokeParticle.java:58)
		zd *= 1.1;  // Beta: this.zd *= 1.1 (SmokeParticle.java:59)
	}
	
	xd *= 0.96f;  // Beta: this.xd *= 0.96F (SmokeParticle.java:62)
	yd *= 0.96f;  // Beta: this.yd *= 0.96F (SmokeParticle.java:63)
	zd *= 0.96f;  // Beta: this.zd *= 0.96F (SmokeParticle.java:64)
	
	if (onGround)  // Beta: if (this.onGround) (SmokeParticle.java:65)
	{
		xd *= 0.7f;  // Beta: this.xd *= 0.7F (SmokeParticle.java:66)
		zd *= 0.7f;  // Beta: this.zd *= 0.7F (SmokeParticle.java:67)
	}
}

void SmokeParticle::render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2)
{
	// Beta: SmokeParticle.render() - scales size based on age (SmokeParticle.java:31-43)
	float l = (age + a) / lifetime * 32.0f;  // Beta: float l = (this.age + a) / this.lifetime * 32.0F (SmokeParticle.java:32)
	if (l < 0.0f)  // Beta: if (l < 0.0F) (SmokeParticle.java:33)
		l = 0.0f;  // Beta: l = 0.0F (SmokeParticle.java:34)
	if (l > 1.0f)  // Beta: if (l > 1.0F) (SmokeParticle.java:37)
		l = 1.0f;  // Beta: l = 1.0F (SmokeParticle.java:38)
	
	size = oSize * l;  // Beta: this.size = this.oSize * l (SmokeParticle.java:41)
	Particle::render(t, a, xa, ya, za, xa2, za2);  // Beta: super.render(t, a, xa, ya, za, xa2, za2) (SmokeParticle.java:42)
}
