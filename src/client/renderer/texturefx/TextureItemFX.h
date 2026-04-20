#pragma once

#include <array>
#include <cstdint>

#include "java/String.h"
#include "java/Type.h"

class Minecraft;

namespace TextureItemFX
{
	bool loadIconPixels(Minecraft &minecraft, const jstring &resourceName, int_t iconIndex, std::array<uint32_t, 256> &out);
	bool loadWholePixels(Minecraft &minecraft, const jstring &resourceName, std::array<uint32_t, 256> &out);
	void applyAnaglyph(int_t &red, int_t &green, int_t &blue, bool enabled);
}
