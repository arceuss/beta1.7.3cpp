#pragma once

#include <array>
#include <cstdint>

#include "client/renderer/texturefx/TextureFX.h"

class Minecraft;

class TextureCompassFX : public TextureFX
{
private:
	Minecraft &minecraft;
	std::array<uint32_t, 256> compassIconImageData = {};
	double rotation = 0.0;
	double rotationDelta = 0.0;

public:
	explicit TextureCompassFX(Minecraft &minecraft);

	void onTick() override;
};
