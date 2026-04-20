#include "client/renderer/texturefx/TextureFlamesFX.h"

#include <cstdlib>

#include "world/level/tile/FireTile.h"

TextureFlamesFX::TextureFlamesFX(int_t variant) : TextureFX(Tile::fire.tex + variant * 16)
{
}

void TextureFlamesFX::onTick()
{
	for (int_t x = 0; x < 16; ++x)
	{
		for (int_t y = 0; y < 20; ++y)
		{
			int_t count = 18;
			float total = current[x + ((y + 1) % 20) * 16] * static_cast<float>(count);

			for (int_t sx = x - 1; sx <= x + 1; ++sx)
			{
				for (int_t sy = y; sy <= y + 1; ++sy)
				{
					if (sx >= 0 && sy >= 0 && sx < 16 && sy < 20)
						total += current[sx + sy * 16];
					++count;
				}
			}

			next[x + y * 16] = total / (static_cast<float>(count) * 1.06f);
			if (y >= 19)
			{
				float r0 = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float r1 = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float r2 = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				next[x + y * 16] = r0 * r0 * r1 * 4.0f + r2 * 0.1f + 0.2f;
			}
		}
	}

	current.swap(next);

	for (int_t i = 0; i < 256; ++i)
	{
		float flame = current[i] * 1.8f;
		if (flame > 1.0f)
			flame = 1.0f;
		if (flame < 0.0f)
			flame = 0.0f;

		int_t red = static_cast<int_t>(flame * 155.0f + 100.0f);
		int_t green = static_cast<int_t>(flame * flame * 255.0f);
		int_t blue = static_cast<int_t>(flame * flame * flame * flame * flame * flame * flame * flame * flame * flame * 255.0f);
		int_t alpha = flame < 0.5f ? 0 : 255;

		if (anaglyphEnabled)
		{
			int_t grayRed = (red * 30 + green * 59 + blue * 11) / 100;
			int_t grayGreen = (red * 30 + green * 70) / 100;
			int_t grayBlue = (red * 30 + blue * 70) / 100;
			red = grayRed;
			green = grayGreen;
			blue = grayBlue;
		}

		imageData[i * 4 + 0] = static_cast<byte_t>(red);
		imageData[i * 4 + 1] = static_cast<byte_t>(green);
		imageData[i * 4 + 2] = static_cast<byte_t>(blue);
		imageData[i * 4 + 3] = static_cast<byte_t>(alpha);
	}
}
