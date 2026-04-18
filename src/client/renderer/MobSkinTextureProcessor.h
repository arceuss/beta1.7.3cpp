#pragma once

#include "client/renderer/HttpTextureProcessor.h"
#include "java/BufferedImage.h"
#include "java/Type.h"

class MobSkinTextureProcessor : public HttpTextureProcessor
{
private:
	int_t width;
	int_t height;

	bool hasAlpha(int_t x0, int_t y0, int_t x1, int_t y1, const unsigned char *pixels, int_t imgWidth);
	void setForceAlpha(int_t x0, int_t y0, int_t x1, int_t y1, unsigned char *pixels, int_t imgWidth);
	void setNoAlpha(int_t x0, int_t y0, int_t x1, int_t y1, unsigned char *pixels, int_t imgWidth);

public:
	virtual BufferedImage process(BufferedImage &in) override;
};
