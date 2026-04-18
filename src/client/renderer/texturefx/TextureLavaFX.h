#pragma once

#include "client/renderer/texturefx/TextureFX.h"

class TextureLavaFX : public TextureFX
{
private:
	float red0[256] = {};
	float red1[256] = {};
	float green0[256] = {};
	float green1[256] = {};

public:
	TextureLavaFX();

	void onTick() override;
};
