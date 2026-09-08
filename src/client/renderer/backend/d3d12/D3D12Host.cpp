#include "client/renderer/backend/d3d12/D3D12Device.h"

#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "client/renderer/backend/BackendHost.h"

#include "SDL.h"
#include "SDL_syswm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace b173
{
namespace render
{
namespace D3D12HostDetail
{
using Microsoft::WRL::ComPtr;
// Three swapchain images and three frame leases. FrameLifetime refuses to reuse a
// slot whose serial has not retired, so the steady state waits at most on the one
// slot the host is about to record into again - never on the present itself.
const unsigned kFrameCount = 3;
// One query heap plus one readback buffer per 4096 occlusion queries. LevelRenderer
// generates one query per chunk (thousands at long view distances) and may begin
// every near chunk's query within a single frame, so slots are per query, not per
// frame, and the pool grows on demand instead of overwriting live queries.
const unsigned kQueryPoolSize = 4096;
// ResolveQueryData writes one UINT64 per occlusion query, binary or counting.
const std::uint64_t kQueryBytes = 8;

static std::string hresult(HRESULT result)
{
	std::ostringstream s;
	s << "HRESULT 0x" << std::hex << static_cast<unsigned long>(result);
	return s.str();
}

static void check(HRESULT result, const char *where)
{
	if (FAILED(result))
		throw std::runtime_error(std::string(where) + ": " + hresult(result));
}

static std::string utf8(const WCHAR *text)
{
	if (!text || !*text)
		return "unnamed adapter";
	const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
	if (bytes <= 1)
		return "unnamed adapter";
	std::vector<char> buffer(std::size_t(bytes), '\0');
	if (!WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer.data(), bytes, nullptr, nullptr))
		return "unnamed adapter";
	return std::string(buffer.data());
}

static std::string featureLevelName(D3D_FEATURE_LEVEL level)
{
	switch (static_cast<unsigned>(level))
	{
	case 0xc200:
		return "12_2"; // D3D_FEATURE_LEVEL_12_2, named only in newer SDKs.
	case D3D_FEATURE_LEVEL_12_1:
		return "12_1";
	case D3D_FEATURE_LEVEL_12_0:
		return "12_0";
	case D3D_FEATURE_LEVEL_11_1:
		return "11_1";
	case D3D_FEATURE_LEVEL_11_0:
		return "11_0";
	default:
		break;
	}
	std::ostringstream s;
	s << "0x" << std::hex << static_cast<unsigned>(level);
	return s.str();
}

static std::string formatName(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return "R8G8B8A8_UNORM";
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return "B8G8R8A8_UNORM";
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return "D24_UNORM_S8_UINT";
	case DXGI_FORMAT_D32_FLOAT:
		return "D32_FLOAT";
	default:
		break;
	}
	std::ostringstream s;
	s << "DXGI_FORMAT " << unsigned(format);
	return s.str();
}

// B173_RENDER_VALIDATION=1 asks for the debug layer. Any other non-empty value
// except "0" enables it too; its absence is reported, never silently ignored.
static bool validationRequested()
{
	const char *value = std::getenv("B173_RENDER_VALIDATION");
	return value && *value && std::strcmp(value, "0") != 0;
}
} // namespace D3D12HostDetail

using namespace D3D12HostDetail;

// B173 - Direct3D 12 window/device/presentation owner. Submission, fences and the
// swapchain live here; D3D12Device only records into the list this host supplies.
class D3D12Host final : public BackendHost
{
	struct EventHandle
	{
		HANDLE value = nullptr;

		~EventHandle()
		{
			if (value)
				CloseHandle(value);
		}
	};

	struct Slot
	{
		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12Resource> color;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
		std::uint64_t serial = 0; // Last serial submitted with this slot's allocator.
		D3D12_RESOURCE_STATES colorState = D3D12_RESOURCE_STATE_PRESENT;
		bool cleared = false;
	};

	// State the currently recorded list will leave behind. It is committed to the
	// slot only when that list is actually executed, so an abandoned frame cannot
	// desynchronize resource-state or first-clear tracking.
	struct Recorded
	{
		D3D12_RESOURCE_STATES colorState = D3D12_RESOURCE_STATE_PRESENT;
		bool colorClear = false, depthClear = false;
	};

	struct QueryPool
	{
		ComPtr<ID3D12QueryHeap> heap;
		ComPtr<ID3D12Resource> readback;
	};

	struct Query
	{
		unsigned slot = 0;
		std::uint64_t serial = 0;
		bool resolved = false;
	};

	struct RetiringQuery
	{
		unsigned slot = 0;
		std::uint64_t serial = 0;
	};

