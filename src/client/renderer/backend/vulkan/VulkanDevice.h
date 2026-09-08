#pragma once

#include "client/renderer/portable/Device.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace b173
{
namespace render
{
struct VulkanCreateInfo
{
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
	VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
	unsigned framesInFlight = 3;
	unsigned maxDescriptorSetsPerFrame = 8192;
	std::size_t geometryPageBytes = 16 * 1024 * 1024;
	std::size_t uploadPageBytes = 4 * 1024 * 1024;
	bool wideLinesEnabled = false; // Must match the feature enabled on VkDevice.
	std::vector<std::uint32_t> vertexSpirv, fragmentSpirv;
};

struct VulkanFrame
{
	// A primary command buffer, recording, outside a render pass. Submit these
	// frames IN SERIAL ORDER on one graphics queue. No hidden queue ownership.
	VkCommandBuffer commands = VK_NULL_HANDLE;
	VkImageView colorView = VK_NULL_HANDLE, depthView = VK_NULL_HANDLE;
	int width = 0, height = 0;
	unsigned slot = 0;
	std::uint64_t serial = 0, completedSerial = 0;
	// Host transitions attachments to COLOR_ATTACHMENT_OPTIMAL and
	// DEPTH_STENCIL_ATTACHMENT_OPTIMAL before beginFrame; they remain there.
};

struct VulkanReadback
{
	struct Data;
	std::shared_ptr<Data> data;
};

class VulkanDevice final : public Device
{
  public:
	explicit VulkanDevice(const VulkanCreateInfo &info);
	// Host must retire all submitted work and discard unsubmitted commands first.
	~VulkanDevice() override;
	void beginFrame(const VulkanFrame &frame);
	void endFrame(); // Ends render pass and flushes mapped writes; does not submit.
	// Drops the cached framebuffers so the host can destroy the views they were
	// built from. The host must have retired every submission that used them.
	void discardTargets();
	// Keeps a host-owned object (a query pool) alive for as long as the frame
	// slot recording right now may still be executing on the queue.
	void retainFrameResource(const std::shared_ptr<void> &resource);
	// Real occlusion queries. Reset, begin and end are recorded outside the
	// render pass: a pool reset is illegal inside one, and a texture or mesh
	// upload that splits the pass in the middle of a query stays legal.
	void beginOcclusionQuery(VkQueryPool pool, std::uint32_t index);
	void endOcclusionQuery(VkQueryPool pool, std::uint32_t index);
	MeshId createMesh(const MeshData &mesh) override;
	void destroyMesh(MeshId id) override;
	TextureId createTexture(const TextureDesc &desc, const void *rgba, std::size_t bytes,
							std::size_t rowStride = 0) override;
	void updateTexture(TextureId id, Rect region, const void *rgba, std::size_t bytes,
					   std::size_t rowStride = 0) override;
	void destroyTexture(TextureId id) override;
	void draw(const Draw &draw) override;
	void drawTransient(const MeshData &mesh, const Draw &draw) override;
	// Copy the whole current color target. This is an ordered asynchronous copy;
	// resolve only after the host fence reports ticket serial completion.
	VulkanReadback recordReadback(VkImage currentColorImage);
	std::vector<std::uint8_t> resolveReadback(const VulkanReadback &ticket, std::uint64_t completedSerial);

  private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace render
} // namespace b173
