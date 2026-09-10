#include "world/entity/PrimedTNT.h"

#include "java/Math.h"
#include "nbt/CompoundTag.h"
#include "util/Mth.h"
#include "world/level/Explosion.h"
#include "world/level/Level.h"

PrimedTNT::PrimedTNT(Level &level) : Entity(level)
{
	blocksBuilding = true;
	setSize(0.98f, 0.98f);
	heightOffset = bbHeight / 2.0f;
	makeStepSound = false;
}

PrimedTNT::PrimedTNT(Level &level, double x, double y, double z) : PrimedTNT(level)
{
	setPos(x, y, z);
	// The launch angle is drawn over 0..2*pi from Math.random (not the entity RNG),
	// then fed through a second degrees-to-radians conversion. Beta really does this,
	// so primed TNT only ever launches within a ~6.3 degree arc. Keep the expression.
	float angle = static_cast<float>(Math::random() * static_cast<double>(Mth::PI) * 2.0);
	xd = -Mth::sin(angle * Mth::PI / 180.0f) * 0.02f;
	yd = 0.2f;
	zd = -Mth::cos(angle * Mth::PI / 180.0f) * 0.02f;
	fuse = 80;
	xo = x;
	yo = y;
	zo = z;
}

bool PrimedTNT::isPickable()
{
	return !removed;
}

void PrimedTNT::tick()
{
	xo = x;
	yo = y;
	zo = z;
	yd -= 0.04f;
	move(xd, yd, zd);
	xd *= 0.98f;
	yd *= 0.98f;
	zd *= 0.98f;
	if (onGround)
	{
		xd *= 0.7f;
		zd *= 0.7f;
		yd *= -0.5;
	}

	// Post-decrement: a fuse of 80 burns for 81 ticks and detonates while the field reads -1.
	if (fuse-- <= 0)
	{
		if (!level.isOnline)
		{
			remove();
			explode();
		}
		else
		{
			remove();
		}
	}
	else
	{
		level.addParticle(u"smoke", x, y + 0.5, z, 0.0, 0.0, 0.0);
	}
}

void PrimedTNT::explode()
{
	float radius = 4.0f;
	// No source entity: the blast owns no damage attribution in Beta.
	level.createExplosion(nullptr, x, y, z, radius);
}

void PrimedTNT::addAdditionalSaveData(CompoundTag &tag)
{
	tag.putByte(u"Fuse", static_cast<byte_t>(fuse));
}

void PrimedTNT::readAdditionalSaveData(CompoundTag &tag)
{
	fuse = tag.getByte(u"Fuse");
}

float PrimedTNT::getShadowHeightOffs()
{
	return 0.0f;
}