	SDL_Window *window_ = nullptr;
	HWND hwnd_ = nullptr;
	ComPtr<IDXGIFactory4> factory_;
	ComPtr<ID3D12Device> device_;
	ComPtr<ID3D12CommandQueue> queue_;
	ComPtr<IDXGISwapChain3> swapchain_;
	ComPtr<ID3D12DescriptorHeap> rtvHeap_, dsvHeap_;
	ComPtr<ID3D12Resource> depth_;
	ComPtr<ID3D12GraphicsCommandList> commands_;
	ComPtr<ID3D12Fence> fence_;
	ComPtr<ID3D12InfoQueue> messages_;
	EventHandle fenceEvent_;
	std::vector<Slot> slots_;
	std::vector<QueryPool> queryPools_;
	std::unordered_map<std::uint32_t, Query> queries_;
	std::vector<unsigned> freeQuerySlots_;
	std::vector<RetiringQuery> retiringQuerySlots_;
	// Declared after every native object it draws with: destroyed first.
	std::unique_ptr<D3D12Device> renderer_;
	std::string description_, validation_, adapterName_;
	MeshData clearMesh_;
	float clearDepth_ = -1;
	DXGI_FORMAT colorFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM;
	// D24_UNORM keeps GL's integral polygon-offset units exact; see D3D12Device.h.
	DXGI_FORMAT depthFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
	Recorded recorded_;
	UINT rtvIncrement_ = 0;
	int width_ = 0, height_ = 0;
	unsigned slot_ = 0;
	std::uint64_t nextSerial_ = 1, serial_ = 0, submitted_ = 0;
	std::uint32_t nextQuery_ = 1, activeQuery_ = 0;
	bool recording_ = false, depthCleared_ = false, validating_ = false;
	bool validationError_ = false;

  public:
	explicit D3D12Host(SDL_Window *window) : window_(window), slots_(kFrameCount)
	{
		if (!window_)
			throw std::invalid_argument("Direct3D 12 host requires an SDL window");
		SDL_SysWMinfo wm;
		SDL_VERSION(&wm.version);
		if (!SDL_GetWindowWMInfo(window_, &wm))
			throw std::runtime_error(std::string("SDL_GetWindowWMInfo failed: ") + SDL_GetError());
		if (wm.subsystem != SDL_SYSWM_WINDOWS)
			throw std::runtime_error("Direct3D 12 needs a Win32 window; this SDL video driver provides none");
		hwnd_ = wm.info.win.window;
		// Debug/GPU-based validation must be turned on before device creation.
		if (validationRequested())
			enableValidation();
		createDevice();
		createQueueAndFence();
		createSwapchain();
		createDescriptorHeaps();
		createFrameCommands();
		createTargets();
		D3D12CreateInfo info;
		info.device = device_.Get();
		info.colorFormat = colorFormat_;
		info.depthFormat = depthFormat_;
		info.framesInFlight = kFrameCount;
		renderer_.reset(new D3D12Device(info));
		describe();
		drainValidation();
	}

	~D3D12Host() override
	{
		try
		{
			// An abandoned recording is closed, never executed, so nothing it
			// references can still be reachable by the GPU.
			if (recording_)
			{
				commands_->Close();
				recording_ = false;
			}
			waitForIdle();
		}
		catch (...)
		{
			// A removed or hung device never retires work. Releasing is all that
			// is left, and a destructor must not propagate the failure.
		}
		renderer_.reset();
		drainValidation();
	}

	Device &device() override
	{
		return *renderer_;
	}

	bool beginFrame() override
	{
		if (recording_)
			return true;
		int width = 0, height = 0;
		clientSize(&width, &height);
		// Minimized or zero-area client rect: there is nothing to draw into, and
		// DXGI rejects a zero-sized swapchain, so the old one is left untouched.
		if (width <= 0 || height <= 0)
			return false;
		if (width != width_ || height != height_)
			resize(width, height);
		retireQueries();
		slot_ = swapchain_->GetCurrentBackBufferIndex();
		Slot &slot = slots_.at(slot_);
		// The only wait in a normal frame, and only when this slot's own previous
		// frame has not retired yet.
		waitForSerial(slot.serial);
		check(slot.allocator->Reset(), "reset frame command allocator");
		check(commands_->Reset(slot.allocator.Get(), nullptr), "reset frame command list");
		recorded_ = Recorded();
		recorded_.colorState = slot.colorState;
		try
		{
			transitionColor(D3D12_RESOURCE_STATE_RENDER_TARGET);
			clearFreshTargets(slot);
			serial_ = nextSerial_++;
			renderer_->beginFrame(currentFrame());
		}
		catch (...)
		{
			// Close the list so the next frame may reset its allocator.
			commands_->Close();
			throw;
		}
		recording_ = true;
		return true;
	}

