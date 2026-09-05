#pragma once

#include "java/Type.h"

class BufferedImage;

namespace FoliageColor
{

int_t get(double x, double y);
int_t getEvergreenColor();
int_t getBirchColor();
// ColorizerFoliage.func_31073_c: the fixed colour used for oak leaf items and render colour.
int_t getDefaultColor();
void setImage(BufferedImage image);

}
