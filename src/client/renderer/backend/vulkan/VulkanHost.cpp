#include "client/renderer/backend/BackendHost.h"
#include "client/renderer/backend/vulkan/VulkanDevice.h"

#include <vulkan/vulkan.h>

#include "SDL.h"
#include "SDL_vulkan.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Main's build generates the shared shader source, compiles it to SPIR-V and
// stages it beside the executable like every other runtime resource.
#ifndef B173_VULKAN_SHADER_DIR
#error "B173_VULKAN_SHADER_DIR must name the staged directory holding legacy.vert.spv and legacy.frag.spv"
#endif

namespace b173
{
namespace render
{
namespace VulkanHostDetail
{
// GL error codes: the semantic boundary reports them through glGetError.
const unsigned NO_ERROR_CODE = 0, INVALID_OPERATION = 0x0502, OUT_OF_MEMORY = 0x0505;
const std::uint32_t QUERIES_PER_POOL = 512;
const unsigned FRAMES_IN_FLIGHT = 3;

struct QueryPool
{
	VkDevice device = VK_NULL_HANDLE;
	VkQueryPool pool = VK_NULL_HANDLE;

	~QueryPool()
	{
		if (pool)
			vkDestroyQueryPool(device, pool, nullptr);
	}
};

static std::vector<std::uint32_t> readSpirv(const std::string &path)
{
	std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("cannot open the generated SPIR-V module " + path);
	const std::streamoff bytes = file.tellg();
	if (bytes <= 0 || bytes % 4)
		throw std::runtime_error("generated SPIR-V module has an invalid size: " + path);
	std::vector<std::uint32_t> code(std::size_t(bytes) / 4);
	file.seekg(0, std::ios::beg);
	if (!file.read(reinterpret_cast<char *>(code.data()), bytes))
		throw std::runtime_error("cannot read the generated SPIR-V module " + path);
	if (code[0] != 0x07230203u)
		throw std::runtime_error("not a SPIR-V module: " + path);
	return code;
}

// Resolved next to the executable, never as a baked build-machine path.
static std::string shaderPath(const char *name)
{
	std::string directory;
	if (char *base = SDL_GetBasePath())
	{
		directory = base;
		SDL_free(base);
	}
	directory += B173_VULKAN_SHADER_DIR;
	directory += "/";
	return directory + name;
}

static bool hasLayer(const char *name)
{
	std::uint32_t count = 0;
	if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS || !count)
		return false;
	std::vector<VkLayerProperties> layers(count);
	if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS)
		return false;
	for (const VkLayerProperties &layer : layers)
		if (!std::strcmp(layer.layerName, name))
			return true;
	return false;
}

static bool hasInstanceExtension(const char *name)
{
	std::uint32_t count = 0;
	if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS || !count)
		return false;
	std::vector<VkExtensionProperties> extensions(count);
	if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS)
		return false;
	for (const VkExtensionProperties &extension : extensions)
		if (!std::strcmp(extension.extensionName, name))
			return true;
	return false;
}

static bool hasDeviceExtension(VkPhysicalDevice device, const char *name)
{
	std::uint32_t count = 0;
	if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS || !count)
		return false;
	std::vector<VkExtensionProperties> extensions(count);
	if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) != VK_SUCCESS)
		return false;
	for (const VkExtensionProperties &extension : extensions)
		if (!std::strcmp(extension.extensionName, name))
			return true;
	return false;
}

static VkFormat depthAttachmentFormat(VkPhysicalDevice device)
{
	// D24 first, then the valid 24/32-bit fallbacks. Never a silent D16.
	const VkFormat wanted[] = {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT};
	for (VkFormat format : wanted)
	{
		VkFormatProperties properties{};
		vkGetPhysicalDeviceFormatProperties(device, format, &properties);
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			return format;
	}
	return VK_FORMAT_UNDEFINED;
}

static std::uint32_t memoryType(const VkPhysicalDeviceMemoryProperties &properties, std::uint32_t mask,
								VkMemoryPropertyFlags required)
{
	for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
		if ((mask & (1u << i)) && (properties.memoryTypes[i].propertyFlags & required) == required)
			return i;
	throw std::runtime_error("required Vulkan memory type is unavailable");
}

static std::uint32_t clampExtent(int value, std::uint32_t low, std::uint32_t high)
{
	const std::uint32_t v = value < 0 ? 0u : std::uint32_t(value);
	return v < low ? low : (v > high ? high : v);
}

static const char *deviceKind(VkPhysicalDeviceType type)
{
	return type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU		? "discrete GPU"
		   : type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated GPU"
		   : type == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU	? "virtual GPU"
		   : type == VK_PHYSICAL_DEVICE_TYPE_CPU			? "software"
															: "other";
}

static std::string hex(std::uint32_t value)
{
	static const char digits[] = "0123456789ABCDEF";
	std::string out = "0x";
	for (int shift = 28; shift >= 0; shift -= 4)
		out.push_back(digits[(value >> shift) & 15]);
	return out;
}
} // namespace VulkanHostDetail

using namespace VulkanHostDetail;

