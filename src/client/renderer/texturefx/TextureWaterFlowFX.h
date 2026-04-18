#pragma once

#include "client/renderer/texturefx/TextureFX.h"

class TextureWaterFlowFX : public TextureFX
{
private:
	float red0[256] = {};
	float red1[256] = {};
	float green0[256] = {};
	float green1[256] = {};
	int_t tickCounter = 0;

public:
	TextureWaterFlowFX();

	void onTick() override;
};
