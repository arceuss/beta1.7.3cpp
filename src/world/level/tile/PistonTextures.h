#pragma once

#include "java/Type.h"

namespace PistonTextures
{
	// opposite face for each direction
	constexpr int_t oppositeFace[6] = {1, 0, 3, 2, 5, 4};
	// x offset for each direction
	constexpr int_t offsetX[6] = {0, 0, 0, 0, -1, 1};
	// y offset for each direction
	constexpr int_t offsetY[6] = {-1, 1, 0, 0, 0, 0};
	// z offset for each direction
	constexpr int_t offsetZ[6] = {0, 0, -1, 1, 0, 0};
}
