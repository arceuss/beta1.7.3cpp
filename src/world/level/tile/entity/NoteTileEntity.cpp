#include "world/level/tile/entity/NoteTileEntity.h"

#include "world/level/Level.h"
#include "world/level/material/Material.h"
#include "world/level/material/GasMaterial.h"

void NoteTileEntity::load(CompoundTag &tag)
{
	TileEntity::load(tag);
	note = tag.getByte(u"note");
	if (note < 0)
		note = 0;
	if (note > 24)
		note = 24;
}

void NoteTileEntity::save(CompoundTag &tag)
{
	TileEntity::save(tag);
	tag.putByte(u"note", note);
}

void NoteTileEntity::changePitch()
{
	note = static_cast<byte_t>((note + 1) % 25);
	setChanged();
}

void NoteTileEntity::triggerNote(Level &level, int_t x, int_t y, int_t z)
{
	const Material &airMaterial = Material::air;
	if (&level.getMaterial(x, y + 1, z) != &airMaterial)
		return;

	const Material &below = level.getMaterial(x, y - 1, z);
	int_t instrument = 0;
	if (&below == &Material::stone)
		instrument = 1;
	if (&below == &Material::sand)
		instrument = 2;
	if (&below == &Material::glass)
		instrument = 3;
	if (&below == &Material::wood)
		instrument = 4;

	// MusicTileEntity.java:50 - the sound and particle belong to NoteTile::playBlock,
	// reached both from here and from a received block-event packet
	level.playNoteAt(x, y, z, instrument, note);
}