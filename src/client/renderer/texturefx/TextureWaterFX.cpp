#include "client/renderer/texturefx/TextureWaterFX.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "world/level/tile/Tile.h"
#include "world/level/tile/LiquidTile.h"

TextureWaterFX::TextureWaterFX() : TextureFX(Tile::water.tex)
{
}

void TextureWaterFX::onTick()
{
	++tickCounter;

	for (int_t x = 0; x < 16; ++x)
	{
		for (int_t y = 0; y < 16; ++y)
		{
			float val = 0.0f;

			for (int_t xx = x - 1; xx <= x + 1; ++xx)
			{
				int_t xi = xx & 15;
				int_t yi = y & 15;
				val += red0[xi + yi * 16];
			}

			red1[x + y * 16] = val / 3.3f + green0[x + y * 16] * 0.8f;
		}
	}

	for (int_t x = 0; x < 16; ++x)
	{
		for (int_t y = 0; y < 16; ++y)
		{
			green0[x + y * 16] += green1[x + y * 16] * 0.05f;
			if (green0[x + y * 16] < 0.0f)
				green0[x + y * 16] = 0.0f;

			green1[x + y * 16] -= 0.1f;
			if ((static_cast<double>(std::rand()) / RAND_MAX) < 0.05)
				green1[x + y * 16] = 0.5f;
		}
	}

	// Swap buffers
	float *temp = new float[256];
	std::memcpy(temp, red1, sizeof(float) * 256);
	std::memcpy(red1, red0, sizeof(float) * 256);
	std::memcpy(red0, temp, sizeof(float) * 256);
	delete[] temp;

	for (int_t i = 0; i < 256; ++i)
	{
		float val = red0[i];
		if (val > 1.0f) val = 1.0f;
		if (val < 0.0f) val = 0.0f;

		float sq = val * val;
		int_t r = static_cast<int_t>(32.0f + sq * 32.0f);
		int_t g = static_cast<int_t>(50.0f + sq * 64.0f);
		int_t b = 255;
		int_t a = static_cast<int_t>(146.0f + sq * 50.0f);

		if (anaglyphEnabled)
		{
			int_t rr = (r * 30 + g * 59 + b * 11) / 100;
			int_t gg = (r * 30 + g * 70) / 100;
			int_t bb = (r * 30 + b * 70) / 100;
			r = rr;
			g = gg;
			b = bb;
		}

		imageData[i * 4 + 0] = static_cast<byte_t>(r);
		imageData[i * 4 + 1] = static_cast<byte_t>(g);
		imageData[i * 4 + 2] = static_cast<byte_t>(b);
		imageData[i * 4 + 3] = static_cast<byte_t>(a);
	}
}
