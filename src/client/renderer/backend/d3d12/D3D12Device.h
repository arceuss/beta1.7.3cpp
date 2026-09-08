#pragma once

#include "client/renderer/portable/Device.h"

// B173 - Native Direct3D 12 command backend. This header, D3D12Device.cpp and
// D3D12Host.cpp are the only translation units that may see Windows, DXGI or
// D3D12 declarations; no native handle reaches game code through Device.h.
// windows.h is pulled in first, and lean, because d3d12.h is a MIDL header that
// expects the base Win32 types. NOMINMAX keeps the min/max macros away from
// std::min/std::max and std::numeric_limits in this backend's own sources.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d12.h>
#include <dxgiformat.h>

#include <memory>

namespace b173
{
namespace render
{
struct D3D12CreateInfo
{
	ID3D12Device *device = nullptr;
	DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	// D24_UNORM is deliberate, not a default: GL's glPolygonOffset units and
	// D3D12's DepthBias are both counted in the smallest representable depth
	// step of a UNORM depth buffer, so the game's integral offsets map exactly.
	// A float depth target derives that step from each primitive's exponent
	// instead, which would silently change the offset the game asked for.
	DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	unsigned framesInFlight = 3;
	unsigned maxTexturesPerFrame = 2048;
	std::size_t geometryPageBytes = 16 * 1024 * 1024;
	std::size_t uploadPageBytes = 4 * 1024 * 1024;
};

struct D3D12Frame
{
	ID3D12GraphicsCommandList *commands = nullptr;
	ID3D12Resource *colorResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE colorRTV{}, depthDSV{};
	int width = 0, height = 0;
	unsigned slot = 0;
	std::uint64_t serial = 0, completedSerial = 0;
	// The host supplies a recording direct list, RT in RENDER_TARGET state,
	// depth in DEPTH_WRITE state, and submits serials on ONE direct queue.
};

struct D3D12Readback
{
	struct Data;
	std::shared_ptr<Data> data;
};

class D3D12Device final : public Device
{
  public:
	explicit D3D12Device(const D3D12CreateInfo &info);
	// The host must wait for outstanding fences before destroying the backend.
	~D3D12Device() override;
	void beginFrame(const D3D12Frame &frame);
	void endFrame(); // Does not Close(), ExecuteCommandLists(), Signal(), or Present().
	MeshId createMesh(const MeshData &mesh) override;
	void destroyMesh(MeshId id) override;
	TextureId createTexture(const TextureDesc &desc, const void *rgba, std::size_t bytes,
							std::size_t rowStride = 0) override;
	void updateTexture(TextureId id, Rect region, const void *rgba, std::size_t bytes,
					   std::size_t rowStride = 0) override;
	void destroyTexture(TextureId id) override;
	void draw(const Draw &draw) override;
	void drawTransient(const MeshData &mesh, const Draw &draw) override;
	D3D12Readback recordReadback();
	std::vector<std::uint8_t> resolveReadback(const D3D12Readback &ticket, std::uint64_t completedSerial);

  private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace render
} // namespace b173
