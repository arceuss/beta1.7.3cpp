#include "util/Mth.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

static constexpr int_t BIG_ENOUGH_INT = 1024;
static constexpr float BIG_ENOUGH_FLOAT = 1024.0f;

namespace
{

int_t javaNumberToInt(double value)
{
	if (std::isnan(value))
		return 0;
	if (value <= static_cast<double>(std::numeric_limits<int_t>::min()))
		return std::numeric_limits<int_t>::min();
	if (value >= static_cast<double>(std::numeric_limits<int_t>::max()))
		return std::numeric_limits<int_t>::max();
	return static_cast<int_t>(value);
}

int_t javaSubtractOne(int_t value)
{
	uint_t bits = static_cast<uint_t>(value) - 1U;
	int_t result;
	std::memcpy(&result, &bits, sizeof(result));
	return result;
}

}

namespace Mth
{

namespace
{

struct SinTable
{
	float table[65536];

	SinTable()
	{
		for (int_t i = 0; i < 65536; i++)
			table[i] = static_cast<float>(std::sin(i * 3.141592653589793 * 2.0 / 65536.0));
	}
} static const SIN_TABLE;

}

float sin(float angle)
{
	return SIN_TABLE.table[javaNumberToInt(angle * 10430.378f) & 65535];
}
float cos(float angle)
{
	return SIN_TABLE.table[javaNumberToInt(angle * 10430.378f + 16384.0f) & 65535];
}

float sqrt(float value)
{
	return std::sqrt(value);
}
float sqrt(double value)
{
	return std::sqrt(value);
}

int_t floor(float value)
{
	int_t i = javaNumberToInt(value);
	return (value < i) ? javaSubtractOne(i) : i;
}
int_t fastFloor(double value)
{
	return static_cast<int_t>(value + BIG_ENOUGH_FLOAT) - BIG_ENOUGH_INT;
}
int_t floor(double value)
{
	int_t i = javaNumberToInt(value);
	return (value < i) ? javaSubtractOne(i) : i;
}
int_t absFloor(double value)
{
	return (value >= 0.0) ? value : (-value + 1);
}

float abs(float value)
{
	return (value >= 0.0f) ? value : -value;
}

int_t ceil(float value)
{
	int_t i = value;
	return (value > i) ? (i + 1) : i;
}

/*
class Main
{
public:
	Main()
	{
		for (Type::byte i = -64; i <= 64; i++)
			std::cout << static_cast<int>(i) << " -> " << intFloorDiv(i, 32) << '\n';
	}
};
static Main main;
*/

double asbMax(double a, double b)
{
	if (a < 0)
		a = -a;
	if (b < 0)
		b = -b;
	return (a > b) ? a : b;
}

int_t intFloorDiv(int_t a, int_t b)
{
	return (a < 0) ? (-((-a - 1) / b) - 1) : (a / b);
}

}
