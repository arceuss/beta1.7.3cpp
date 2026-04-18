#include "client/renderer/texturefx/TextureLavaFlowFX.h"

#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "world/level/tile/Tile.h"
#include "world/level/tile/LiquidTile.h"

TextureLavaFlowFX::TextureLavaFlowFX() : TextureFX(Tile::lava.tex + 1)
{
	tileSize = 2;
}

void TextureLavaFlowFX::onTick()
{
	++tickCounter;

	for (int_t x = 0; x < 16; ++x)
	{
		for (int_t y = 0; y < 16; ++y)
		{
			float val = 0.0f;
			int_t sinY = static_cast<int_t>(std::sin(static_cast<float>(y) * static_cast<float>(M_PI) * 2.0f / 16.0f) * 1.2f);
			int_t sinX = static_cast<int_t>(std::sin(static_cast<float>(x) * static_cast<float>(M_PI) * 2.0f / 16.0f) * 1.2f);

			for (int_t xx = x - 1; xx <= x + 1; ++xx)
			{
				for (int_t yy = y - 1; yy <= y + 1; ++yy)
				{
					int_t xi = (xx + sinY) & 15;
					int_t yi = (yy + sinX) & 15;
					val += red0[xi + yi * 16];
				}
			}

			red1[x + y * 16] = val / 10.0f +
				(green0[((x + 0) & 15) + ((y + 0) & 15) * 16] +
				 green0[((x + 1) & 15) + ((y + 0) & 15) * 16] +
				 green0[((x + 1) & 15) + ((y + 1) & 15) * 16] +
				 green0[((x + 0) & 15) + ((y + 1) & 15) * 16]) / 4.0f * 0.8f;

			green0[x + y * 16] += green1[x + y * 16] * 0.01f;
			if (green0[x + y * 16] < 0.0f)
				green0[x + y * 16] = 0.0f;

			green1[x + y * 16] -= 0.06f;
			if ((static_cast<double>(std::rand()) / RAND_MAX) < 0.005)
				green1[x + y * 16] = 1.5f;
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
		float val = red0[i - tickCounter / 3 * 16 & 255] * 2.0f;
		if (val > 1.0f) val = 1.0f;
		if (val < 0.0f) val = 0.0f;

		int_t r = static_cast<int_t>(val * 100.0f + 155.0f);
		int_t g = static_cast<int_t>(val * val * 255.0f);
		int_t b = static_cast<int_t>(val * val * val * val * 128.0f);

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
		imageData[i * 4 + 3] = static_cast<byte_t>(255);
	}
}
