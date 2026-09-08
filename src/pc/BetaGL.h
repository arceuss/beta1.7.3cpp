#pragma once
#include <glad/glad.h>
struct SDL_Window;

namespace BetaGL
{
enum class Backend
{
	Compatibility,
	Core33,
	ES20,
	Vulkan,
	D3D12
};
Backend backend();
// Consumes --backend VALUE or --backend=VALUE at index, before device creation.
bool consumeBackendArgument(int &index, int argc, char *const argv[]);
bool modern();
bool hasDrawableTarget();
void initialize(SDL_Window *window);
void beginFrame();
void present();
bool supportsOcclusion();
void shutdown();
const char *description();
void drawableSize(int *width, int *height);
} // namespace BetaGL