// Owns the instance, device, surface, swapchain, submission and presentation.
// The renderer below only records into the command buffer it is handed.
class VulkanHost final : public BackendHost
{
	struct Image
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkSemaphore rendered = VK_NULL_HANDLE;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		bool initialized = false;
	};

	struct Slot
	{
		VkCommandPool pool = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> buffers;
		std::size_t used = 0;
		VkFence fence = VK_NULL_HANDLE;
		VkSemaphore acquire = VK_NULL_HANDLE;
		std::uint64_t serial = 0;
		bool pending = false;
	};

	struct Query
	{
		std::vector<std::uint32_t> segments; // One per submission the query spans.
		std::uint64_t serial = 0;
		bool active = false, resolved = false;
		std::uint32_t value = 0;
	};

	struct FreeQuery
	{
		std::uint32_t index = 0;
		std::uint64_t serial = 0; // Reusable once this submission completed.
	};

	SDL_Window *window_ = nullptr;
	bool loaded_ = false, validationRequested_ = false, validationEnabled_ = false;
	VkInstance instance_ = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
	PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger_ = nullptr;
	VkSurfaceKHR surface_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_ = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties properties_{};
	VkPhysicalDeviceMemoryProperties memoryProperties_{};
	std::uint32_t graphicsFamily_ = 0, presentFamily_ = 0;
	VkDevice device_ = VK_NULL_HANDLE;
	VkQueue graphicsQueue_ = VK_NULL_HANDLE, presentQueue_ = VK_NULL_HANDLE;
	VkFormat colorFormat_ = VK_FORMAT_UNDEFINED, depthFormat_ = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR colorSpace_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkImageAspectFlags depthAspect_ = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkExtent2D extent_{0, 0};
	// Drawable size observed when the swapchain was built. Comparing against it
	// reacts to real window changes without rebuilding every frame when the
	// surface reports a clamped extent.
	int createdWidth_ = 0, createdHeight_ = 0;
	std::vector<Image> images_;
	VkImage depthImage_ = VK_NULL_HANDLE;
	VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
	VkImageView depthView_ = VK_NULL_HANDLE;
	bool depthInitialized_ = false;
	std::vector<Slot> slots_;
	unsigned frameIndex_ = 0;
	std::uint32_t imageIndex_ = 0;
	std::uint64_t nextSerial_ = 1, completed_ = 0;
	bool recording_ = false, acquireWaited_ = false, needRecreate_ = false;
	VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
	VkCompositeAlphaFlagBitsKHR compositeAlpha_ = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkCommandBuffer commands_ = VK_NULL_HANDLE;
	std::vector<std::shared_ptr<QueryPool>> queryPools_;
	std::vector<FreeQuery> freeQuerySlots_;
	std::unordered_map<std::uint32_t, Query> queries_;
	std::uint32_t nextQueryId_ = 1, activeQuery_ = 0;
	std::unique_ptr<VulkanDevice> renderer_;
	std::string description_;
	std::atomic<unsigned> errorCode_{NO_ERROR_CODE};
	MeshData clearMesh_;
	float clearMeshDepth_ = -1;

  public:
	explicit VulkanHost(SDL_Window *window) : window_(window)
	{
		try
		{
			create();
		}
		catch (...)
		{
			destroy();
			throw;
		}
	}

	~VulkanHost() override
	{
		destroy();
	}

	Device &device() override
	{
		return *renderer_;
	}

	bool beginFrame() override
	{
		if (recording_)
			throw std::logic_error("beginFrame called while a frame is still recording");
		int width = 0, height = 0;
		drawableSize(&width, &height);
		// Minimized or zero-size: never submit or present an empty target.
		if (width <= 0 || height <= 0)
			return false;
		if (needRecreate_ || width != createdWidth_ || height != createdHeight_)
			if (!recreate())
				return false;
		Slot &slot = currentSlot();
		// The only wait a normal frame performs: this slot's own last submission.
		waitSlot(slot);
		completed_ = completedSerial();
		bool acquired = false;
		for (unsigned attempt = 0; attempt < 2 && !acquired; ++attempt)
		{
			const VkResult result =
				vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, slot.acquire, VK_NULL_HANDLE, &imageIndex_);
			if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
			{
				needRecreate_ = needRecreate_ || result == VK_SUBOPTIMAL_KHR;
				acquired = true;
				break;
			}
			if (result != VK_ERROR_OUT_OF_DATE_KHR)
				require(result, "acquire swapchain image");
			if (!recreate())
				return false;
		}
		if (!acquired)
			return false;
		// recording_ marks both the acquired image and the open device frame.
		acquireWaited_ = false;
		slot.serial = nextSerial_++;
		// A new frame restarts this slot's command buffers from the first one.
		slot.used = 0;
		beginCommands(slot);
		prepareAttachments();
		beginDeviceFrame(slot);
		recording_ = true;
		return true;
	}

	void present() override
	{
		// Nothing was acquired this frame, so there is nothing to show.
		if (!recording_)
			return;
		if (activeQuery_)
			throw std::logic_error("an occlusion query is still active at present");
		if (compositeAlpha_ != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		{
			// Some surfaces require composited alpha. The engine's RGB target
			// remains opaque without changing its rendered RGB channels.
			PipelineState alpha;
			alpha.colorMask = 8;
			alpha.depthWrite = false;
			clear({0, 0, 0, 1}, 1, true, false, alpha);
		}
		renderer_->endFrame();
		presentBarrier();
		submit(true);
		recording_ = false;
		VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &images_[imageIndex_].rendered;
		info.swapchainCount = 1;
		info.pSwapchains = &swapchain_;
		info.pImageIndices = &imageIndex_;
		// Asynchronous by design: the frame is never waited on here.
		const VkResult result = vkQueuePresentKHR(presentQueue_, &info);
		frameIndex_ = (frameIndex_ + 1) % unsigned(slots_.size());
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			needRecreate_ = true;
		else
			require(result, "present swapchain image");
	}

	void finish() override
	{
		// Submits and waits, but keeps the acquired image and resumes recording.
		if (recording_)
			splitSubmission();
		else
			waitAllPending();
	}

	void clear(Vec4 color, float depth, bool colorBit, bool depthBit, const PipelineState &state) override
	{
		if (!recording_ || (!colorBit && !depthBit))
			return;
		// A masked or scissored legacy clear is a draw, not vkCmdClearAttachments.
		if (clearMesh_.vertices.empty() || clearMeshDepth_ != depth)
		{
			clearMesh_ = clearTriangle(depth);
			clearMeshDepth_ = depth;
		}
		// drawTransient supplies the geometry; the draw's mesh handle is unused.
		const Draw draw = clearDraw(0, int(extent_.width), int(extent_.height), color, colorBit, depthBit, state);
		try
		{
			renderer_->drawTransient(clearMesh_, draw);
		}
		catch (...)
		{
			errorCode_ = INVALID_OPERATION;
			throw;
		}
	}

	std::vector<std::uint8_t> readPixels(Rect region) override
	{
		if (region.x < 0 || region.y < 0 || std::int64_t(region.x) + region.width > std::int64_t(extent_.width) ||
			std::int64_t(region.y) + region.height > std::int64_t(extent_.height))
			throw std::out_of_range("readback rectangle");
		const std::size_t bytes = rgbaByteCount(region.width, region.height);
		// No acquired image means no drawable surface and therefore no pixels.
		if (!recording_)
			return std::vector<std::uint8_t>(bytes, 0);
		std::vector<std::uint8_t> full;
		try
		{
			const VulkanReadback ticket = renderer_->recordReadback(images_[imageIndex_].image);
			// Submit and wait without presenting; recording resumes on the same
			// color/depth target with a higher serial.
			splitSubmission();
			full = renderer_->resolveReadback(ticket, completed_);
		}
		catch (...)
		{
			errorCode_ = INVALID_OPERATION;
			throw;
		}
		if (!region.x && !region.y && std::uint32_t(region.width) == extent_.width &&
			std::uint32_t(region.height) == extent_.height)
			return full;
		// Both buffers hold tightly packed RGBA rows in bottom-left order.
		std::vector<std::uint8_t> out(bytes);
		const std::size_t sourceRow = std::size_t(extent_.width) * 4, row = std::size_t(region.width) * 4;
		for (int y = 0; y < region.height; ++y)
			std::memcpy(out.data() + std::size_t(y) * row,
						full.data() + std::size_t(region.y + y) * sourceRow + std::size_t(region.x) * 4, row);
		return out;
	}

	void drawableSize(int *width, int *height) const override
	{
		if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED)
		{
			*width = *height = 0;
			return;
		}
		SDL_Vulkan_GetDrawableSize(window_, width, height);
	}

	std::string description() const override
	{
		return description_;
	}

	// Wide lines are expanded into triangles above this boundary, so the native
	// pipeline only ever sees the one width Vulkan guarantees without wideLines.
	float maximumLineWidth() const override
	{
		return 1;
	}

	bool supportsOcclusion() const override
	{
		return true;
	}

	void genQueries(int count, std::uint32_t *ids) override
	{
		if (count < 0 || (count && !ids))
			throw std::invalid_argument("occlusion query allocation");
		for (int i = 0; i < count; ++i)
		{
			const std::uint32_t id = nextQueryId_++;
			queries_.emplace(id, Query());
			ids[i] = id;
		}
	}

	void deleteQueries(int count, const std::uint32_t *ids) override
	{
		if (count < 0 || (count && !ids))
			throw std::invalid_argument("occlusion query deletion");
		for (int i = 0; i < count; ++i)
		{
			const auto it = queries_.find(ids[i]);
			if (it == queries_.end())
				continue;
			if (it->first == activeQuery_)
			{
				// GL ends an active query when its name is deleted.
				endQuerySegment(it->second);
				it->second.serial = recordedSerial();
				activeQuery_ = 0;
			}
			releaseSegments(it->second);
			queries_.erase(it);
		}
	}

	void beginQuery(std::uint32_t id) override
	{
		if (!recording_)
			throw std::logic_error("occlusion query outside a frame");
		if (activeQuery_)
			throw std::logic_error("occlusion queries cannot nest");
		const auto it = queries_.find(id);
		if (it == queries_.end())
			throw std::out_of_range("unknown occlusion query");
		Query &query = it->second;
		// Beginning a query discards its previous result, as GL does.
		releaseSegments(query);
		query.resolved = false;
		query.value = 0;
		query.serial = 0;
		beginQuerySegment(query);
		query.active = true;
		activeQuery_ = id;
	}

	void endQuery() override
	{
		if (!activeQuery_)
			throw std::logic_error("no active occlusion query");
		Query &query = queries_.at(activeQuery_);
		endQuerySegment(query);
		query.serial = recordedSerial();
		query.active = false;
		activeQuery_ = 0;
	}

	std::uint32_t queryResult(std::uint32_t id, bool availability) override
	{
		const auto it = queries_.find(id);
		if (it == queries_.end())
			throw std::out_of_range("unknown occlusion query");
		Query &query = it->second;
		if (query.resolved)
			return availability ? 1u : query.value;
		// An unfinished query has no result and nothing to wait for yet.
		if (query.active || query.segments.empty())
			return 0;
		if (completedSerial() < query.serial)
		{
			if (availability)
				return 0;
			// A blocking result read flushes the recording frame like GL does.
			if (recording_ && currentSlot().serial == query.serial)
				splitSubmission();
			waitSerial(query.serial);
		}
		const VkQueryResultFlags flags =
			VK_QUERY_RESULT_64_BIT |
			(availability ? VkQueryResultFlags(0) : VkQueryResultFlags(VK_QUERY_RESULT_WAIT_BIT));
		std::uint64_t total = 0;
		for (std::uint32_t index : query.segments)
		{
			std::uint64_t value = 0;
			const VkResult result =
				vkGetQueryPoolResults(device_, queryPools_[index / QUERIES_PER_POOL]->pool, index % QUERIES_PER_POOL, 1,
									  sizeof(value), &value, sizeof(value), flags);
			if (result == VK_NOT_READY)
				return 0;
			require(result, "read occlusion query result");
			// Samples of every segment the query spans add up.
			total += value;
		}
		query.value = total > 0xFFFFFFFFull ? 0xFFFFFFFFu : std::uint32_t(total);
		query.resolved = true;
		releaseSegments(query);
		return availability ? 1u : query.value;
	}

	unsigned error() override
	{
		return errorCode_.exchange(NO_ERROR_CODE, std::memory_order_relaxed);
	}

  private:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
														VkDebugUtilsMessageTypeFlagsEXT,
														const VkDebugUtilsMessengerCallbackDataEXT *data, void *user)
	{
		const char *message = data && data->pMessage ? data->pMessage : "(no message)";
		if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Vulkan validation error: %s", message);
			if (VulkanHost *host = static_cast<VulkanHost *>(user))
				host->errorCode_ = INVALID_OPERATION;
		}
		else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Vulkan validation warning: %s", message);
		}
		return VK_FALSE;
	}

	void require(VkResult result, const char *where)
	{
		if (result == VK_SUCCESS)
			return;
		errorCode_ = result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY ||
							 result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL
						 ? OUT_OF_MEMORY
						 : INVALID_OPERATION;
		throw std::runtime_error(std::string(where) + ": VkResult " + std::to_string(int(result)));
	}

	Slot &currentSlot()
	{
		return slots_[frameIndex_ % slots_.size()];
	}

	// Serial of the submission the commands recorded right now will belong to.
	std::uint64_t recordedSerial() const
	{
		return nextSerial_ - 1;
	}

	void create()
	{
		if (!window_)
			throw std::invalid_argument("a Vulkan window is required");
		if (!(SDL_GetWindowFlags(window_) & SDL_WINDOW_VULKAN))
			throw std::invalid_argument("the window was not created with SDL_WINDOW_VULKAN");
		const char *validation = std::getenv("B173_RENDER_VALIDATION");
		validationRequested_ = validation && !std::strcmp(validation, "1");
		if (SDL_Vulkan_LoadLibrary(nullptr) != 0)
			throw std::runtime_error(std::string("SDL_Vulkan_LoadLibrary: ") + SDL_GetError());
		loaded_ = true;
		createInstance();
		createMessenger();
		if (!SDL_Vulkan_CreateSurface(window_, instance_, &surface_))
			throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface: ") + SDL_GetError());
		selectPhysicalDevice();
		createDevice();
		createSlots();
		if (!createSwapchain())
			throw std::runtime_error("the Vulkan surface has no drawable area at startup");
		VulkanCreateInfo info;
		info.physicalDevice = physical_;
		info.device = device_;
		info.colorFormat = colorFormat_;
		info.depthFormat = depthFormat_;
		info.framesInFlight = unsigned(slots_.size());
		info.wideLinesEnabled = false;
		info.vertexSpirv = readSpirv(shaderPath("legacy.vert.spv"));
		info.fragmentSpirv = readSpirv(shaderPath("legacy.frag.spv"));
		renderer_.reset(new VulkanDevice(info));
		description_ = describe();
	}

	void createInstance()
	{
		unsigned count = 0;
		if (!SDL_Vulkan_GetInstanceExtensions(window_, &count, nullptr))
			throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions: ") + SDL_GetError());
		std::vector<const char *> extensions(count ? count : 1, nullptr);
		if (count && !SDL_Vulkan_GetInstanceExtensions(window_, &count, extensions.data()))
			throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions: ") + SDL_GetError());
		extensions.resize(count);
		const char *layers[] = {"VK_LAYER_KHRONOS_validation"};
		if (validationRequested_)
		{
			validationEnabled_ = hasLayer(layers[0]) && hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			if (validationEnabled_)
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			else
				SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
							"B173_RENDER_VALIDATION=1 but %s with %s is not installed; "
							"running without Vulkan validation",
							layers[0], VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
		application.pApplicationName = "McBetaCpp";
		application.applicationVersion = 1;
		application.pEngineName = "b173 portable renderer";
		application.engineVersion = 1;
		// The device below uses only Vulkan 1.0 render passes and descriptors.
		application.apiVersion = VK_API_VERSION_1_0;
		VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
		create.pApplicationInfo = &application;
		create.enabledExtensionCount = std::uint32_t(extensions.size());
		create.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
		create.enabledLayerCount = validationEnabled_ ? 1 : 0;
		create.ppEnabledLayerNames = validationEnabled_ ? layers : nullptr;
		require(vkCreateInstance(&create, nullptr, &instance_), "create Vulkan instance");
	}

	void createMessenger()
	{
		if (!validationEnabled_)
			return;
		const auto createMessengerFunction = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
		destroyMessenger_ = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
		if (!createMessengerFunction || !destroyMessenger_)
			throw std::runtime_error("VK_EXT_debug_utils is enabled but its entry points are missing");
		VkDebugUtilsMessengerCreateInfoEXT create{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
		create.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
							 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
							 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create.pfnUserCallback = &VulkanHost::debugCallback;
		create.pUserData = this;
		require(createMessengerFunction(instance_, &create, nullptr, &messenger_), "create debug messenger");
	}

	bool queueFamilies(VkPhysicalDevice device, std::uint32_t &graphics, std::uint32_t &present) const
	{
		std::uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
		if (!count)
			return false;
		std::vector<VkQueueFamilyProperties> families(count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
		bool haveGraphics = false, havePresent = false;
		for (std::uint32_t i = 0; i < count; ++i)
		{
			const bool graphicsCapable =
				families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			VkBool32 presentCapable = VK_FALSE;
			if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentCapable) != VK_SUCCESS)
				presentCapable = VK_FALSE;
			// One family for both keeps swapchain images free of ownership transfers.
			if (graphicsCapable && presentCapable)
			{
				graphics = present = i;
				return true;
			}
			if (graphicsCapable && !haveGraphics)
			{
				graphics = i;
				haveGraphics = true;
			}
			if (presentCapable && !havePresent)
			{
				present = i;
				havePresent = true;
			}
		}
		return haveGraphics && havePresent;
	}

	VkFormat surfaceFormat(VkPhysicalDevice device, VkColorSpaceKHR *space) const
	{
		std::uint32_t count = 0;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, nullptr) != VK_SUCCESS || !count)
			return VK_FORMAT_UNDEFINED;
		std::vector<VkSurfaceFormatKHR> formats(count);
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, formats.data()) != VK_SUCCESS)
			return VK_FORMAT_UNDEFINED;
		// One undefined entry means the surface accepts any format.
		if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		{
			if (space)
				*space = formats[0].colorSpace;
			return VK_FORMAT_B8G8R8A8_UNORM;
		}
		// UNORM only: an sRGB swapchain would re-encode every written pixel.
		const VkFormat wanted[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
		for (VkFormat format : wanted)
			for (const VkSurfaceFormatKHR &available : formats)
				if (available.format == format)
				{
					if (space)
						*space = available.colorSpace;
					return format;
				}
		return VK_FORMAT_UNDEFINED;
	}

	bool presentSupported(VkPhysicalDevice device) const
	{
		std::uint32_t count = 0;
		return vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, nullptr) == VK_SUCCESS && count;
	}

	void selectPhysicalDevice()
	{
		std::uint32_t count = 0;
		require(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "enumerate Vulkan devices");
		if (!count)
			throw std::runtime_error("no Vulkan physical device is available");
		std::vector<VkPhysicalDevice> devices(count);
		require(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "enumerate Vulkan devices");
		unsigned best = 0;
		for (VkPhysicalDevice candidate : devices)
		{
			std::uint32_t graphicsIndex = 0, presentIndex = 0;
			if (!queueFamilies(candidate, graphicsIndex, presentIndex) ||
				!hasDeviceExtension(candidate, VK_KHR_SWAPCHAIN_EXTENSION_NAME) || !presentSupported(candidate) ||
				surfaceFormat(candidate, nullptr) == VK_FORMAT_UNDEFINED ||
				depthAttachmentFormat(candidate) == VK_FORMAT_UNDEFINED)
				continue;
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(candidate, &properties);
			const unsigned score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU	 ? 4
								   : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 3
								   : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU	 ? 2
																									 : 1;
			if (score <= best)
				continue;
			best = score;
			physical_ = candidate;
			properties_ = properties;
			graphicsFamily_ = graphicsIndex;
			presentFamily_ = presentIndex;
		}
		if (!physical_)
			throw std::runtime_error("no Vulkan device offers a graphics and present queue, a swapchain, "
									 "a linear RGBA8 surface format and a D24/D32 depth format");
		vkGetPhysicalDeviceMemoryProperties(physical_, &memoryProperties_);
		colorFormat_ = surfaceFormat(physical_, &colorSpace_);
		depthFormat_ = depthAttachmentFormat(physical_);
		depthAspect_ = depthFormat_ == VK_FORMAT_D24_UNORM_S8_UINT
						   ? VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
						   : VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	void createDevice()
	{
		const float priority = 1;
		VkDeviceQueueCreateInfo queues[2]{};
		queues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queues[0].queueFamilyIndex = graphicsFamily_;
		queues[0].queueCount = 1;
		queues[0].pQueuePriorities = &priority;
		std::uint32_t queueCount = 1;
		if (presentFamily_ != graphicsFamily_)
		{
			queues[1] = queues[0];
			queues[1].queueFamilyIndex = presentFamily_;
			queueCount = 2;
		}
		const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
		// No wideLines: line width above one is expanded into triangles above
		// this boundary. No occlusionQueryPrecise: the game only tests for zero.
		VkPhysicalDeviceFeatures features{};
		VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
		create.queueCreateInfoCount = queueCount;
		create.pQueueCreateInfos = queues;
		create.enabledExtensionCount = 1;
		create.ppEnabledExtensionNames = deviceExtensions;
		create.pEnabledFeatures = &features;
		require(vkCreateDevice(physical_, &create, nullptr, &device_), "create Vulkan device");
		vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
		vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);
	}

	void createSlots()
	{
		slots_.resize(FRAMES_IN_FLIGHT);
		for (Slot &slot : slots_)
		{
			VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
			pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			pool.queueFamilyIndex = graphicsFamily_;
			require(vkCreateCommandPool(device_, &pool, nullptr, &slot.pool), "create frame command pool");
			VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			require(vkCreateFence(device_, &fence, nullptr, &slot.fence), "create frame fence");
			VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			require(vkCreateSemaphore(device_, &semaphore, nullptr, &slot.acquire), "create acquire semaphore");
		}
	}

	bool createSwapchain()
	{
		VkSurfaceCapabilitiesKHR caps{};
		require(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &caps), "query surface capabilities");
		int width = 0, height = 0;
		drawableSize(&width, &height);
		VkExtent2D extent = caps.currentExtent;
		if (extent.width == 0xFFFFFFFFu || extent.height == 0xFFFFFFFFu)
		{
			extent.width = clampExtent(width, caps.minImageExtent.width, caps.maxImageExtent.width);
			extent.height = clampExtent(height, caps.minImageExtent.height, caps.maxImageExtent.height);
		}
		if (!extent.width || !extent.height)
			return false;
		// Readback copies out of the acquired image, and its first use is
		// initialized with a clear before the loading render pass reads it.
		const VkImageUsageFlags transfers = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if ((caps.supportedUsageFlags & transfers) != transfers)
			throw std::runtime_error("this surface cannot be read back or initialized: swapchain images lack "
									 "transfer-source and transfer-destination usage");
		std::uint32_t imageCount = caps.minImageCount + 1;
		if (caps.maxImageCount && imageCount > caps.maxImageCount)
			imageCount = caps.maxImageCount;
		std::uint32_t modeCount = 0;
		require(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &modeCount, nullptr),
				"query present modes");
		std::vector<VkPresentModeKHR> modes(modeCount);
		require(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &modeCount, modes.data()),
				"query present modes");
		modes.resize(modeCount);
		// Match the GL host's swap interval zero. FIFO is only the capability
		// fallback, not an unconditional refresh-rate cap on the renderer.
		presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
		if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end())
			presentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
		else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end())
			presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
		VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
		create.surface = surface_;
		create.minImageCount = imageCount;
		create.imageFormat = colorFormat_;
		create.imageColorSpace = colorSpace_;
		create.imageExtent = extent;
		create.imageArrayLayers = 1;
		create.imageUsage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		const std::uint32_t families[] = {graphicsFamily_, presentFamily_};
		if (graphicsFamily_ != presentFamily_)
		{
			create.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create.queueFamilyIndexCount = 2;
			create.pQueueFamilyIndices = families;
		}
		else
		{
			create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		create.preTransform = caps.currentTransform;
		const VkCompositeAlphaFlagBitsKHR alphaModes[] = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR};
		for (VkCompositeAlphaFlagBitsKHR mode : alphaModes)
		{
			if (caps.supportedCompositeAlpha & mode)
			{
				compositeAlpha_ = mode;
				break;
			}
		}
		create.compositeAlpha = compositeAlpha_;
		create.presentMode = presentMode_;
		// Screenshots and deterministic captures must include obscured pixels.
		create.clipped = VK_FALSE;
		require(vkCreateSwapchainKHR(device_, &create, nullptr, &swapchain_), "create swapchain");
		std::uint32_t count = 0;
		require(vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr), "query swapchain images");
		std::vector<VkImage> handles(count);
		require(vkGetSwapchainImagesKHR(device_, swapchain_, &count, handles.data()), "query swapchain images");
		images_.resize(count);
		for (std::uint32_t i = 0; i < count; ++i)
		{
			Image &image = images_[i];
			image = Image();
			image.image = handles[i];
			VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
			view.image = image.image;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.format = colorFormat_;
			view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
			require(vkCreateImageView(device_, &view, nullptr, &image.view), "create swapchain image view");
			VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			require(vkCreateSemaphore(device_, &semaphore, nullptr, &image.rendered), "create present semaphore");
		}
		createDepth(extent);
		extent_ = extent;
		createdWidth_ = width;
		createdHeight_ = height;
		needRecreate_ = false;
		return true;
	}

	void createDepth(VkExtent2D extent)
	{
		VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		create.imageType = VK_IMAGE_TYPE_2D;
		create.format = depthFormat_;
		create.extent = {extent.width, extent.height, 1};
		create.mipLevels = create.arrayLayers = 1;
		create.samples = VK_SAMPLE_COUNT_1_BIT;
		create.tiling = VK_IMAGE_TILING_OPTIMAL;
		// TRANSFER_DST carries the one-time initialization of a fresh image.
		create.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		require(vkCreateImage(device_, &create, nullptr, &depthImage_), "create depth image");
		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device_, depthImage_, &requirements);
		VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		allocate.allocationSize = requirements.size;
		allocate.memoryTypeIndex =
			memoryType(memoryProperties_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		require(vkAllocateMemory(device_, &allocate, nullptr, &depthMemory_), "allocate depth memory");
		require(vkBindImageMemory(device_, depthImage_, depthMemory_, 0), "bind depth memory");
		VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		view.image = depthImage_;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = depthFormat_;
		view.subresourceRange = {depthAspect_, 0, 1, 0, 1};
		require(vkCreateImageView(device_, &view, nullptr, &depthView_), "create depth view");
		depthInitialized_ = false;
	}

	void destroySwapchainResources()
	{
		if (!device_)
			return;
		if (depthView_)
			vkDestroyImageView(device_, depthView_, nullptr);
		if (depthImage_)
			vkDestroyImage(device_, depthImage_, nullptr);
		if (depthMemory_)
			vkFreeMemory(device_, depthMemory_, nullptr);
		depthView_ = VK_NULL_HANDLE;
		depthImage_ = VK_NULL_HANDLE;
		depthMemory_ = VK_NULL_HANDLE;
		depthInitialized_ = false;
		for (Image &image : images_)
		{
			if (image.view)
				vkDestroyImageView(device_, image.view, nullptr);
			if (image.rendered)
				vkDestroySemaphore(device_, image.rendered, nullptr);
		}
		images_.clear();
		if (swapchain_)
			vkDestroySwapchainKHR(device_, swapchain_, nullptr);
		swapchain_ = VK_NULL_HANDLE;
		extent_ = VkExtent2D{0, 0};
		// Nothing is drawable until a swapchain exists again.
		createdWidth_ = createdHeight_ = 0;
		needRecreate_ = true;
	}

	bool recreate()
	{
		if (recording_)
			throw std::logic_error("the swapchain cannot be recreated while a frame is recording");
		// Retire every frame that used the old attachments, then drop the
		// framebuffers built from their views before the views disappear.
		waitAllPending();
		require(vkDeviceWaitIdle(device_), "wait for device idle before resize");
		if (renderer_)
			renderer_->discardTargets();
		destroySwapchainResources();
		return createSwapchain();
	}

	std::uint64_t completedSerial()
	{
		// Ordered submissions on one queue: a signaled fence proves every
		// earlier serial finished too.
		for (Slot &slot : slots_)
			if (slot.pending && slot.serial > completed_ && vkGetFenceStatus(device_, slot.fence) == VK_SUCCESS)
				completed_ = slot.serial;
		return completed_;
	}

	void waitSlot(Slot &slot)
	{
		if (!slot.pending)
			return;
		require(vkWaitForFences(device_, 1, &slot.fence, VK_TRUE, UINT64_MAX), "wait for frame completion");
		if (slot.serial > completed_)
			completed_ = slot.serial;
		require(vkResetFences(device_, 1, &slot.fence), "reset frame fence");
		slot.pending = false;
	}

	void waitAllPending()
	{
		for (Slot &slot : slots_)
			waitSlot(slot);
	}

	void waitSerial(std::uint64_t serial)
	{
		for (Slot &slot : slots_)
			if (slot.pending && slot.serial >= serial)
			{
				require(vkWaitForFences(device_, 1, &slot.fence, VK_TRUE, UINT64_MAX), "wait for submitted work");
				if (slot.serial > completed_)
					completed_ = slot.serial;
			}
	}

	void beginCommands(Slot &slot)
	{
		if (!slot.used)
			require(vkResetCommandPool(device_, slot.pool, 0), "reset frame command pool");
		if (slot.used == slot.buffers.size())
		{
			VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			allocate.commandPool = slot.pool;
			allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocate.commandBufferCount = 1;
			VkCommandBuffer buffer = VK_NULL_HANDLE;
			require(vkAllocateCommandBuffers(device_, &allocate, &buffer), "allocate frame command buffer");
			slot.buffers.push_back(buffer);
		}
		commands_ = slot.buffers[slot.used++];
		VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		require(vkBeginCommandBuffer(commands_, &begin), "begin frame command buffer");
	}

	void beginDeviceFrame(const Slot &slot)
	{
		VulkanFrame frame;
		frame.commands = commands_;
		frame.colorView = images_[imageIndex_].view;
		frame.depthView = depthView_;
		frame.width = int(extent_.width);
		frame.height = int(extent_.height);
		frame.slot = frameIndex_ % unsigned(slots_.size());
		frame.serial = slot.serial;
		frame.completedSerial = completed_;
		renderer_->beginFrame(frame);
	}

	void prepareAttachments()
	{
		Image &image = images_[imageIndex_];
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image.image;
		barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		if (!image.initialized)
		{
			// The render pass loads its attachments, so their first use needs
			// defined contents: a masked game clear cannot supply them.
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			vkCmdPipelineBarrier(commands_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			VkClearColorValue black{};
			black.float32[3] = 1;
			const VkImageSubresourceRange color = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
			vkCmdClearColorImage(commands_, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &color);
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			image.initialized = true;
		}
		else
		{
			barrier.oldLayout = image.layout;
			barrier.srcAccessMask = 0;
		}
		barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		vkCmdPipelineBarrier(commands_, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		image.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		if (depthInitialized_)
			return;
		// A fresh or resized depth image is initialized once. Afterwards it
		// stays in the attachment layout and the render pass' external
		// dependencies order consecutive frames against each other.
		VkImageMemoryBarrier depth{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		depth.srcQueueFamilyIndex = depth.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		depth.image = depthImage_;
		depth.subresourceRange = {depthAspect_, 0, 1, 0, 1};
		depth.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		depth.srcAccessMask = 0;
		depth.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(commands_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
							 nullptr, 0, nullptr, 1, &depth);
		VkClearDepthStencilValue clearDepthValue{};
		clearDepthValue.depth = 1;
		const VkImageSubresourceRange range = {depthAspect_, 0, 1, 0, 1};
		vkCmdClearDepthStencilImage(commands_, depthImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepthValue, 1,
									&range);
		depth.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		depth.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		depth.dstAccessMask =
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		vkCmdPipelineBarrier(commands_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
							 0, nullptr, 0, nullptr, 1, &depth);
		depthInitialized_ = true;
	}

	void presentBarrier()
	{
		Image &image = images_[imageIndex_];
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image.image;
		barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		barrier.oldLayout = image.layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = 0;
		vkCmdPipelineBarrier(commands_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		image.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}

	void submit(bool signalRendered)
	{
		Slot &slot = currentSlot();
		require(vkEndCommandBuffer(commands_), "end frame command buffer");
		const VkPipelineStageFlags waitStage =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		// The acquire semaphore is consumed by the frame's first submission only.
		if (!acquireWaited_)
		{
			info.waitSemaphoreCount = 1;
			info.pWaitSemaphores = &slot.acquire;
			info.pWaitDstStageMask = &waitStage;
		}
		info.commandBufferCount = 1;
		info.pCommandBuffers = &commands_;
		if (signalRendered)
		{
			info.signalSemaphoreCount = 1;
			info.pSignalSemaphores = &images_[imageIndex_].rendered;
		}
		require(vkQueueSubmit(graphicsQueue_, 1, &info, slot.fence), "submit frame");
		slot.pending = true;
		acquireWaited_ = true;
		commands_ = VK_NULL_HANDLE;
	}

	// Submit what is recorded, wait for it, and resume recording into the same
	// acquired color/depth target under a higher serial. Nothing is presented.
	void splitSubmission()
	{
		Slot &slot = currentSlot();
		const std::uint32_t active = activeQuery_;
		// A query may not span command buffers; its segments' samples add up.
		if (active)
			endQuerySegment(queries_.at(active));
		renderer_->endFrame();
		submit(false);
		waitSlot(slot);
		slot.serial = nextSerial_++;
		beginCommands(slot);
		beginDeviceFrame(slot);
		if (active)
			beginQuerySegment(queries_.at(active));
	}

	void growQueryPool()
	{
		auto pool = std::make_shared<QueryPool>();
		pool->device = device_;
		VkQueryPoolCreateInfo create{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		create.queryType = VK_QUERY_TYPE_OCCLUSION;
		create.queryCount = QUERIES_PER_POOL;
		require(vkCreateQueryPool(device_, &create, nullptr, &pool->pool), "create occlusion query pool");
		const std::uint32_t base = std::uint32_t(queryPools_.size()) * QUERIES_PER_POOL;
		queryPools_.push_back(pool);
		for (std::uint32_t i = 0; i < QUERIES_PER_POOL; ++i)
		{
			FreeQuery entry;
			entry.index = base + i;
			entry.serial = 0;
			freeQuerySlots_.push_back(entry);
		}
	}

	std::uint32_t allocateQuerySlot()
	{
		// A slot is reusable only once the submission that last reset, began and
		// ended it completed: resetting live queries would race the GPU.
		const std::uint64_t done = completedSerial();
		for (std::size_t i = 0; i < freeQuerySlots_.size(); ++i)
			if (freeQuerySlots_[i].serial <= done)
			{
				const std::uint32_t index = freeQuerySlots_[i].index;
				freeQuerySlots_.erase(freeQuerySlots_.begin() + std::ptrdiff_t(i));
				return index;
			}
		growQueryPool();
		const std::uint32_t index = freeQuerySlots_.back().index;
		freeQuerySlots_.pop_back();
		return index;
	}

	void releaseSegments(Query &query)
	{
		const std::uint64_t gate = query.serial ? query.serial : recordedSerial();
		for (std::uint32_t index : query.segments)
		{
			FreeQuery entry;
			entry.index = index;
			entry.serial = gate;
			freeQuerySlots_.push_back(entry);
		}
		query.segments.clear();
	}

	void beginQuerySegment(Query &query)
	{
		const std::uint32_t index = allocateQuerySlot();
		const std::shared_ptr<QueryPool> &pool = queryPools_[index / QUERIES_PER_POOL];
		// The pool outlives every submission referencing it, even when the game
		// deletes its query names first.
		renderer_->retainFrameResource(pool);
		renderer_->beginOcclusionQuery(pool->pool, index % QUERIES_PER_POOL);
		query.segments.push_back(index);
	}

	void endQuerySegment(Query &query)
	{
		if (query.segments.empty())
			throw std::logic_error("occlusion query has no recorded segment");
		const std::uint32_t index = query.segments.back();
		const std::shared_ptr<QueryPool> &pool = queryPools_[index / QUERIES_PER_POOL];
		renderer_->retainFrameResource(pool);
		renderer_->endOcclusionQuery(pool->pool, index % QUERIES_PER_POOL);
	}

	std::string describe() const
	{
		const std::uint32_t api = properties_.apiVersion;
		std::string text = "Vulkan " + std::to_string(VK_API_VERSION_MAJOR(api)) + "." +
						   std::to_string(VK_API_VERSION_MINOR(api)) + "." + std::to_string(VK_API_VERSION_PATCH(api));
		text += " - ";
		text += properties_.deviceName;
		text += " (";
		text += deviceKind(properties_.deviceType);
		text += ", driver ";
		text += hex(properties_.driverVersion);
		text += ")";
		text += presentMode_ == VK_PRESENT_MODE_IMMEDIATE_KHR ? " [immediate]"
				: presentMode_ == VK_PRESENT_MODE_MAILBOX_KHR ? " [mailbox]"
															  : " [fifo]";
		if (validationEnabled_)
			text += " [validation]";
		else if (validationRequested_)
			text += " [validation requested but unavailable]";
		return text;
	}

	void destroy()
	{
		if (device_)
		{
			// Complete submitted work; no destructor guesses that the GPU is idle.
			for (Slot &slot : slots_)
				if (slot.pending)
					vkWaitForFences(device_, 1, &slot.fence, VK_TRUE, UINT64_MAX);
			vkDeviceWaitIdle(device_);
		}
		// Unsubmitted commands are discarded with their pool; the renderer drops
		// its framebuffers, descriptors, meshes and textures first.
		renderer_.reset();
		queries_.clear();
		freeQuerySlots_.clear();
		queryPools_.clear();
		destroySwapchainResources();
		if (device_)
		{
			for (Slot &slot : slots_)
			{
				if (slot.acquire)
					vkDestroySemaphore(device_, slot.acquire, nullptr);
				if (slot.fence)
					vkDestroyFence(device_, slot.fence, nullptr);
				if (slot.pool)
					vkDestroyCommandPool(device_, slot.pool, nullptr);
			}
			slots_.clear();
			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}
		if (surface_)
		{
			vkDestroySurfaceKHR(instance_, surface_, nullptr);
			surface_ = VK_NULL_HANDLE;
		}
		if (messenger_ && destroyMessenger_)
		{
			destroyMessenger_(instance_, messenger_, nullptr);
			messenger_ = VK_NULL_HANDLE;
		}
		if (instance_)
		{
			vkDestroyInstance(instance_, nullptr);
			instance_ = VK_NULL_HANDLE;
		}
		if (loaded_)
		{
			SDL_Vulkan_UnloadLibrary();
			loaded_ = false;
		}
	}
};

std::unique_ptr<BackendHost> createVulkanHost(SDL_Window *window)
{
	return std::unique_ptr<BackendHost>(new VulkanHost(window));
}
} // namespace render
} // namespace b173
