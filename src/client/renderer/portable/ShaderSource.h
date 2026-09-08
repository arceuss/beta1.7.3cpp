#pragma once

#include <string>

namespace b173
{
namespace render
{
enum class ShaderLanguage
{
	GL330,
	ES100,
	Vulkan450,
	HLSL50
};
// fragmentHighp reports what the context actually offers. GLSL ES 1.00 only
// guarantees mediump in the fragment stage, so the generator lowers the shared
// varying precision instead of refusing to run.
std::string vertexShader(ShaderLanguage language, bool fragmentHighp = true);
std::string fragmentShader(ShaderLanguage language, bool fragmentHighp = true);
std::string hlslShader();
} // namespace render
} // namespace b173