	void present() override
	{
		// Display::update presents on every tick, including ticks where beginFrame
		// reported no drawable surface. There is nothing recorded to present then.
		if (!recording_)
			return;
		if (activeQuery_)
			throw std::logic_error("an occlusion query is still open at present");
		renderer_->endFrame();
		transitionColor(D3D12_RESOURCE_STATE_PRESENT);
		submit();
		recording_ = false;
		// Sync interval 0 matches the GL path's SDL_GL_SetSwapInterval(0). The
		// present is asynchronous: no fence wait, no queue flush here.
		const HRESULT result = swapchain_->Present(0, 0);
		if (FAILED(result))
			throw std::runtime_error("Direct3D 12 present failed: " + hresult(result) + " (device removal reason " +
									 hresult(device_->GetDeviceRemovedReason()) + ")");
		drainValidation();
	}

	void finish() override
	{
		if (!recording_)
		{
			waitForIdle();
			return;
		}
		if (activeQuery_)
			throw std::logic_error("an occlusion query cannot span a submission boundary");
		submitAndResume();
	}

	void clear(Vec4 color, float depth, bool colorBit, bool depthBit, const PipelineState &state) override
	{
		requireFrame("clear");
		if (!colorBit && !depthBit)
			return;
		// One cached clear triangle per depth value; the shared draw-based clear
		// honors the caller's write masks and scissor, which ClearRTV cannot.
		if (clearMesh_.vertices.empty() || clearDepth_ != depth)
		{
			clearMesh_ = clearTriangle(depth);
			clearDepth_ = depth;
		}
		// drawTransient assigns its own transient mesh id; the argument is unused.
		renderer_->drawTransient(clearMesh_, clearDraw(0, width_, height_, color, colorBit, depthBit, state));
	}

	std::vector<std::uint8_t> readPixels(Rect region) override
	{
		// A flip-model back buffer has no defined contents outside a frame, so a
		// readback is only meaningful against the target being recorded.
		requireFrame("readback");
		if (region.width <= 0 || region.height <= 0 || region.x < 0 || region.y < 0 ||
			std::int64_t(region.x) + region.width > width_ || std::int64_t(region.y) + region.height > height_)
			throw std::out_of_range("readback rectangle is outside the drawable");
		if (activeQuery_)
			throw std::logic_error("an occlusion query cannot span a submission boundary");
		const D3D12Readback ticket = renderer_->recordReadback();
		// Submits the copy and waits for it, then keeps recording into the same
		// back buffer: the swapchain image is neither presented nor advanced.
		submitAndResume();
		std::vector<std::uint8_t> full = renderer_->resolveReadback(ticket, fence_->GetCompletedValue());
		if (!region.x && !region.y && region.width == width_ && region.height == height_)
			return full;
		// Tightly packed RGBA, bottom-left rows, cropped to the requested rect.
		const std::size_t row = std::size_t(region.width) * 4, pitch = std::size_t(width_) * 4;
		std::vector<std::uint8_t> out(row * std::size_t(region.height));
		for (int y = 0; y < region.height; ++y)
			std::memcpy(out.data() + std::size_t(y) * row,
						full.data() + std::size_t(region.y + y) * pitch + std::size_t(region.x) * 4, row);
		return out;
	}

	void drawableSize(int *width, int *height) const override
	{
		// The swapchain extent, which is what the recorded frame actually uses.
		if (width)
			*width = width_;
		if (height)
			*height = height_;
	}

	std::string description() const override
	{
		return description_;
	}

