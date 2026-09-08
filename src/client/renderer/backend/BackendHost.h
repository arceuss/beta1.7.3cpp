#pragma once
#include "client/renderer/portable/Device.h"
#include <memory>
#include <string>

struct SDL_Window;

namespace b173
{
namespace render
{
// API ownership, submission and presentation stay below the semantic renderer.
// Readback/finish may submit and wait, but do not present or discard the target.
class BackendHost
{
  public:
	virtual ~BackendHost() = default;
	virtual Device &device() = 0;
	virtual bool beginFrame() = 0;
	virtual void present() = 0;
	virtual void finish() = 0;
	virtual void clear(Vec4 color, float depth, bool colorBit, bool depthBit, const PipelineState &state) = 0;
	virtual std::vector<std::uint8_t> readPixels(Rect region) = 0;
	virtual void drawableSize(int *width, int *height) const = 0;
	virtual std::string description() const = 0;
	virtual float maximumLineWidth() const = 0;
	virtual bool supportsOcclusion() const = 0;
	virtual void genQueries(int count, std::uint32_t *ids) = 0;
	virtual void deleteQueries(int count, const std::uint32_t *ids) = 0;
	virtual void beginQuery(std::uint32_t id) = 0;
	virtual void endQuery() = 0;
	virtual std::uint32_t queryResult(std::uint32_t id, bool availability) = 0;
	virtual unsigned error() = 0;
};

std::unique_ptr<BackendHost> createGLHost(SDL_Window *window, bool es);
std::unique_ptr<BackendHost> createVulkanHost(SDL_Window *window);
std::unique_ptr<BackendHost> createD3D12Host(SDL_Window *window);
} // namespace render
} // namespace b173
