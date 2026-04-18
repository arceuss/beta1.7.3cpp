#pragma once

#include "java/Type.h"

class TextureFX
{
public:
	byte_t imageData[1024] = {};
	int_t iconIndex = 0;
	bool anaglyphEnabled = false;
	int_t textureId = 0;
	int_t tileSize = 1;
	int_t tileImage = 0;

	TextureFX(int_t iconIndex);
	virtual ~TextureFX() {}

	virtual void onTick();
};
