#include "client/renderer/backend/vulkan/VulkanDevice.h"

#include "client/renderer/portable/FrameLifetime.h"
#include "client/renderer/portable/LegacyState.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <unordered_map>
#include <utility>

namespace b173
{
namespace render
{
namespace VulkanDetail
{
static void check(VkResult result, const char *where)
{
	if (result != VK_SUCCESS)
		throw std::runtime_error(std::string(where) + ": VkResult " + std::to_string(int(result)));
}

static std::int64_t clampRange(std::int64_t value, std::int64_t low, std::int64_t high)
{
	return value < low ? low : (value > high ? high : value);
}

static std::uint32_t memoryType(const VkPhysicalDeviceMemoryProperties &p, std::uint32_t mask,
								VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred = 0)
{
	for (unsigned pass = 0; pass < 2; ++pass)
		for (std::uint32_t i = 0; i < p.memoryTypeCount; ++i)
			if ((mask & (1u << i)) && (p.memoryTypes[i].propertyFlags & required) == required &&
				(pass || (p.memoryTypes[i].propertyFlags & preferred) == preferred))
				return i;
	throw std::runtime_error("required Vulkan memory type is unavailable");
}

struct Buffer
{
	VkDevice device = VK_NULL_HANDLE;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDeviceSize size = 0;
	void *mapped = nullptr;
	bool coherent = false;

	~Buffer()
	{
		if (mapped)
			vkUnmapMemory(device, memory);
		if (buffer)
			vkDestroyBuffer(device, buffer, nullptr);
		if (memory)
			vkFreeMemory(device, memory, nullptr);
	}

	void flush() const
	{
		if (!mapped || coherent)
			return;
		VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		check(vkFlushMappedMemoryRanges(device, 1, &range), "flush upload memory");
	}

