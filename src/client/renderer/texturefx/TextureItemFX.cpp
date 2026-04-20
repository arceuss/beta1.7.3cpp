#include "client/renderer/texturefx/TextureItemFX.h"

#include <memory>

#include "client/Minecraft.h"
#include "client/skins/TexturePack.h"
#include "java/BufferedImage.h"

namespace
{
	uint32_t packPixel(const unsigned char *rawPixels, int_t pixelIndex)
	{
		uint32_t red = rawPixels[pixelIndex * 4 + 0];
		uint32_t green = rawPixels[pixelIndex * 4 + 1];
		uint32_t blue = rawPixels[pixelIndex * 4 + 2];
		uint32_t alpha = rawPixels[pixelIndex * 4 + 3];
		return (alpha << 24) | (red << 16) | (green << 8) | blue;
	}

	bool loadRegion(const BufferedImage &image, int_t startX, int_t startY, std::array<uint32_t, 256> &out)
	{
		if (image.getWidth() < startX + 16 || image.getHeight() < startY + 16)
			return false;

		const unsigned char *rawPixels = image.getRawPixels();
		for (int_t y = 0; y < 16; ++y)
		{
			for (int_t x = 0; x < 16; ++x)
			{
				int_t pixelIndex = (startX + x) + (startY + y) * image.getWidth();
				out[y * 16 + x] = packPixel(rawPixels, pixelIndex);
			}
		}
		return true;
	}

	BufferedImage loadImage(Minecraft &minecraft, const jstring &resourceName)
	{
		TexturePack *texturePack = minecraft.texturePackRepository.selected;
		if (texturePack == nullptr)
			return {};

		std::unique_ptr<std::istream> stream(texturePack->getResource(resourceName));
		if (stream == nullptr)
			return {};

		return BufferedImage::ImageIO_read(*stream);
	}
}

bool TextureItemFX::loadIconPixels(Minecraft &minecraft, const jstring &resourceName, int_t iconIndex, std::array<uint32_t, 256> &out)
{
	BufferedImage image = loadImage(minecraft, resourceName);
	return loadRegion(image, (iconIndex % 16) * 16, (iconIndex / 16) * 16, out);
}

bool TextureItemFX::loadWholePixels(Minecraft &minecraft, const jstring &resourceName, std::array<uint32_t, 256> &out)
{
	BufferedImage image = loadImage(minecraft, resourceName);
	return loadRegion(image, 0, 0, out);
}

void TextureItemFX::applyAnaglyph(int_t &red, int_t &green, int_t &blue, bool enabled)
{
	if (!enabled)
		return;

	int_t grayRed = (red * 30 + green * 59 + blue * 11) / 100;
	int_t grayGreen = (red * 30 + green * 70) / 100;
	int_t grayBlue = (red * 30 + blue * 70) / 100;
	red = grayRed;
	green = grayGreen;
	blue = grayBlue;
}
