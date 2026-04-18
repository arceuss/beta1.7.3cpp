#pragma once

#include "java/BufferedImage.h"

class HttpTextureProcessor
{
public:
	virtual ~HttpTextureProcessor() {}
	virtual BufferedImage process(BufferedImage &in) = 0;
};