	void invalidate() const
	{
		if (!mapped || coherent)
			return;
		VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		check(vkInvalidateMappedMemoryRanges(device, 1, &range), "invalidate readback memory");
	}
};

// The buffer owns its allocation and unmaps/frees it on every failure path.
static std::shared_ptr<Buffer> makeBuffer(VkDevice device, const VkPhysicalDeviceMemoryProperties &memory,
										  VkDeviceSize size, VkBufferUsageFlags usage, bool host)
{
	auto b = std::make_shared<Buffer>();
	b->device = device;
	b->size = size;
	VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	create.size = size;
	create.usage = usage;
	create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	check(vkCreateBuffer(device, &create, nullptr, &b->buffer), "create buffer");
	VkMemoryRequirements requirements{};
	vkGetBufferMemoryRequirements(device, b->buffer, &requirements);
	const std::uint32_t type =
		memoryType(memory, requirements.memoryTypeBits,
				   host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   host ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0);
	VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocate.allocationSize = requirements.size;
	allocate.memoryTypeIndex = type;
	check(vkAllocateMemory(device, &allocate, nullptr, &b->memory), "allocate buffer memory");
	check(vkBindBufferMemory(device, b->buffer, b->memory, 0), "bind buffer memory");
	b->coherent = (memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
	if (host)
		check(vkMapMemory(device, b->memory, 0, VK_WHOLE_SIZE, 0, &b->mapped), "map host buffer");
	return b;
}

struct GeometryPage
{
	std::shared_ptr<Buffer> buffer;
	RangeAllocator free;

	explicit GeometryPage(std::shared_ptr<Buffer> b) : buffer(std::move(b)), free(buffer->size) {}
};

struct Slice
{
	// page is set only for arena slices, which return their range on destruction.
	// Immediate geometry points straight at a frame upload page instead.
	std::shared_ptr<GeometryPage> page;
	std::shared_ptr<Buffer> buffer;
	ByteRange range;
	VkDeviceSize indexOffset = 0;
	std::uint32_t count = 0;

	~Slice()
	{
		if (page)
			page->free.release(range);
	}
};

struct Texture
{
	VkDevice device = VK_NULL_HANDLE;
	TextureDesc desc;
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	bool initialized = false;

	struct Update
	{
		Rect region;
		std::vector<std::uint8_t> pixels;
	};

	std::vector<Update> pending;

	~Texture()
	{
		if (sampler)
			vkDestroySampler(device, sampler, nullptr);
		if (view)
			vkDestroyImageView(device, view, nullptr);
		if (image)
			vkDestroyImage(device, image, nullptr);
		if (memory)
			vkFreeMemory(device, memory, nullptr);
	}
};

struct Mesh
{
	MeshData source;
	std::shared_ptr<Slice> smooth, flat;
};

struct UploadPage
{
	std::shared_ptr<Buffer> buffer;
	VkDeviceSize used = 0;
};

struct Upload
{
	std::shared_ptr<Buffer> buffer;
	VkDeviceSize offset = 0;
	void *pointer = nullptr;
};

struct Frame
{
	VkDevice device = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	std::vector<UploadPage> uploads;
	std::map<std::pair<TextureId, VkBuffer>, VkDescriptorSet> descriptors;

	~Frame()
	{
		if (framebuffer)
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		if (pool)
			vkDestroyDescriptorPool(device, pool, nullptr);
	}
};

static VkBlendFactor blend(BlendFactor f)
{
	static const VkBlendFactor factors[] = {VK_BLEND_FACTOR_ZERO,
											VK_BLEND_FACTOR_ONE,
											VK_BLEND_FACTOR_SRC_COLOR,
											VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
											VK_BLEND_FACTOR_DST_COLOR,
											VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
											VK_BLEND_FACTOR_SRC_ALPHA,
											VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
											VK_BLEND_FACTOR_DST_ALPHA,
											VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
											VK_BLEND_FACTOR_SRC_ALPHA_SATURATE};
	return factors[unsigned(f)];
}

static VkBlendOp blendOperation(BlendOp op)
{
	static const VkBlendOp ops[] = {VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT};
	return ops[unsigned(op)];
}

static VkCompareOp compareOperation(Compare c)
{
	// Compare is declared in the VK_COMPARE_OP_NEVER..VK_COMPARE_OP_ALWAYS order.
	static_assert(int(Compare::Never) == VK_COMPARE_OP_NEVER, "Compare::Never must map to VK_COMPARE_OP_NEVER");
	static_assert(int(Compare::Less) == VK_COMPARE_OP_LESS, "Compare::Less must map to VK_COMPARE_OP_LESS");
	static_assert(int(Compare::Equal) == VK_COMPARE_OP_EQUAL, "Compare::Equal must map to VK_COMPARE_OP_EQUAL");
	static_assert(int(Compare::LessEqual) == VK_COMPARE_OP_LESS_OR_EQUAL,
				  "Compare::LessEqual must map to VK_COMPARE_OP_LESS_OR_EQUAL");
	static_assert(int(Compare::Greater) == VK_COMPARE_OP_GREATER, "Compare::Greater must map to VK_COMPARE_OP_GREATER");
	static_assert(int(Compare::NotEqual) == VK_COMPARE_OP_NOT_EQUAL,
				  "Compare::NotEqual must map to VK_COMPARE_OP_NOT_EQUAL");
	static_assert(int(Compare::GreaterEqual) == VK_COMPARE_OP_GREATER_OR_EQUAL,
				  "Compare::GreaterEqual must map to VK_COMPARE_OP_GREATER_OR_EQUAL");
	static_assert(int(Compare::Always) == VK_COMPARE_OP_ALWAYS, "Compare::Always must map to VK_COMPARE_OP_ALWAYS");
	return static_cast<VkCompareOp>(c);
}

static VkColorComponentFlags colorComponents(std::uint8_t mask)
{
	// The portable mask uses the GL bit order, which is also the Vulkan one.
	static_assert(VK_COLOR_COMPONENT_R_BIT == 1, "color mask bit 0 must be red");
	static_assert(VK_COLOR_COMPONENT_G_BIT == 2, "color mask bit 1 must be green");
	static_assert(VK_COLOR_COMPONENT_B_BIT == 4, "color mask bit 2 must be blue");
	static_assert(VK_COLOR_COMPONENT_A_BIT == 8, "color mask bit 3 must be alpha");
	return VkColorComponentFlags(mask);
}

static VkCullModeFlags cullMode(Cull cull)
{
	return cull == Cull::None	 ? VkCullModeFlags(VK_CULL_MODE_NONE)
		   : cull == Cull::Front ? VkCullModeFlags(VK_CULL_MODE_FRONT_BIT)
		   : cull == Cull::Back	 ? VkCullModeFlags(VK_CULL_MODE_BACK_BIT)
								 : VkCullModeFlags(VK_CULL_MODE_FRONT_AND_BACK);
}

static VkFrontFace frontFace(bool frontCCW)
{
	// Vulkan's facing area (primsrast "Basic Polygon Rasterization") is
	// a = -1/2 * sum(x_i*y_{i+1} - x_{i+1}*y_i) in framebuffer coordinates. That
	// leading negation cancels the downward framebuffer Y axis, so a positive
	// area - VK_FRONT_FACE_COUNTER_CLOCKWISE - is the winding a viewer sees as
	// counter-clockwise, exactly like glFrontFace. The shader Y flip reproduces
	// the OpenGL image, therefore logical GL winding maps straight through; the
	// viewport height stays positive and the index order is never reversed.
	return frontCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

static VkPrimitiveTopology topology(Primitive p)
{
	return p == Primitive::Triangles ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		   : p == Primitive::Lines	 ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
									 : VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

static VkRect2D scissor(Rect r, int width, int height)
{
	const Rect top = topLeftRect(r, height);
	const std::int64_t left = clampRange(top.x, 0, width);
	const std::int64_t upper = clampRange(top.y, 0, height);
	const std::int64_t right = clampRange(std::int64_t(top.x) + top.width, 0, width);
	const std::int64_t lower = clampRange(std::int64_t(top.y) + top.height, 0, height);
	VkRect2D out{};
	out.offset = {std::int32_t(left), std::int32_t(upper)};
	out.extent = {std::uint32_t(right > left ? right - left : 0), std::uint32_t(lower > upper ? lower - upper : 0)};
	return out;
}
} // namespace VulkanDetail

struct VulkanReadback::Data
{
	std::shared_ptr<VulkanDetail::Buffer> buffer;
	std::uint64_t serial = 0;
	int width = 0, height = 0;
	bool bgra = false;
};

using namespace VulkanDetail;

struct VulkanDevice::Impl
{
	VulkanCreateInfo info;
	VkPhysicalDeviceMemoryProperties memory{};
	VkPhysicalDeviceProperties properties{};
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
	std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> pipelines;
	std::vector<std::shared_ptr<GeometryPage>> geometry;
	std::unordered_map<MeshId, std::shared_ptr<Mesh>> meshes;
	std::unordered_map<TextureId, std::shared_ptr<Texture>> textures;
	std::vector<std::unique_ptr<Frame>> frames;
	FrameLifetime lifetime;
	VulkanFrame current{};
	bool inPass = false;
	MeshId nextMesh = 1;
	TextureId nextTexture = 1;
	// Immediate geometry reuses one mesh/slice pair: its data lives in a frame
	// upload page, which outlives every frame, so no per-draw handle is minted.
	std::shared_ptr<Mesh> transientMesh;
	std::shared_ptr<Slice> transientSlice;
	MeshData flatScratch;

	explicit Impl(const VulkanCreateInfo &ci) : info(ci), lifetime(ci.framesInFlight)
	{
		if (!info.device || !info.physicalDevice)
			throw std::invalid_argument("Vulkan device handles are required");
		if (info.colorFormat != VK_FORMAT_R8G8B8A8_UNORM && info.colorFormat != VK_FORMAT_B8G8R8A8_UNORM)
			throw std::invalid_argument("Vulkan backend requires linear RGBA8/BGRA8, not sRGB");
		if (info.depthFormat != VK_FORMAT_D24_UNORM_S8_UINT && info.depthFormat != VK_FORMAT_X8_D24_UNORM_PACK32 &&
			info.depthFormat != VK_FORMAT_D32_SFLOAT)
			throw std::invalid_argument("explicit D24 or D32 depth format required; no silent D16 fallback");
		if (!info.geometryPageBytes || !info.uploadPageBytes || !info.maxDescriptorSetsPerFrame)
			throw std::invalid_argument("invalid Vulkan arena limits");
		vkGetPhysicalDeviceMemoryProperties(info.physicalDevice, &memory);
		vkGetPhysicalDeviceProperties(info.physicalDevice, &properties);
		VkFormatProperties depthProperties{};
		vkGetPhysicalDeviceFormatProperties(info.physicalDevice, info.depthFormat, &depthProperties);
		if (!(depthProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
			throw std::runtime_error("requested depth format unsupported");
		try
		{
			makeRenderPass();
			VkDescriptorSetLayoutBinding bindings[2]{};
			bindings[0].binding = 0;
			bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			bindings[0].descriptorCount = 1;
			bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			bindings[1].binding = 1;
			bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[1].descriptorCount = 1;
			bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
			layout.bindingCount = 2;
			layout.pBindings = bindings;
			check(vkCreateDescriptorSetLayout(info.device, &layout, nullptr, &descriptorLayout),
				  "create descriptor layout");
			VkPipelineLayoutCreateInfo pipeline{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
			pipeline.setLayoutCount = 1;
			pipeline.pSetLayouts = &descriptorLayout;
			check(vkCreatePipelineLayout(info.device, &pipeline, nullptr, &pipelineLayout), "create pipeline layout");
			vs = shader(info.vertexSpirv);
			fs = shader(info.fragmentSpirv);
			for (unsigned i = 0; i < info.framesInFlight; ++i)
			{
				std::unique_ptr<Frame> frame(new Frame());
				frame->device = info.device;
				VkDescriptorPoolSize sizes[] = {
					{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, info.maxDescriptorSetsPerFrame},
					{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, info.maxDescriptorSetsPerFrame}};
				VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
				pool.maxSets = info.maxDescriptorSetsPerFrame;
				pool.poolSizeCount = 2;
				pool.pPoolSizes = sizes;
				check(vkCreateDescriptorPool(info.device, &pool, nullptr, &frame->pool),
					  "create frame descriptor pool");
				frames.push_back(std::move(frame));
			}
			transientMesh = std::make_shared<Mesh>();
			transientSlice = std::make_shared<Slice>();
			TextureDesc white;
			white.width = white.height = 1;
			textures.emplace(0, makeTexture(white, {255, 255, 255, 255}));
		}
		catch (...)
		{
			cleanup();
			throw;
		}
	}

	~Impl()
	{
		cleanup();
	}

	void cleanup()
	{
		// Platform must have waited for the queue before destruction.
		for (auto &p : pipelines)
			vkDestroyPipeline(info.device, p.second, nullptr);
		pipelines.clear();
		if (vs)
			vkDestroyShaderModule(info.device, vs, nullptr);
		if (fs)
			vkDestroyShaderModule(info.device, fs, nullptr);
		vs = fs = VK_NULL_HANDLE;
		if (pipelineLayout)
			vkDestroyPipelineLayout(info.device, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
		if (descriptorLayout)
			vkDestroyDescriptorSetLayout(info.device, descriptorLayout, nullptr);
		descriptorLayout = VK_NULL_HANDLE;
		frames.clear();
		if (renderPass)
			vkDestroyRenderPass(info.device, renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
	}

	VkShaderModule shader(const std::vector<std::uint32_t> &code)
	{
		if (code.empty() || code[0] != 0x07230203)
			throw std::invalid_argument("valid SPIR-V modules are required");
		VkShaderModuleCreateInfo create{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		create.codeSize = code.size() * 4;
		create.pCode = code.data();
		VkShaderModule result = VK_NULL_HANDLE;
		check(vkCreateShaderModule(info.device, &create, nullptr, &result), "create shader module");
		return result;
	}

	void makeRenderPass()
	{
		VkAttachmentDescription attachments[2]{};
		attachments[0].format = info.colorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[1] = attachments[0];
		attachments[1].format = info.depthFormat;
		attachments[1].initialLayout = attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference depth{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color;
		subpass.pDepthStencilAttachment = &depth;
		// Consecutive frames share one depth image; these external dependencies
		// order every attachment access against all earlier queue submissions.
		const VkPipelineStageFlags stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
											VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		const VkAccessFlags reads = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		const VkAccessFlags writes =
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		VkSubpassDependency deps[2]{};
		deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		deps[0].dstSubpass = 0;
		deps[0].srcStageMask = deps[0].dstStageMask = stages;
		deps[0].srcAccessMask = writes;
		deps[0].dstAccessMask = reads | writes;
		deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		deps[1] = deps[0];
		deps[1].srcSubpass = 0;
		deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		VkRenderPassCreateInfo create{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
		create.attachmentCount = 2;
		create.pAttachments = attachments;
		create.subpassCount = 1;
		create.pSubpasses = &subpass;
		create.dependencyCount = 2;
		create.pDependencies = deps;
		check(vkCreateRenderPass(info.device, &create, nullptr, &renderPass), "create load/store render pass");
	}

	Frame &frame()
	{
		return *frames.at(lifetime.slot());
	}

	void requireFrame()
	{
		if (!lifetime.active())
			throw std::logic_error("Vulkan operation requires beginFrame");
	}

	void endPass()
	{
		if (inPass)
		{
			vkCmdEndRenderPass(current.commands);
			inPass = false;
		}
	}

	void beginPass()
	{
		requireFrame();
		if (inPass)
			return;
		VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		begin.renderPass = renderPass;
		begin.framebuffer = frame().framebuffer;
		begin.renderArea.extent = {std::uint32_t(current.width), std::uint32_t(current.height)};
		vkCmdBeginRenderPass(current.commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
		inPass = true;
	}

	Upload upload(VkDeviceSize bytes, VkDeviceSize alignment)
	{
		Frame &f = frame();
		for (UploadPage &p : f.uploads)
		{
			const VkDeviceSize offset = alignBytes(p.used, alignment);
			if (offset <= p.buffer->size && bytes <= p.buffer->size - offset)
			{
				p.used = offset + bytes;
				Upload out;
				out.buffer = p.buffer;
				out.offset = offset;
				out.pointer = static_cast<std::uint8_t *>(p.buffer->mapped) + offset;
				return out;
			}
		}
		const VkDeviceSize size = std::max<VkDeviceSize>(info.uploadPageBytes, alignBytes(bytes, alignment));
		if (size > UINT32_MAX)
			throw std::length_error("Vulkan upload page exceeds dynamic uniform offset range");
		auto buffer = makeBuffer(info.device, memory, size,
								 VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
									 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
								 true);
		UploadPage page;
		page.buffer = buffer;
		page.used = bytes;
		f.uploads.push_back(page);
		Upload out;
		out.buffer = buffer;
		out.offset = 0;
		out.pointer = buffer->mapped;
		return out;
	}

	std::shared_ptr<Slice> uploadMesh(const MeshData &mesh)
	{
		requireFrame();
		endPass();
		const VkDeviceSize vertexBytes = mesh.vertices.size() * sizeof(Vertex);
		const VkDeviceSize indexStart = alignBytes(vertexBytes, 4);
		const VkDeviceSize total = indexStart + mesh.indices.size() * 4;
		ByteRange range;
		std::shared_ptr<GeometryPage> page;
		for (auto &p : geometry)
			if (p->free.allocate(total, 16, range))
			{
				page = p;
				break;
			}
		if (!page)
		{
			auto b = makeBuffer(
				info.device, memory, std::max<VkDeviceSize>(info.geometryPageBytes, alignBytes(total, 16)),
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				false);
			page = std::make_shared<GeometryPage>(b);
			geometry.push_back(page);
			if (!page->free.allocate(total, 16, range))
				throw std::logic_error("fresh geometry arena allocation failed");
		}
		auto slice = std::make_shared<Slice>();
		slice->page = page;
		slice->buffer = page->buffer;
		slice->range = range;
		slice->indexOffset = indexStart;
		slice->count = std::uint32_t(mesh.indices.size());
		const Upload stage = upload(total, 4);
		std::memcpy(stage.pointer, mesh.vertices.data(), std::size_t(vertexBytes));
		std::memcpy(static_cast<std::uint8_t *>(stage.pointer) + indexStart, mesh.indices.data(),
					mesh.indices.size() * 4);
		VkBufferCopy copy{stage.offset, range.offset, total};
		vkCmdCopyBuffer(current.commands, stage.buffer->buffer, page->buffer->buffer, 1, &copy);
		VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
		barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = page->buffer->buffer;
		barrier.offset = range.offset;
		barrier.size = total;
		vkCmdPipelineBarrier(current.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0,
							 nullptr, 1, &barrier, 0, nullptr);
		return slice;
	}

	std::shared_ptr<Texture> makeTexture(const TextureDesc &desc, std::vector<std::uint8_t> pixels)
	{
		auto t = std::make_shared<Texture>();
		t->device = info.device;
		t->desc = desc;
		VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = VK_FORMAT_R8G8B8A8_UNORM;
		image.extent = {std::uint32_t(desc.width), std::uint32_t(desc.height), 1};
		image.mipLevels = image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		check(vkCreateImage(info.device, &image, nullptr, &t->image), "create texture");
		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(info.device, t->image, &requirements);
		VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		allocate.allocationSize = requirements.size;
		allocate.memoryTypeIndex = memoryType(memory, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		check(vkAllocateMemory(info.device, &allocate, nullptr, &t->memory), "allocate texture memory");
		check(vkBindImageMemory(info.device, t->image, t->memory, 0), "bind texture memory");
		VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		view.image = t->image;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = image.format;
		view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		check(vkCreateImageView(info.device, &view, nullptr, &t->view), "create texture view");
		VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		sampler.minFilter = desc.minFilter == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		sampler.magFilter = desc.magFilter == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		// Legacy clamp and its border term are evaluated by the shader; the
		// sampler only has to stop wrapping on those axes.
		sampler.addressModeU =
			desc.wrapS == Wrap::Repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV =
			desc.wrapT == Wrap::Repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.maxLod = 0;
		sampler.maxAnisotropy = 1;
		check(vkCreateSampler(info.device, &sampler, nullptr, &t->sampler), "create sampler");
		Texture::Update update;
		update.region = {0, 0, desc.width, desc.height};
		update.pixels = std::move(pixels);
		t->pending.push_back(std::move(update));
		return t;
	}

	void textureCopy(const std::shared_ptr<Texture> &t, Rect r, const std::vector<std::uint8_t> &pixels)
	{
		requireFrame();
		endPass();
		lifetime.retain(t);
		const Upload stage = upload(pixels.size(), 4);
		std::memcpy(stage.pointer, pixels.data(), pixels.size());
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.image = t->image;
		barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		barrier.oldLayout = t->initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcAccessMask = t->initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(current.commands,
							 t->initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		VkBufferImageCopy copy{};
		copy.bufferOffset = stage.offset;
		copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.imageOffset = {r.x, r.y, 0};
		copy.imageExtent = {std::uint32_t(r.width), std::uint32_t(r.height), 1};
		vkCmdCopyBufferToImage(current.commands, stage.buffer->buffer, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							   1, &copy);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(current.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
							 0, nullptr, 0, nullptr, 1, &barrier);
		t->initialized = true;
	}

	void ensureTexture(const std::shared_ptr<Texture> &t)
	{
		for (const Texture::Update &update : t->pending)
			textureCopy(t, update.region, update.pixels);
		t->pending.clear();
	}

	VkDescriptorSet descriptor(TextureId id, const std::shared_ptr<Texture> &texture, const Upload &constants)
	{
		Frame &f = frame();
		const std::pair<TextureId, VkBuffer> key(id, constants.buffer->buffer);
		const auto found = f.descriptors.find(key);
		if (found != f.descriptors.end())
			return found->second;
		VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocate.descriptorPool = f.pool;
		allocate.descriptorSetCount = 1;
		allocate.pSetLayouts = &descriptorLayout;
		VkDescriptorSet set = VK_NULL_HANDLE;
		check(vkAllocateDescriptorSets(info.device, &allocate, &set),
			  "allocate frame descriptor set (increase maxDescriptorSetsPerFrame on exhaustion)");
		VkDescriptorBufferInfo buffer{constants.buffer->buffer, 0, sizeof(Uniforms)};
		VkDescriptorImageInfo image{texture->sampler, texture->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		VkWriteDescriptorSet writes[2]{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = set;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writes[0].pBufferInfo = &buffer;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = set;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &image;
		vkUpdateDescriptorSets(info.device, 2, writes, 0, nullptr);
		f.descriptors.emplace(key, set);
		return set;
	}

	VkPipeline pipeline(const PipelineState &p, Primitive primitive)
	{
		if (primitive == Primitive::Lines && p.lineWidth != 1 && !info.wideLinesEnabled)
			throw std::runtime_error("wideLines was not enabled on this Vulkan device");
		const PipelineKey key = pipelineKey(p, primitive);
		const auto found = pipelines.find(key);
		if (found != pipelines.end())
			return found->second;
		VkPipelineShaderStageCreateInfo stages[2]{};
		stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vs;
		stages[0].pName = "main";
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fs;
		stages[1].pName = "main";
		VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
		// The packed normal is fetched as unsigned bytes and decoded in the
		// shader; binding it as SNORM would change its zero point.
		VkVertexInputAttributeDescription attributes[] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
														  {1, 0, VK_FORMAT_R32G32_SFLOAT, 12},
														  {2, 0, VK_FORMAT_R8G8B8A8_UNORM, 20},
														  {3, 0, VK_FORMAT_R8G8B8A8_UNORM, 24}};
		VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		vertex.vertexBindingDescriptionCount = 1;
		vertex.pVertexBindingDescriptions = &binding;
		vertex.vertexAttributeDescriptionCount = 4;
		vertex.pVertexAttributeDescriptions = attributes;
		VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
		assembly.topology = topology(primitive);
		VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		viewport.viewportCount = viewport.scissorCount = 1;
		VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
		raster.polygonMode = VK_POLYGON_MODE_FILL;
		raster.cullMode = cullMode(p.cull);
		raster.frontFace = frontFace(p.frontCCW);
		raster.depthBiasEnable = p.polygonOffset;
		// glPolygonOffset(factor, units): factor scales the depth slope, units
		// scales the format's minimum resolvable difference.
		raster.depthBiasConstantFactor = p.offsetUnits;
		raster.depthBiasSlopeFactor = p.offsetFactor;
		raster.lineWidth = primitive == Primitive::Lines ? p.lineWidth : 1.0f;
		VkPipelineMultisampleStateCreateInfo sample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
		sample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
		depth.depthTestEnable = p.depthTest;
		// GL does not update depth while the test is disabled; neither does this.
		depth.depthWriteEnable = p.depthTest && p.depthWrite;
		depth.depthCompareOp = compareOperation(p.depthCompare);
		depth.minDepthBounds = 0;
		depth.maxDepthBounds = 1;
		VkPipelineColorBlendAttachmentState blendState{};
		blendState.blendEnable = p.blend;
		blendState.srcColorBlendFactor = blend(p.srcRGB);
		blendState.dstColorBlendFactor = blend(p.dstRGB);
		blendState.srcAlphaBlendFactor = blend(p.srcAlpha);
		blendState.dstAlphaBlendFactor = blend(p.dstAlpha);
		blendState.colorBlendOp = blendOperation(p.rgbOp);
		blendState.alphaBlendOp = blendOperation(p.alphaOp);
		blendState.colorWriteMask = colorComponents(p.colorMask);
		VkPipelineColorBlendStateCreateInfo blendInfo{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
		blendInfo.attachmentCount = 1;
		blendInfo.pAttachments = &blendState;
		VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
		dynamic.dynamicStateCount = 2;
		dynamic.pDynamicStates = dynamicStates;
		VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
		create.stageCount = 2;
		create.pStages = stages;
		create.pVertexInputState = &vertex;
		create.pInputAssemblyState = &assembly;
		create.pViewportState = &viewport;
		create.pRasterizationState = &raster;
		create.pMultisampleState = &sample;
		create.pDepthStencilState = &depth;
		create.pColorBlendState = &blendInfo;
		create.pDynamicState = &dynamic;
		create.layout = pipelineLayout;
		create.renderPass = renderPass;
		VkPipeline result = VK_NULL_HANDLE;
		check(vkCreateGraphicsPipelines(info.device, VK_NULL_HANDLE, 1, &create, nullptr, &result),
			  "create legacy graphics pipeline");
		try
		{
			pipelines.emplace(key, result);
		}
		catch (...)
		{
			vkDestroyPipeline(info.device, result, nullptr);
			throw;
		}
		return result;
	}

	void drawSlice(const Mesh &mesh, const std::shared_ptr<Slice> &slice, const Draw &d)
	{
		auto texture = textures.at(d.texture);
		ensureTexture(texture);
		lifetime.retain(texture);
		Uniforms u = d.uniforms;
		applyClipConvention(u, ClipConvention::Vulkan);
		applyMeshUniforms(u, mesh.source);
		applyTextureUniforms(u, texture->desc);
		const Upload constants =
			upload(sizeof(u), std::max<VkDeviceSize>(16, properties.limits.minUniformBufferOffsetAlignment));
		std::memcpy(constants.pointer, &u, sizeof(u));
		const VkDescriptorSet set = descriptor(d.texture, texture, constants);
		const VkPipeline pipe = pipeline(d.pipeline, mesh.source.primitive);
		beginPass();
		vkCmdBindPipeline(current.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		const Rect r = topLeftRect(d.pipeline.viewport, current.height);
		VkViewport viewport{float(r.x), float(r.y), float(r.width), float(r.height), 0, 1};
		// Positive viewport height: the shader carries the Vulkan-only Y conversion.
		vkCmdSetViewport(current.commands, 0, 1, &viewport);
		const VkRect2D scissorRect = VulkanDetail::scissor(
			d.pipeline.scissor ? d.pipeline.scissorRect : Rect{0, 0, current.width, current.height}, current.width,
			current.height);
		vkCmdSetScissor(current.commands, 0, 1, &scissorRect);
		const std::uint32_t offset = std::uint32_t(constants.offset);
		vkCmdBindDescriptorSets(current.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &set, 1,
								&offset);
		const VkBuffer buffer = slice->buffer->buffer;
		const VkDeviceSize vertexOffset = slice->range.offset;
		vkCmdBindVertexBuffers(current.commands, 0, 1, &buffer, &vertexOffset);
		vkCmdBindIndexBuffer(current.commands, buffer, slice->range.offset + slice->indexOffset, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(current.commands, slice->count, 1, 0, 0, 0);
	}
};

VulkanDevice::VulkanDevice(const VulkanCreateInfo &info) : impl_(new Impl(info)) {}

VulkanDevice::~VulkanDevice() = default;

void VulkanDevice::beginFrame(const VulkanFrame &f)
{
	Impl &i = *impl_;
	if (!f.commands || !f.colorView || !f.depthView || f.width <= 0 || f.height <= 0)
		throw std::invalid_argument("invalid Vulkan frame target");
	i.lifetime.begin(f.slot, f.serial, f.completedSerial);
	i.current = f;
	Frame &frame = i.frame();
	check(vkResetDescriptorPool(i.info.device, frame.pool, 0), "reset retired descriptor pool");
	frame.descriptors.clear();
	for (UploadPage &page : frame.uploads)
		page.used = 0;
	if (frame.framebuffer)
	{
		vkDestroyFramebuffer(i.info.device, frame.framebuffer, nullptr);
		frame.framebuffer = VK_NULL_HANDLE;
	}
	VkImageView attachments[] = {f.colorView, f.depthView};
	VkFramebufferCreateInfo create{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	create.renderPass = i.renderPass;
	create.attachmentCount = 2;
	create.pAttachments = attachments;
	create.width = std::uint32_t(f.width);
	create.height = std::uint32_t(f.height);
	create.layers = 1;
	check(vkCreateFramebuffer(i.info.device, &create, nullptr, &frame.framebuffer), "create frame framebuffer");
}

void VulkanDevice::endFrame()
{
	Impl &i = *impl_;
	i.requireFrame();
	i.endPass();
	for (UploadPage &page : i.frame().uploads)
		if (page.used)
			page.buffer->flush();
	i.lifetime.end();
}

void VulkanDevice::discardTargets()
{
	Impl &i = *impl_;
	if (i.lifetime.active())
		throw std::logic_error("frame targets cannot be discarded while a frame is recording");
	for (auto &frame : i.frames)
		if (frame->framebuffer)
		{
			vkDestroyFramebuffer(i.info.device, frame->framebuffer, nullptr);
			frame->framebuffer = VK_NULL_HANDLE;
		}
}

void VulkanDevice::retainFrameResource(const std::shared_ptr<void> &resource)
{
	if (!resource)
		throw std::invalid_argument("no resource to retain");
	impl_->lifetime.retain(resource);
}

void VulkanDevice::beginOcclusionQuery(VkQueryPool pool, std::uint32_t index)
{
	Impl &i = *impl_;
	i.requireFrame();
	if (!pool)
		throw std::invalid_argument("occlusion query pool required");
	// vkCmdResetQueryPool is illegal inside a render pass instance, and a query
	// begun outside one keeps counting across the passes the draws open.
	i.endPass();
	vkCmdResetQueryPool(i.current.commands, pool, index, 1);
	vkCmdBeginQuery(i.current.commands, pool, index, 0);
}

void VulkanDevice::endOcclusionQuery(VkQueryPool pool, std::uint32_t index)
{
	Impl &i = *impl_;
	i.requireFrame();
	if (!pool)
		throw std::invalid_argument("occlusion query pool required");
	i.endPass();
	vkCmdEndQuery(i.current.commands, pool, index);
}

MeshId VulkanDevice::createMesh(const MeshData &source)
{
	validateMesh(source);
	auto mesh = std::make_shared<Mesh>();
	mesh->source = source;
	Impl &i = *impl_;
	const MeshId id = i.nextMesh++;
	i.meshes.emplace(id, std::move(mesh));
	return id;
}

void VulkanDevice::destroyMesh(MeshId id)
{
	if (!impl_->meshes.erase(id))
		throw std::out_of_range("unknown Vulkan mesh");
}

TextureId VulkanDevice::createTexture(const TextureDesc &desc, const void *rgba, std::size_t bytes, std::size_t stride)
{
	validateTexture(desc, false);
	std::vector<std::uint8_t> data = unpackRGBA(rgba, bytes, desc.width, desc.height, stride);
	Impl &i = *impl_;
	auto texture = i.makeTexture(desc, std::move(data));
	const TextureId id = i.nextTexture++;
	i.textures.emplace(id, std::move(texture));
	return id;
}

void VulkanDevice::updateTexture(TextureId id, Rect r, const void *rgba, std::size_t bytes, std::size_t stride)
{
	Impl &i = *impl_;
	auto t = i.textures.at(id);
	if (!id || r.x < 0 || r.y < 0 || std::int64_t(r.x) + r.width > t->desc.width ||
		std::int64_t(r.y) + r.height > t->desc.height)
		throw std::out_of_range("texture subimage rectangle");
	std::vector<std::uint8_t> data = unpackRGBA(rgba, bytes, r.width, r.height, stride);
	if (i.lifetime.active())
	{
		i.ensureTexture(t);
		i.textureCopy(t, r, data);
	}
	else
	{
		Texture::Update update;
		update.region = r;
		update.pixels = std::move(data);
		t->pending.push_back(std::move(update));
	}
}

void VulkanDevice::destroyTexture(TextureId id)
{
	if (!id)
		throw std::invalid_argument("cannot destroy internal white texture");
	if (!impl_->textures.erase(id))
		throw std::out_of_range("unknown Vulkan texture");
}

void VulkanDevice::draw(const Draw &d)
{
	Impl &i = *impl_;
	i.requireFrame();
	validatePipeline(d.pipeline);
	if (!d.pipeline.viewport.width || !d.pipeline.viewport.height)
		return;
	auto mesh = i.meshes.at(d.mesh);
	std::shared_ptr<Slice> &slice = d.pipeline.flat ? mesh->flat : mesh->smooth;
	if (!slice)
	{
		if (d.pipeline.flat)
		{
			flatMesh(mesh->source, i.flatScratch);
			slice = i.uploadMesh(i.flatScratch);
		}
		else
		{
			slice = i.uploadMesh(mesh->source);
		}
	}
	i.lifetime.retain(mesh);
	i.drawSlice(*mesh, slice, d);
}

void VulkanDevice::drawTransient(const MeshData &input, const Draw &state)
{
	Impl &i = *impl_;
	i.requireFrame();
	validateMesh(input);
	validatePipeline(state.pipeline);
	if (!state.pipeline.viewport.width || !state.pipeline.viewport.height)
		return;
	const MeshData *source = &input;
	if (state.pipeline.flat)
	{
		flatMesh(input, i.flatScratch);
		source = &i.flatScratch;
	}
	const VkDeviceSize vertexBytes = source->vertices.size() * sizeof(Vertex);
	const VkDeviceSize indexOffset = alignBytes(vertexBytes, 4);
	const VkDeviceSize total = indexOffset + source->indices.size() * 4;
	const Upload staged = i.upload(total, 16);
	std::memcpy(staged.pointer, source->vertices.data(), std::size_t(vertexBytes));
	std::memcpy(static_cast<std::uint8_t *>(staged.pointer) + indexOffset, source->indices.data(),
				source->indices.size() * 4);
	// Immediate geometry is drawn straight out of the mapped upload page. The
	// page belongs to the frame slot and outlives every frame that reads it, so
	// the reused slice owns no arena range and needs no per-draw handle.
	Slice &slice = *i.transientSlice;
	slice.buffer = staged.buffer;
	slice.range = {staged.offset, total};
	slice.indexOffset = indexOffset;
	slice.count = std::uint32_t(source->indices.size());
	Mesh &mesh = *i.transientMesh;
	mesh.source.primitive = source->primitive;
	mesh.source.hasTexture = source->hasTexture;
	mesh.source.hasColor = source->hasColor;
	mesh.source.hasNormal = source->hasNormal;
	Draw d = state;
	d.pipeline.flat = false; // The flat variant is baked into the vertices above.
	i.drawSlice(mesh, i.transientSlice, d);
}

VulkanReadback VulkanDevice::recordReadback(VkImage image)
{
	Impl &i = *impl_;
	i.requireFrame();
	if (!image)
		throw std::invalid_argument("readback image required");
	i.endPass();
	auto result = std::make_shared<VulkanReadback::Data>();
	result->serial = i.lifetime.serial();
	result->width = i.current.width;
	result->height = i.current.height;
	result->bgra = i.info.colorFormat == VK_FORMAT_B8G8R8A8_UNORM;
	result->buffer = makeBuffer(i.info.device, i.memory, rgbaByteCount(result->width, result->height),
								VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.image = image;
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(i.current.commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	VkBufferImageCopy copy{};
	copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	copy.imageExtent = {std::uint32_t(result->width), std::uint32_t(result->height), 1};
	vkCmdCopyImageToBuffer(i.current.commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, result->buffer->buffer, 1,
						   &copy);
	VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
	host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	host.buffer = result->buffer->buffer;
	host.size = VK_WHOLE_SIZE;
	vkCmdPipelineBarrier(i.current.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr,
						 1, &host, 0, nullptr);
	// The target keeps its attachment layout: readback never ends the frame.
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	vkCmdPipelineBarrier(i.current.commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	i.lifetime.retain(result);
	VulkanReadback ticket;
	ticket.data = result;
	return ticket;
}

std::vector<std::uint8_t> VulkanDevice::resolveReadback(const VulkanReadback &ticket, std::uint64_t completed)
{
	if (!ticket.data || completed < ticket.data->serial)
		throw std::logic_error("readback fence has not completed");
	const VulkanReadback::Data &t = *ticket.data;
	if (t.buffer->device != impl_->info.device)
		throw std::invalid_argument("readback belongs to another device");
	t.buffer->invalidate();
	return readbackRGBA(t.buffer->mapped, std::size_t(t.buffer->size), t.width, t.height, std::size_t(t.width) * 4,
						true, t.bgra);
}
} // namespace render
} // namespace b173
