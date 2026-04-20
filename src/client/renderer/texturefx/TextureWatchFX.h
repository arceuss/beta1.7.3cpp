#pragma once

#include <array>
#include <cstdint>

#include "client/renderer/texturefx/TextureFX.h"

class Minecraft;

class TextureWatchFX : public TextureFX
{
private:
	Minecraft &minecraft;
	std::array<uint32_t, 256> watchIconImageData = {};
	std::array<uint32_t, 256> dialImageData = {};
	double rotation = 0.0;
	double rotationDelta = 0.0;

public:
	explicit TextureWatchFX(Minecraft &minecraft);

	void onTick() override;
};
