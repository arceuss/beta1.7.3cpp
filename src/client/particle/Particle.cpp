#include "client/particle/Particle.h"

#include "world/level/Level.h"

#include "util/Mth.h"

#include "world/level/material/LiquidMaterial.h"

#include <cmath>
#include <cstdlib>

double Particle::xOff = 0.0;
double Particle::yOff = 0.0;
double Particle::zOff = 0.0;

Particle::Particle(Level &level, double x, double y, double z, double xa, double ya, double za)
	: Entity(level)
{
	setSize(0.2f, 0.2f);
	heightOffset = bbHeight / 2.0f;
	setPos(x, y, z);
	rCol = gCol = bCol = 1.0f;

	// Beta: Randomize velocity (Particle.java:30-32)
	xd = xa + (float)((random.nextFloat() * 2.0 - 1.0) * 0.4f);
	yd = ya + (float)((random.nextFloat() * 2.0 - 1.0) * 0.4f);
	zd = za + (float)((random.nextFloat() * 2.0 - 1.0) * 0.4f);

	// Beta: Calculate speed and normalize (Particle.java:33-37)
	float speed = (float)((random.nextFloat() + random.nextFloat() + 1.0) * 0.15f);
	float dd = Mth::sqrt((float)(xd * xd + yd * yd + zd * zd));
	xd = xd / dd * speed * 0.4f;
	yd = yd / dd * speed * 0.4f + 0.1f;
	zd = zd / dd * speed * 0.4f;

	// Beta: Randomize texture offset and size (Particle.java:38-41)
	uo = random.nextFloat() * 3.0f;
	vo = random.nextFloat() * 3.0f;
	size = (random.nextFloat() * 0.5f + 0.5f) * 2.0f;
	lifetime = (int_t)(4.0f / (random.nextFloat() * 0.9f + 0.1f));
	age = 0;
	makeStepSound = false;
}

Particle &Particle::setPower(float power)
{
	xd *= power;
	yd = (yd - 0.1f) * power + 0.1f;
	zd *= power;
	return *this;
}

Particle &Particle::scale(float scale)
{
	setSize(0.2f * scale, 0.2f * scale);
	size *= scale;
	return *this;
}

void Particle::tick()
{
	xo = x;
	yo = y;
	zo = z;

	if (age++ >= lifetime)
	{
		remove();
		return;
	}

	// Beta: Apply gravity (Particle.java:72)
	yd = yd - 0.04 * gravity;
	move(xd, yd, zd);

	// Beta: Apply friction (Particle.java:74-80)
	xd *= 0.98f;
	yd *= 0.98f;
	zd *= 0.98f;

	if (onGround)
	{
		xd *= 0.7f;
		zd *= 0.7f;
	}
}

void Particle::render(Tesselator &t, float a, float xa, float ya, float za, float xa2, float za2)
{
	// Beta: Calculate texture coordinates (Particle.java:84-87)
	float u0 = (tex % 16) / 16.0f;
	float u1 = u0 + 0.0624375f;
	float v0 = (tex / 16) / 16.0f;
	float v1 = v0 + 0.0624375f;

	float r = 0.1f * size;
	float x = (float)(xo + (this->x - xo) * a - xOff);
	float y = (float)(yo + (this->y - yo) * a - yOff);
	float z = (float)(zo + (this->z - zo) * a - zOff);

	float br = getBrightness(a);
	t.color(rCol * br, gCol * br, bCol * br);

	// Beta: Render quad (Particle.java:94-97)
	t.vertexUV(x - xa * r - xa2 * r, y - ya * r, z - za * r - za2 * r, u1, v1);
	t.vertexUV(x - xa * r + xa2 * r, y + ya * r, z - za * r + za2 * r, u1, v0);
	t.vertexUV(x + xa * r + xa2 * r, y + ya * r, z + za * r + za2 * r, u0, v0);
	t.vertexUV(x + xa * r - xa2 * r, y - ya * r, z + za * r - za2 * r, u0, v1);
}

int_t Particle::getParticleTexture() const
{
	return 0;  // Beta: MISC_TEXTURE by default (Particle.java:100-102)
}
