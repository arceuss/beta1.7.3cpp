#pragma once

#include <stdexcept>

// net.minecraft.src.MinecraftException - thrown by the session-lock check so
// the run loop can show the level-save-conflict screen instead of crashing
class MinecraftException : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};
