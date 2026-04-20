#pragma once

#include <array>

#include "client/renderer/texturefx/TextureFX.h"

class TexturePortalFX : public TextureFX
{
private:
	std::array<std::array<byte_t, 1024>, 32> frames = {};
	int_t portalTickCounter = 0;

public:
	explicit TexturePortalFX(int_t iconIndex);

	void onTick() override;
};
