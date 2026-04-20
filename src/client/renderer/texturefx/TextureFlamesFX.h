#pragma once

#include <array>

#include "client/renderer/texturefx/TextureFX.h"

class TextureFlamesFX : public TextureFX
{
private:
	std::array<float, 320> current = {};
	std::array<float, 320> next = {};

public:
	explicit TextureFlamesFX(int_t variant);

	void onTick() override;
};
