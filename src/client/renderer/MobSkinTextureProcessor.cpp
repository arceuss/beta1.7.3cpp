#include "client/renderer/MobSkinTextureProcessor.h"

#include "java/BufferedImage.h"

BufferedImage MobSkinTextureProcessor::process(BufferedImage &in)
{
	if (in.getWidth() == 0 || in.getHeight() == 0)
	{
		return BufferedImage();
	}

	width = 64;
	height = 32;
	BufferedImage out(width, height);

	int_t srcW = in.getWidth();
	int_t srcH = in.getHeight();
	int_t dstW = width;
	int_t dstH = height;

	unsigned char *dstPixels = const_cast<unsigned char *>(out.getRawPixels());
	const unsigned char *srcPixels = in.getRawPixels();

	for (int_t y = 0; y < dstH; y++)
	{
		int_t srcY = (y * srcH) / dstH;
		for (int_t x = 0; x < dstW; x++)
		{
			int_t srcX = (x * srcW) / dstW;
			int_t srcIdx = (srcY * srcW + srcX) * 4;
			int_t dstIdx = (y * dstW + x) * 4;
			dstPixels[dstIdx + 0] = srcPixels[srcIdx + 0];
			dstPixels[dstIdx + 1] = srcPixels[srcIdx + 1];
			dstPixels[dstIdx + 2] = srcPixels[srcIdx + 2];
			dstPixels[dstIdx + 3] = srcPixels[srcIdx + 3];
		}
	}

	setNoAlpha(0, 0, 32, 16, dstPixels, width);
	setForceAlpha(32, 0, 64, 32, dstPixels, width);
	setNoAlpha(0, 16, 64, 32, dstPixels, width);

	return out;
}

bool MobSkinTextureProcessor::hasAlpha(int_t x0, int_t y0, int_t x1, int_t y1, const unsigned char *pixels, int_t imgWidth)
{
	for (int_t x = x0; x < x1; x++)
	{
		for (int_t y = y0; y < y1; y++)
		{
			int_t idx = (y * imgWidth + x) * 4 + 3;
			if (pixels[idx] < 128)
			{
				return true;
			}
		}
	}
	return false;
}

void MobSkinTextureProcessor::setForceAlpha(int_t x0, int_t y0, int_t x1, int_t y1, unsigned char *pixels, int_t imgWidth)
{
	if (!hasAlpha(x0, y0, x1, y1, pixels, imgWidth))
	{
		for (int_t x = x0; x < x1; x++)
		{
			for (int_t y = y0; y < y1; y++)
			{
				int_t idx = (y * imgWidth + x) * 4;
				pixels[idx + 3] = 0;
			}
		}
	}
}

void MobSkinTextureProcessor::setNoAlpha(int_t x0, int_t y0, int_t x1, int_t y1, unsigned char *pixels, int_t imgWidth)
{
	for (int_t x = x0; x < x1; x++)
	{
		for (int_t y = y0; y < y1; y++)
		{
			int_t idx = (y * imgWidth + x) * 4;
			pixels[idx + 3] = 255;
		}
	}
}
