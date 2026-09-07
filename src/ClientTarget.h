#pragma once

#include "java/Type.h"

namespace ClientTarget
{

enum class Type
{
	Beta173,
	AlphaPlace,
};

#if defined(B173_TARGET_ALPHAPLACE)
static constexpr Type CURRENT = Type::AlphaPlace;
#else
static constexpr Type CURRENT = Type::Beta173;
#endif

constexpr bool isAlphaPlace()
{
	return CURRENT == Type::AlphaPlace;
}

// AlphaPlace keeps the b1.7.3 packet layouts with a server-specific login version.
constexpr int_t loginProtocolVersion()
{
	return isAlphaPlace() ? 2000 : 14;
}

// Apply at the Java AP simulation call sites, not to all Level/SoundEngine audio.
constexpr bool useServerSoundEvents(bool multiplayer)
{
	return isAlphaPlace() && multiplayer;
}

}
