#pragma once

#include "client/renderer/portable/RenderTypes.h"

namespace b173
{
namespace render
{
class Device
{
  public:
	virtual ~Device() = default;
	Device() = default;
	Device(const Device &) = delete;
	Device &operator=(const Device &) = delete;
	virtual MeshId createMesh(const MeshData &mesh) = 0;
	virtual void destroyMesh(MeshId id) = 0;
	virtual TextureId createTexture(const TextureDesc &desc, const void *rgba, std::size_t bytes,
									std::size_t rowStride = 0) = 0;
	virtual void updateTexture(TextureId id, Rect region, const void *rgba, std::size_t bytes,
							   std::size_t rowStride = 0) = 0;
	virtual void destroyTexture(TextureId id) = 0;
	virtual void draw(const Draw &draw) = 0;
	virtual void drawTransient(const MeshData &mesh, const Draw &draw) = 0;
};

// Clears use a regular draw, not native vkCmdClearAttachments/ClearRTV: those do
// not honor GL color/depth write masks. Keep the caller's viewport/state intact.
MeshData clearTriangle(float depth);
Draw clearDraw(MeshId triangle, int width, int height, Vec4 color, bool clearColor, bool clearDepth,
			   const PipelineState &callerState);
} // namespace render
} // namespace b173