	// Wide lines are expanded into triangles above this backend; D3D12 has no
	// wide-line rasterizer, and the device rejects any line draw that still
	// carries a non-unit width.
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
			throw std::invalid_argument("invalid occlusion query name request");
		const std::uint32_t first = nextQuery_;
		int made = 0;
		try
		{
			for (; made < count; ++made)
			{
				if (nextQuery_ == std::numeric_limits<std::uint32_t>::max())
					throw std::length_error("occlusion query names exhausted");
				Query query;
				query.slot = acquireQuerySlot();
				ids[made] = nextQuery_++;
				queries_.emplace(ids[made], query);
			}
		}
		catch (...)
		{
			for (int i = 0; i < made; ++i)
			{
				freeQuerySlots_.push_back(queries_.at(ids[i]).slot);
				queries_.erase(ids[i]);
			}
			nextQuery_ = first;
			throw;
		}
	}

	void deleteQueries(int count, const std::uint32_t *ids) override
	{
		if (count < 0 || (count && !ids))
			throw std::invalid_argument("invalid occlusion query name list");
		for (int i = 0; i < count; ++i)
		{
			// glDeleteQueries ignores zero and names that are not queries.
			auto found = queries_.find(ids[i]);
			if (found == queries_.end())
				continue;
			if (activeQuery_ == ids[i])
				throw std::logic_error("cannot delete an occlusion query while it is recording");
			// The heap slot and its readback bytes stay reserved until the GPU has
			// passed the resolve recorded for them, even though the name is gone.
			RetiringQuery retiring;
			retiring.slot = found->second.slot;
			retiring.serial = found->second.resolved ? found->second.serial : 0;
			retiringQuerySlots_.push_back(retiring);
			queries_.erase(found);
		}
		retireQueries();
	}

	void beginQuery(std::uint32_t id) override
	{
		requireFrame("occlusion query");
		if (activeQuery_)
			throw std::logic_error("an occlusion query is already recording");
		Query &query = queryAt(id);
		QueryPool &pool = queryPools_.at(query.slot / kQueryPoolSize);
		commands_->BeginQuery(pool.heap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, query.slot % kQueryPoolSize);
		activeQuery_ = id;
	}

	void endQuery() override
	{
		requireFrame("occlusion query");
		if (!activeQuery_)
			throw std::logic_error("no occlusion query is recording");
		Query &query = queryAt(activeQuery_);
		QueryPool &pool = queryPools_.at(query.slot / kQueryPoolSize);
		const UINT index = query.slot % kQueryPoolSize;
		commands_->EndQuery(pool.heap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, index);
		// Resolved in the same list that recorded it; the result becomes readable
		// when this frame's fence value is reached, not before.
		commands_->ResolveQueryData(pool.heap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, index, 1, pool.readback.Get(),
									std::uint64_t(index) * kQueryBytes);
		query.serial = serial_;
		query.resolved = true;
		activeQuery_ = 0;
	}

	std::uint32_t queryResult(std::uint32_t id, bool availability) override
	{
		Query &query = queryAt(id);
		if (!query.resolved)
		{
			if (availability)
				return 0;
			throw std::logic_error("occlusion query has no recorded result");
		}
		if (availability)
			return fence_->GetCompletedValue() >= query.serial ? 1u : 0u;
		// GL_QUERY_RESULT blocks until the result exists. When the resolve is
		// still sitting in this frame's unsubmitted list no fence can ever reach
		// it, so the frame is flushed first, exactly like GL's implicit flush.
		// The game polls availability first, so neither path is the normal one.
		if (query.serial > submitted_)
		{
			if (!recording_)
				throw std::logic_error("occlusion query result belongs to a frame that was never submitted");
			if (activeQuery_)
				throw std::logic_error("an occlusion query cannot span a submission boundary");
			submitAndResume();
		}
		waitForSerial(query.serial);
		// A binary occlusion query answers "any samples passed". The only consumer
		// compares the value against zero, so one is the honest nonzero answer.
		return sampleQuery(query) ? 1u : 0u;
	}

	unsigned error() override
	{
		drainValidation();
		if (validationError_)
		{
			validationError_ = false;
			return 0x0502u; // Invalid operation at the legacy semantic boundary.
		}
		const HRESULT removed = device_->GetDeviceRemovedReason();
		return SUCCEEDED(removed) ? 0u : unsigned(removed);
	}

  private:
	void requireFrame(const char *operation)
	{
		if (!recording_)
			throw std::logic_error(std::string("Direct3D 12 ") + operation + " requires an open frame");
	}

	void enableValidation()
	{
		ComPtr<ID3D12Debug> debug;
		const HRESULT result = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
		if (FAILED(result))
		{
			validation_ = "validation requested but the D3D12 debug layer is unavailable (" + hresult(result) +
						  "); install the Windows \"Graphics Tools\" optional feature";
			std::fprintf(stderr, "B173 D3D12: %s\n", validation_.c_str());
			return;
		}
		debug->EnableDebugLayer();
		validating_ = true;
		validation_ = "debug layer";
		ComPtr<ID3D12Debug1> gpuValidation;
		if (SUCCEEDED(debug.As(&gpuValidation)))
		{
			gpuValidation->SetEnableGPUBasedValidation(TRUE);
			gpuValidation->SetEnableSynchronizedCommandQueueValidation(TRUE);
			validation_ += " with GPU-based and synchronized-queue validation";
		}
		else
		{
			validation_ += " without GPU-based validation (ID3D12Debug1 unavailable)";
			std::fprintf(stderr, "B173 D3D12: %s\n", validation_.c_str());
		}
	}

	// Drains the debug layer to stderr. Without this the messages only reach an
	// attached debugger, and a console validation run would look clean.
	void drainValidation()
	{
		if (!messages_)
			return;
		const UINT64 stored = messages_->GetNumStoredMessages();
		for (UINT64 i = 0; i < stored; ++i)
		{
			SIZE_T bytes = 0;
			if (FAILED(messages_->GetMessage(i, nullptr, &bytes)) || bytes < sizeof(D3D12_MESSAGE))
				continue;
			// Suitably aligned storage for the header the runtime writes ahead of
			// the description, which is a null-terminated string.
			std::vector<std::uint64_t> storage((std::size_t(bytes) + 7) / 8, 0);
			D3D12_MESSAGE *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
			if (FAILED(messages_->GetMessage(i, message, &bytes)) || !message->pDescription)
				continue;
			if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
				message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
				validationError_ = true;
			std::fprintf(stderr, "B173 D3D12 validation (severity %u, id %u): %s\n", unsigned(message->Severity),
						 unsigned(message->ID), message->pDescription);
		}
		if (stored)
			messages_->ClearStoredMessages();
	}

	void createDevice()
	{
		UINT flags = 0;
		if (validating_)
			flags |= DXGI_CREATE_FACTORY_DEBUG;
		check(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_)), "create DXGI factory");
		std::string rejected;
		// IDXGIFactory6 orders adapters by GPU preference; EnumAdapters1 does not,
		// so the plain enumeration is only the fallback for older runtimes.
		ComPtr<IDXGIFactory6> ordered;
		if (SUCCEEDED(factory_.As(&ordered)))
		{
			for (UINT i = 0;; ++i)
			{
				ComPtr<IDXGIAdapter1> candidate;
				if (FAILED(ordered->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
															   IID_PPV_ARGS(&candidate))))
					break;
				if (tryAdapter(candidate, rejected))
					return;
			}
		}
		for (UINT i = 0;; ++i)
		{
			ComPtr<IDXGIAdapter1> candidate;
			if (FAILED(factory_->EnumAdapters1(i, &candidate)))
				break;
			if (tryAdapter(candidate, rejected))
				return;
		}
		throw std::runtime_error("no Direct3D 12 hardware adapter accepted device creation" +
								 (rejected.empty() ? std::string() : ": " + rejected));
	}

	bool tryAdapter(const ComPtr<IDXGIAdapter1> &candidate, std::string &rejected)
	{
		DXGI_ADAPTER_DESC1 desc{};
		if (FAILED(candidate->GetDesc1(&desc)))
			return false;
		// WARP renders correctly but at software speed; it is never substituted
		// for the hardware path the game asked for.
		if ((desc.Flags & unsigned(DXGI_ADAPTER_FLAG_SOFTWARE)) != 0)
		{
			rejected += utf8(desc.Description) + " is a software adapter; ";
			return false;
		}
		// 11_0 is the D3D12 floor. The level actually supported is queried below.
		const HRESULT result = D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));
		if (FAILED(result))
		{
			rejected += utf8(desc.Description) + " " + hresult(result) + "; ";
			return false;
		}
		adapterName_ = utf8(desc.Description);
		if (validating_)
			device_.As(&messages_);
		return true;
	}

	D3D_FEATURE_LEVEL supportedFeatureLevel()
	{
		const D3D_FEATURE_LEVEL levels[] = {static_cast<D3D_FEATURE_LEVEL>(0xc200), D3D_FEATURE_LEVEL_12_1,
											D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
		D3D12_FEATURE_DATA_FEATURE_LEVELS data{};
		data.NumFeatureLevels = 5;
		data.pFeatureLevelsRequested = levels;
		if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &data, sizeof(data))))
			return data.MaxSupportedFeatureLevel;
		// A runtime that does not know 12_2 rejects the whole list; ask again
		// without it rather than reporting a level that was never verified.
		data.NumFeatureLevels = 4;
		data.pFeatureLevelsRequested = levels + 1;
		check(device_->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &data, sizeof(data)),
			  "query supported feature level");
		return data.MaxSupportedFeatureLevel;
	}

	void createQueueAndFence()
	{
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		// Exactly one graphics queue: every serial the renderer sees is ordered
		// against every other, which is what FrameLifetime assumes.
		check(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_)), "create direct command queue");
		queue_->SetName(L"b173 direct queue");
		check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "create frame fence");
		fence_->SetName(L"b173 frame serial");
		fenceEvent_.value = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
		if (!fenceEvent_.value)
			throw std::runtime_error("CreateEventEx failed for the frame fence");
	}

	void createSwapchain()
	{
		initialSize(&width_, &height_);
		DXGI_SWAP_CHAIN_DESC1 desc{};
		desc.Width = UINT(width_);
		desc.Height = UINT(height_);
		// A linear UNORM flip-model format: no sRGB conversion is applied to the
		// values the shared shaders compute, matching the GL backends.
		desc.Format = colorFormat_;
		desc.SampleDesc.Count = 1;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = kFrameCount;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		ComPtr<IDXGISwapChain1> chain;
		check(factory_->CreateSwapChainForHwnd(queue_.Get(), hwnd_, &desc, nullptr, nullptr, &chain),
			  "create flip-model swapchain");
		check(chain.As(&swapchain_), "query IDXGISwapChain3");
		// SDL owns fullscreen transitions; DXGI must not consume Alt+Enter.
		check(factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER), "associate swapchain window");
	}

	void createDescriptorHeaps()
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc{};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = kFrameCount;
		check(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap_)), "create RTV heap");
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.NumDescriptors = 1;
		check(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvHeap_)), "create DSV heap");
		rtvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// One allocator per frame slot and one command list for all of them. A list
	// object may be reset as soon as it has been closed and submitted; only the
	// allocator's memory has to survive until the GPU is done with it, which is
	// what the per-slot fence wait in beginFrame guarantees. Duplicating the list
	// per slot would buy nothing.
	void createFrameCommands()
	{
		for (unsigned i = 0; i < kFrameCount; ++i)
			check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slots_[i].allocator)),
				  "create frame command allocator");
		check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, slots_[0].allocator.Get(), nullptr,
										 IID_PPV_ARGS(&commands_)),
			  "create direct command list");
		// Created recording; closed so every frame starts from the same state.
		check(commands_->Close(), "close initial command list");
	}

	void createTargets()
	{
		for (unsigned i = 0; i < kFrameCount; ++i)
		{
			Slot &slot = slots_[i];
			check(swapchain_->GetBuffer(i, IID_PPV_ARGS(&slot.color)), "acquire swapchain buffer");
			slot.rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
			slot.rtv.ptr += SIZE_T(i) * rtvIncrement_;
			D3D12_RENDER_TARGET_VIEW_DESC view{};
			view.Format = colorFormat_;
			view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			device_->CreateRenderTargetView(slot.color.Get(), &view, slot.rtv);
			// Swapchain buffers start in the PRESENT/COMMON state.
			slot.colorState = D3D12_RESOURCE_STATE_PRESENT;
			slot.cleared = false;
		}
		D3D12_RESOURCE_DESC image{};
		image.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		image.Width = UINT64(width_);
		image.Height = UINT(height_);
		image.DepthOrArraySize = image.MipLevels = 1;
		image.Format = depthFormat_;
		image.SampleDesc.Count = 1;
		image.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		image.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		// The optimized clear value matches the depth the first clear writes.
		D3D12_CLEAR_VALUE clear{};
		clear.Format = depthFormat_;
		clear.DepthStencil.Depth = 1;
		clear.DepthStencil.Stencil = 0;
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		heap.CreationNodeMask = heap.VisibleNodeMask = 1;
		depth_.Reset();
		// Created and kept in DEPTH_WRITE: the depth target is never sampled, so
		// it never needs another state.
		check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &image, D3D12_RESOURCE_STATE_DEPTH_WRITE,
											   &clear, IID_PPV_ARGS(&depth_)),
			  "create depth target");
		depth_->SetName(L"b173 depth target");
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
		dsv.Format = depthFormat_;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device_->CreateDepthStencilView(depth_.Get(), &dsv, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
		depthCleared_ = false;
	}

	void resize(int width, int height)
	{
		if (recording_)
			throw std::logic_error("the Direct3D 12 swapchain cannot be resized inside a recorded frame");
		// ResizeBuffers requires every outstanding back-buffer reference to be
		// released and no queued work to be reading them.
		waitForIdle();
		for (Slot &slot : slots_)
			slot.color.Reset();
		depth_.Reset();
		const HRESULT result = swapchain_->ResizeBuffers(kFrameCount, UINT(width), UINT(height), colorFormat_, 0);
		if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
			throw std::runtime_error("Direct3D 12 device lost while resizing: " +
									 hresult(device_->GetDeviceRemovedReason()));
		check(result, "resize swapchain buffers");
		width_ = width;
		height_ = height;
		createTargets();
	}

	void clearFreshTargets(Slot &slot)
	{
		// New or resized targets hold undefined memory. The game's own clear may
		// be masked or scissored, and a readback may sample never-drawn pixels.
		if (!slot.cleared)
		{
			const float black[4] = {0, 0, 0, 1};
			commands_->ClearRenderTargetView(slot.rtv, black, 0, nullptr);
			recorded_.colorClear = true;
		}
		if (!depthCleared_)
		{
			commands_->ClearDepthStencilView(dsvHeap_->GetCPUDescriptorHandleForHeapStart(),
											 D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);
			recorded_.depthClear = true;
		}
	}

	void transitionColor(D3D12_RESOURCE_STATES after)
	{
		if (recorded_.colorState == after)
			return;
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = slots_.at(slot_).color.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = recorded_.colorState;
		barrier.Transition.StateAfter = after;
		commands_->ResourceBarrier(1, &barrier);
		recorded_.colorState = after;
	}

	D3D12Frame currentFrame() const
	{
		D3D12Frame frame;
		frame.commands = commands_.Get();
		frame.colorResource = slots_.at(slot_).color.Get();
		frame.colorRTV = slots_.at(slot_).rtv;
		frame.depthDSV = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
		frame.width = width_;
		frame.height = height_;
		frame.slot = slot_;
		frame.serial = serial_;
		// A real fence value, never a frame counter.
		frame.completedSerial = fence_->GetCompletedValue();
		return frame;
	}

	void submit()
	{
		check(commands_->Close(), "close frame command list");
		ID3D12CommandList *lists[] = {commands_.Get()};
		queue_->ExecuteCommandLists(1, lists);
		check(queue_->Signal(fence_.Get(), serial_), "signal frame serial");
		submitted_ = serial_;
		Slot &slot = slots_.at(slot_);
		slot.serial = serial_;
		// Only an executed list may update the tracked resource state.
		slot.colorState = recorded_.colorState;
		if (recorded_.colorClear)
			slot.cleared = true;
		if (recorded_.depthClear)
			depthCleared_ = true;
	}

	// Submits what has been recorded, waits for it, then resumes recording into
	// the same color and depth target with a greater serial. Nothing is presented
	// and the swapchain image is not advanced.
	void submitAndResume()
	{
		renderer_->endFrame();
		submit();
		waitForSerial(submitted_);
		Slot &slot = slots_.at(slot_);
		check(slot.allocator->Reset(), "reset command allocator after readback");
		check(commands_->Reset(slot.allocator.Get(), nullptr), "resume command list after readback");
		recorded_.colorClear = recorded_.depthClear = false;
		recorded_.colorState = slot.colorState; // Still RENDER_TARGET.
		serial_ = nextSerial_++;
		try
		{
			renderer_->beginFrame(currentFrame());
		}
		catch (...)
		{
			commands_->Close();
			recording_ = false;
			throw;
		}
	}

	void waitForSerial(std::uint64_t serial)
	{
		if (!serial || fence_->GetCompletedValue() >= serial)
			return;
		check(fence_->SetEventOnCompletion(serial, fenceEvent_.value), "arm the frame fence event");
		for (;;)
		{
			const DWORD status = WaitForSingleObjectEx(fenceEvent_.value, 5000, FALSE);
			if (status == WAIT_OBJECT_0)
				return;
			if (status != WAIT_TIMEOUT)
				throw std::runtime_error("waiting on the Direct3D 12 frame fence failed");
			// A long stall is not proof of a hang. A real hang ends in TDR, which
			// removes the device and is reported here instead of waiting forever.
			const HRESULT removed = device_->GetDeviceRemovedReason();
			if (FAILED(removed))
				throw std::runtime_error("Direct3D 12 device removed while waiting for serial " +
										 std::to_string(serial) + ": " + hresult(removed));
		}
	}

	void waitForIdle()
	{
		// Present can enqueue work after the render submission's fence signal.
		// A fresh marker also retires those operations before ResizeBuffers.
		const std::uint64_t serial = nextSerial_++;
		check(queue_->Signal(fence_.Get(), serial), "signal queue-idle marker");
		submitted_ = serial;
		waitForSerial(serial);
	}

	Query &queryAt(std::uint32_t id)
	{
		auto found = queries_.find(id);
		if (found == queries_.end())
			throw std::out_of_range("unknown occlusion query name");
		return found->second;
	}

	unsigned acquireQuerySlot()
	{
		if (freeQuerySlots_.empty())
			retireQueries();
		if (freeQuerySlots_.empty())
			growQueryPool();
		const unsigned slot = freeQuerySlots_.back();
		freeQuerySlots_.pop_back();
		return slot;
	}

	void growQueryPool()
	{
		QueryPool pool;
		D3D12_QUERY_HEAP_DESC desc{};
		desc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		desc.Count = kQueryPoolSize;
		check(device_->CreateQueryHeap(&desc, IID_PPV_ARGS(&pool.heap)), "create occlusion query heap");
		pool.readback = makeReadbackBuffer(std::uint64_t(kQueryPoolSize) * kQueryBytes);
		const unsigned base = unsigned(queryPools_.size()) * kQueryPoolSize;
		queryPools_.push_back(std::move(pool));
		freeQuerySlots_.reserve(freeQuerySlots_.size() + kQueryPoolSize);
		for (unsigned i = kQueryPoolSize; i-- > 0;)
			freeQuerySlots_.push_back(base + i);
	}

	// A slot returns to circulation only once the GPU has passed the resolve that
	// was recorded into it, so a deleted name can never hand live storage away.
	void retireQueries()
	{
		if (retiringQuerySlots_.empty())
			return;
		const std::uint64_t completed = fence_->GetCompletedValue();
		std::size_t keep = 0;
		for (std::size_t i = 0; i < retiringQuerySlots_.size(); ++i)
		{
			if (retiringQuerySlots_[i].serial <= completed)
				freeQuerySlots_.push_back(retiringQuerySlots_[i].slot);
			else
				retiringQuerySlots_[keep++] = retiringQuerySlots_[i];
		}
		retiringQuerySlots_.resize(keep);
	}

	bool sampleQuery(const Query &query)
	{
		QueryPool &pool = queryPools_.at(query.slot / kQueryPoolSize);
		const std::uint64_t offset = std::uint64_t(query.slot % kQueryPoolSize) * kQueryBytes;
		// Map returns the resource base; the range only tells the runtime which
		// bytes the CPU is about to read out of the readback heap.
		D3D12_RANGE range{SIZE_T(offset), SIZE_T(offset + kQueryBytes)};
		void *mapped = nullptr;
		check(pool.readback->Map(0, &range, &mapped), "map occlusion query readback");
		std::uint64_t samples = 0;
		std::memcpy(&samples, static_cast<const std::uint8_t *>(mapped) + offset, sizeof(samples));
		const D3D12_RANGE noWrite{0, 0};
		pool.readback->Unmap(0, &noWrite);
		return samples != 0;
	}

	ComPtr<ID3D12Resource> makeReadbackBuffer(std::uint64_t bytes)
	{
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_READBACK;
		heap.CreationNodeMask = heap.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = bytes;
		desc.Height = 1;
		desc.DepthOrArraySize = desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ComPtr<ID3D12Resource> resource;
		// Readback heaps are created in COPY_DEST, which is the state
		// ResolveQueryData requires, and are never transitioned.
		check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
											   nullptr, IID_PPV_ARGS(&resource)),
			  "create occlusion query readback buffer");
		return resource;
	}

	void clientSize(int *width, int *height) const
	{
		if (IsIconic(hwnd_))
		{
			*width = *height = 0;
			return;
		}
		RECT client{};
		// The client rect is what DXGI sizes the swapchain against; a minimized
		// window reports an empty one.
		if (GetClientRect(hwnd_, &client))
		{
			const LONG w = client.right - client.left, h = client.bottom - client.top;
			if (w > 0 && h > 0)
			{
				*width = int(w);
				*height = int(h);
				return;
			}
		}
		*width = 0;
		*height = 0;
	}

	void initialSize(int *width, int *height) const
	{
		clientSize(width, height);
		if (*width > 0 && *height > 0)
			return;
		// The window may still be hidden or minimized at startup; the swapchain
		// needs a positive extent now and beginFrame resizes it to the real
		// client area as soon as one exists.
		SDL_GetWindowSize(window_, width, height);
		if (*width <= 0)
			*width = 1;
		if (*height <= 0)
			*height = 1;
	}

	void describe()
	{
		std::ostringstream s;
		s << "Direct3D 12 feature level " << featureLevelName(supportedFeatureLevel()) << " on " << adapterName_ << "; "
		  << formatName(colorFormat_) << " flip-discard swapchain, " << kFrameCount << " buffers, "
		  << formatName(depthFormat_) << " depth; HLSL shader model 5.0 compiled at run time by D3DCompile";
		if (!validation_.empty())
			s << "; " << validation_;
		description_ = s.str();
	}
};

std::unique_ptr<BackendHost> createD3D12Host(SDL_Window *window)
{
	return std::unique_ptr<BackendHost>(new D3D12Host(window));
}
} // namespace render
} // namespace b173
