#include "client/renderer/backend/d3d12/D3D12Device.h"

#include "client/renderer/portable/FrameLifetime.h"
#include "client/renderer/portable/LegacyState.h"
#include "client/renderer/portable/ShaderSource.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

namespace b173
{
namespace render
{
namespace D3D12Detail
{
using Microsoft::WRL::ComPtr;

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

// std::clamp is C++17; this backend is compiled at C++14 with the rest of the game.
static LONG clampLong(std::int64_t value, std::int64_t low, std::int64_t high)
{
	return LONG(value < low ? low : (value > high ? high : value));
}

static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type)
{
	D3D12_HEAP_PROPERTIES h{};
	h.Type = type;
	h.CreationNodeMask = h.VisibleNodeMask = 1;
	return h;
}

static D3D12_RESOURCE_DESC bufferDesc(std::uint64_t bytes)
{
	D3D12_RESOURCE_DESC d{};
	d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d.Width = bytes;
	d.Height = 1;
	d.DepthOrArraySize = d.MipLevels = 1;
	d.SampleDesc.Count = 1;
	d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	return d;
}

struct Buffer
{
	ComPtr<ID3D12Resource> resource;
	std::uint64_t size = 0;
	void *mapped = nullptr;

	~Buffer()
	{
		if (mapped)
			resource->Unmap(0, nullptr);
	}
};

static std::shared_ptr<Buffer> makeBuffer(ID3D12Device *device, std::uint64_t bytes, D3D12_HEAP_TYPE type)
{
	auto b = std::make_shared<Buffer>();
	b->size = bytes;
	const auto h = heap(type);
	const auto d = bufferDesc(bytes);
	// Upload heaps are created GENERIC_READ and readback heaps COPY_DEST; both
	// are fixed for the resource's lifetime and must never be transitioned.
	const auto state =
		type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
	check(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&b->resource)),
		  "create buffer");
	if (type == D3D12_HEAP_TYPE_UPLOAD)
	{
		const D3D12_RANGE noRead{0, 0};
		check(b->resource->Map(0, &noRead, &b->mapped), "map upload buffer");
	}
	return b;
}

static void transition(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
					   D3D12_RESOURCE_STATES after)
{
	if (before == after)
		return;
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = resource;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	b.Transition.StateBefore = before;
	b.Transition.StateAfter = after;
	commands->ResourceBarrier(1, &b);
}

struct GeometryPage
{
	std::shared_ptr<Buffer> buffer;
	RangeAllocator free;
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;

	explicit GeometryPage(std::shared_ptr<Buffer> b) : buffer(std::move(b)), free(buffer->size) {}
};

// A slice either owns a range inside a device-local arena page or borrows one out
// of the frame's upload page for immediate geometry. It always holds the buffer it
// draws from, so borrowed geometry needs no arena bookkeeping at all, and holding
// the page is exactly what ownership of the range means.
struct Slice
{
	std::shared_ptr<Buffer> buffer;
	std::shared_ptr<GeometryPage> page;
	ByteRange range;
	std::uint64_t indexOffset = 0;
	std::uint32_t count = 0, vertexBytes = 0;

	~Slice()
	{
		if (page)
			page->free.release(range);
	}
};

struct Mesh
{
	MeshData source;
	std::shared_ptr<Slice> smooth, flat;
};

struct Texture
{
	TextureDesc desc;
	ComPtr<ID3D12Resource> resource;
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;

	struct Update
	{
		Rect region;
		std::vector<std::uint8_t> pixels;
	};

	std::vector<Update> pending;
};

struct UploadPage
{
	std::shared_ptr<Buffer> buffer;
	std::uint64_t used = 0;
};

struct Upload
{
	std::shared_ptr<Buffer> buffer;
	std::uint64_t offset = 0;
	void *pointer = nullptr;
};

struct Descriptors
{
	D3D12_GPU_DESCRIPTOR_HANDLE texture{}, sampler{};
};

struct Frame
{
	ComPtr<ID3D12DescriptorHeap> textures, samplers;
	std::vector<UploadPage> uploads;
	std::unordered_map<TextureId, Descriptors> bindings;
	std::map<std::array<unsigned, 4>, D3D12_GPU_DESCRIPTOR_HANDLE> samplerBindings;
	unsigned textureCount = 0, samplerCount = 0;
};

static D3D12_BLEND blend(BlendFactor factor, bool alpha)
{
	// D3D12 rejects the color-only factors in the alpha slot. GL's alpha channel
	// already uses the alpha component of those factors, so this substitution is
	// the same arithmetic, not an approximation. GL_SRC_ALPHA_SATURATE is defined
	// as 1 for the alpha channel, which D3D12's SRC_ALPHA_SAT is not.
	if (alpha)
	{
		if (factor == BlendFactor::SrcColor)
			factor = BlendFactor::SrcAlpha;
		if (factor == BlendFactor::OneMinusSrcColor)
			factor = BlendFactor::OneMinusSrcAlpha;
		if (factor == BlendFactor::DstColor)
			factor = BlendFactor::DstAlpha;
		if (factor == BlendFactor::OneMinusDstColor)
			factor = BlendFactor::OneMinusDstAlpha;
		if (factor == BlendFactor::SrcAlphaSaturate)
			factor = BlendFactor::One;
	}
	// Declared in BlendFactor order, not D3D12_BLEND numeric order.
	static const D3D12_BLEND values[] = {
		D3D12_BLEND_ZERO,			D3D12_BLEND_ONE,		   D3D12_BLEND_SRC_COLOR,
		D3D12_BLEND_INV_SRC_COLOR,	D3D12_BLEND_DEST_COLOR,	   D3D12_BLEND_INV_DEST_COLOR,
		D3D12_BLEND_SRC_ALPHA,		D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA,
		D3D12_BLEND_INV_DEST_ALPHA, D3D12_BLEND_SRC_ALPHA_SAT};
	return values[unsigned(factor)];
}

static D3D12_BLEND_OP blendOp(BlendOp op)
{
	static const D3D12_BLEND_OP values[] = {D3D12_BLEND_OP_ADD, D3D12_BLEND_OP_SUBTRACT, D3D12_BLEND_OP_REV_SUBTRACT};
	return values[unsigned(op)];
}

static D3D12_RECT scissor(Rect r, int width, int height)
{
	const auto t = topLeftRect(r, height);
	const LONG l = clampLong(t.x, 0, width), top = clampLong(t.y, 0, height);
	const LONG right = clampLong(std::int64_t(t.x) + t.width, 0, width);
	const LONG bottom = clampLong(std::int64_t(t.y) + t.height, 0, height);
	D3D12_RECT out;
	out.left = l;
	out.top = top;
	out.right = std::max(l, right);
	out.bottom = std::max(top, bottom);
	return out;
}
} // namespace D3D12Detail

struct D3D12Readback::Data
{
	D3D12Detail::ComPtr<ID3D12Resource> resource;
	ID3D12Device *owner = nullptr;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	std::uint64_t serial = 0, bytes = 0;
	int width = 0, height = 0;
	bool bgra = false;
};

using namespace D3D12Detail;

struct D3D12Device::Impl
{
	D3D12CreateInfo info;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12RootSignature> root;
	ComPtr<ID3DBlob> vertexCode, fragmentCode;
	std::unordered_map<PipelineKey, ComPtr<ID3D12PipelineState>, PipelineKeyHash> pipelines;
	std::vector<std::shared_ptr<GeometryPage>> geometry;
	std::unordered_map<MeshId, std::shared_ptr<Mesh>> meshes;
	std::unordered_map<TextureId, std::shared_ptr<Texture>> textures;
	std::vector<Frame> frames;
	FrameLifetime lifetime;
	D3D12Frame current{};
	UINT textureIncrement = 0, samplerIncrement = 0;
	MeshId nextMesh = 1;
	TextureId nextTexture = 1;
	MeshData flatScratch;

	explicit Impl(const D3D12CreateInfo &ci)
		: info(ci), device(ci.device), frames(ci.framesInFlight), lifetime(ci.framesInFlight)
	{
		if (!device)
			throw std::invalid_argument("D3D12 device is required");
		if (ci.colorFormat != DXGI_FORMAT_R8G8B8A8_UNORM && ci.colorFormat != DXGI_FORMAT_B8G8R8A8_UNORM)
			throw std::invalid_argument("linear RGBA8/BGRA8 render target required, not sRGB");
		if (ci.depthFormat != DXGI_FORMAT_D24_UNORM_S8_UINT && ci.depthFormat != DXGI_FORMAT_D32_FLOAT)
			throw std::invalid_argument("explicit D24 or D32 required; no silent D16 fallback");
		if (!ci.maxTexturesPerFrame || ci.maxTexturesPerFrame > 2048 || !ci.geometryPageBytes || !ci.uploadPageBytes)
			throw std::invalid_argument("invalid D3D12 descriptor/arena limits");
		textureIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		samplerIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		makeRoot();
		// The shared generator is the single source of shader truth; there is no
		// precompiled blob and no archived HLSL snapshot in this path.
		const auto source = hlslShader();
		vertexCode = compile(source, "vsMain", "vs_5_0");
		fragmentCode = compile(source, "psMain", "ps_5_0");
		for (auto &frame : frames)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc{};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = ci.maxTexturesPerFrame;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&frame.textures)), "create frame SRV heap");
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&frame.samplers)), "create frame sampler heap");
		}
		TextureDesc white;
		white.width = white.height = 1;
		textures.emplace(0, makeTexture(white, {255, 255, 255, 255}));
	}

	ComPtr<ID3DBlob> compile(const std::string &source, const char *entry, const char *profile)
	{
		ComPtr<ID3DBlob> code, errors;
		const HRESULT result =
			D3DCompile(source.data(), source.size(), "b173-fixed-function", nullptr, nullptr, entry, profile,
					   D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_IEEE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
					   &code, &errors);
		if (FAILED(result))
		{
			const std::string detail =
				errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
					   : "no compiler diagnostics";
			throw std::runtime_error(std::string(entry) + ": " + detail);
		}
		return code;
	}

	void makeRoot()
	{
		D3D12_DESCRIPTOR_RANGE ranges[2]{};
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[0].NumDescriptors = 1;
		ranges[0].BaseShaderRegister = 0;
		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		ranges[1].NumDescriptors = 1;
		ranges[1].BaseShaderRegister = 0;
		D3D12_ROOT_PARAMETER parameters[3]{};
		parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameters[0].Descriptor.ShaderRegister = 0;
		parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		for (unsigned i = 0; i < 2; ++i)
		{
			parameters[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameters[i + 1].DescriptorTable.NumDescriptorRanges = 1;
			parameters[i + 1].DescriptorTable.pDescriptorRanges = &ranges[i];
			parameters[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = 3;
		desc.pParameters = parameters;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
					 D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
					 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
					 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		ComPtr<ID3DBlob> blob, errors;
		const HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
		if (FAILED(result))
			throw std::runtime_error(
				errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
					   : "serialize root signature failed: " + hresult(result));
		check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)),
			  "create root signature");
	}

	Frame &frame()
	{
		return frames.at(lifetime.slot());
	}

	void requireFrame()
	{
		if (!lifetime.active())
			throw std::logic_error("D3D12 operation requires beginFrame");
	}

	Upload upload(std::uint64_t bytes, std::uint64_t alignment)
	{
		auto &f = frame();
		for (auto &page : f.uploads)
		{
			const auto offset = alignBytes(page.used, alignment);
			if (offset <= page.buffer->size && bytes <= page.buffer->size - offset)
			{
				page.used = offset + bytes;
				return {page.buffer, offset, static_cast<std::uint8_t *>(page.buffer->mapped) + offset};
			}
		}
		auto b = makeBuffer(device.Get(), std::max<std::uint64_t>(info.uploadPageBytes, alignBytes(bytes, alignment)),
							D3D12_HEAP_TYPE_UPLOAD);
		f.uploads.push_back({b, bytes});
		return {b, 0, b->mapped};
	}

	std::shared_ptr<Slice> uploadMesh(const MeshData &mesh)
	{
		requireFrame();
		const std::uint64_t vertexBytes = mesh.vertices.size() * sizeof(Vertex);
		const std::uint64_t indexOffset = alignBytes(vertexBytes, 4);
		const std::uint64_t total = indexOffset + mesh.indices.size() * 4;
		if (vertexBytes > UINT_MAX || mesh.indices.size() * 4 > UINT_MAX)
			throw std::length_error("D3D12 buffer view exceeds 32-bit size");
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
			page = std::make_shared<GeometryPage>(
				makeBuffer(device.Get(), std::max<std::uint64_t>(info.geometryPageBytes, alignBytes(total, 16)),
						   D3D12_HEAP_TYPE_DEFAULT));
			geometry.push_back(page);
			if (!page->free.allocate(total, 16, range))
				throw std::logic_error("fresh geometry arena allocation failed");
		}
		auto slice = std::make_shared<Slice>();
		slice->buffer = page->buffer;
		slice->page = page;
		slice->range = range;
		slice->indexOffset = indexOffset;
		slice->count = std::uint32_t(mesh.indices.size());
		slice->vertexBytes = std::uint32_t(vertexBytes);
		auto stage = upload(total, 16);
		std::memcpy(stage.pointer, mesh.vertices.data(), std::size_t(vertexBytes));
		std::memcpy(static_cast<std::uint8_t *>(stage.pointer) + indexOffset, mesh.indices.data(),
					mesh.indices.size() * 4);
		transition(current.commands, page->buffer->resource.Get(), page->state, D3D12_RESOURCE_STATE_COPY_DEST);
		current.commands->CopyBufferRegion(page->buffer->resource.Get(), range.offset, stage.buffer->resource.Get(),
										   stage.offset, total);
		const D3D12_RESOURCE_STATES drawState =
			D3D12_RESOURCE_STATES(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER);
		transition(current.commands, page->buffer->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, drawState);
		page->state = drawState;
		return slice;
	}

	std::shared_ptr<Texture> makeTexture(const TextureDesc &desc, std::vector<std::uint8_t> pixels)
	{
		auto t = std::make_shared<Texture>();
		t->desc = desc;
		D3D12_RESOURCE_DESC image{};
		image.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		image.Width = std::uint64_t(desc.width);
		image.Height = UINT(desc.height);
		image.DepthOrArraySize = image.MipLevels = 1;
		image.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		image.SampleDesc.Count = 1;
		image.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		const auto h = heap(D3D12_HEAP_TYPE_DEFAULT);
		check(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &image, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
											  IID_PPV_ARGS(&t->resource)),
			  "create texture");
		t->pending.push_back({{0, 0, desc.width, desc.height}, std::move(pixels)});
		return t;
	}

	void textureCopy(const std::shared_ptr<Texture> &texture, Rect region, const std::vector<std::uint8_t> &pixels)
	{
		requireFrame();
		lifetime.retain(texture);
		// The upload layout comes from GetCopyableFootprints, never from an
		// assumed width * 4 row: D3D12 pads staging rows to 256 bytes.
		auto uploadDesc = texture->resource->GetDesc();
		uploadDesc.Width = UINT64(region.width);
		uploadDesc.Height = UINT(region.height);
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		UINT rows = 0;
		UINT64 rowBytes = 0, total = 0;
		device->GetCopyableFootprints(&uploadDesc, 0, 1, 0, &footprint, &rows, &rowBytes, &total);
		if (total == UINT64_MAX || rowBytes != UINT64(region.width) * 4 || rows != UINT(region.height))
			throw std::runtime_error("unexpected RGBA texture footprint");
		auto stage = upload(total, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
		for (UINT y = 0; y < rows; ++y)
			std::memcpy(static_cast<std::uint8_t *>(stage.pointer) + std::size_t(y) * footprint.Footprint.RowPitch,
						pixels.data() + std::size_t(y) * std::size_t(rowBytes), std::size_t(rowBytes));
		footprint.Offset += stage.offset;
		transition(current.commands, texture->resource.Get(), texture->state, D3D12_RESOURCE_STATE_COPY_DEST);
		D3D12_TEXTURE_COPY_LOCATION source{};
		source.pResource = stage.buffer->resource.Get();
		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		source.PlacedFootprint = footprint;
		D3D12_TEXTURE_COPY_LOCATION destination{};
		destination.pResource = texture->resource.Get();
		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		current.commands->CopyTextureRegion(&destination, UINT(region.x), UINT(region.y), 0, &source, nullptr);
		transition(current.commands, texture->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
				   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		texture->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	void ensureTexture(const std::shared_ptr<Texture> &texture)
	{
		for (auto &pending : texture->pending)
			textureCopy(texture, pending.region, pending.pixels);
		texture->pending.clear();
	}

	Descriptors descriptors(TextureId id, const std::shared_ptr<Texture> &texture)
	{
		auto &f = frame();
		auto found = f.bindings.find(id);
		if (found != f.bindings.end())
			return found->second;
		if (f.textureCount == info.maxTexturesPerFrame)
			throw std::length_error("frame SRV heap exhausted; never overwrite live descriptors");
		auto cpu = f.textures->GetCPUDescriptorHandleForHeapStart();
		cpu.ptr += SIZE_T(f.textureCount) * textureIncrement;
		auto gpu = f.textures->GetGPUDescriptorHandleForHeapStart();
		gpu.ptr += UINT64(f.textureCount++) * textureIncrement;
		D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
		srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(texture->resource.Get(), &srv, cpu);
		const auto &d = texture->desc;
		// GL_CLAMP and GL_CLAMP_TO_EDGE both use the clamping sampler; the shader
		// restores GL_CLAMP's border contribution, so they share one entry.
		const std::array<unsigned, 4> key{{unsigned(d.minFilter), unsigned(d.magFilter),
										   unsigned(d.wrapS == Wrap::Repeat), unsigned(d.wrapT == Wrap::Repeat)}};
		auto sampler = f.samplerBindings.find(key);
		if (sampler == f.samplerBindings.end())
		{
			if (f.samplerCount == info.maxTexturesPerFrame)
				throw std::length_error("frame sampler heap exhausted");
			auto samplerCPU = f.samplers->GetCPUDescriptorHandleForHeapStart();
			samplerCPU.ptr += SIZE_T(f.samplerCount) * samplerIncrement;
			auto samplerGPU = f.samplers->GetGPUDescriptorHandleForHeapStart();
			samplerGPU.ptr += UINT64(f.samplerCount++) * samplerIncrement;
			D3D12_SAMPLER_DESC desc{};
			if (d.minFilter == Filter::Linear)
				desc.Filter = d.magFilter == Filter::Linear ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
															: D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			else
				desc.Filter = d.magFilter == Filter::Linear ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT
															: D3D12_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU =
				d.wrapS == Wrap::Repeat ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			desc.AddressV =
				d.wrapT == Wrap::Repeat ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.MaxAnisotropy = 1;
			desc.MinLOD = desc.MaxLOD = 0;
			device->CreateSampler(&desc, samplerCPU);
			sampler = f.samplerBindings.emplace(key, samplerGPU).first;
		}
		Descriptors result{gpu, sampler->second};
		f.bindings.emplace(id, result);
		return result;
	}

	ID3D12PipelineState *pipeline(const PipelineState &p, Primitive primitive)
	{
		if (primitive == Primitive::Lines && p.lineWidth != 1)
			throw std::runtime_error("D3D12 has no GL wide-line rasterizer; wide-line geometry adapter is required");
		if (p.polygonOffset &&
			(std::trunc(p.offsetUnits) != p.offsetUnits || double(p.offsetUnits) < std::numeric_limits<INT>::min() ||
			 double(p.offsetUnits) > std::numeric_limits<INT>::max()))
			throw std::runtime_error(
				"fractional/out-of-range GL polygon offset cannot map to D3D12 integer depth bias");
		const auto key = pipelineKey(p, primitive);
		auto found = pipelines.find(key);
		if (found != pipelines.end())
			return found->second.Get();
		D3D12_INPUT_ELEMENT_DESC elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			// The signed normal bytes are decoded in the shader, exactly as on the
			// other backends: UNORM here, never SNORM, whose zero point differs.
			{"NORMAL", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = root.Get();
		desc.InputLayout.pInputElementDescs = elements;
		desc.InputLayout.NumElements = 4;
		desc.VS.pShaderBytecode = vertexCode->GetBufferPointer();
		desc.VS.BytecodeLength = vertexCode->GetBufferSize();
		desc.PS.pShaderBytecode = fragmentCode->GetBufferPointer();
		desc.PS.BytecodeLength = fragmentCode->GetBufferSize();
		auto &raster = desc.RasterizerState;
		raster.FillMode = D3D12_FILL_MODE_SOLID;
		// GL_FRONT_AND_BACK discards every triangle but still rasterizes lines and
		// points, so Cull::Both is culled by the caller's early-out in draw() and
		// must not become a triangle cull mode here.
		raster.CullMode = p.cull == Cull::Front	 ? D3D12_CULL_MODE_FRONT
						  : p.cull == Cull::Back ? D3D12_CULL_MODE_BACK
												 : D3D12_CULL_MODE_NONE;
		// frontCCW passes straight through. All three APIs name the winding as a
		// viewer sees it, not as the raw signed area in a downward-Y target comes
		// out, so the mapping is an identity wherever the displayed image already
		// matches GL. Here it does: D3D12's viewport transform is
		// y_target = TopLeftY + (1 - y_ndc) / 2 * Height, which puts clip +Y on
		// the top row exactly as in GL, so the shared shader applies no Y flip
		// (uV[30].y stays 0). And FrontCounterClockwise = FALSE is D3D's own
		// clockwise-front convention, the opposite of GL's naming: the D3D12
		// default rasterizer state culls back faces yet still draws the canonical
		// top / bottom-right / bottom-left sample triangle, which is clockwise on
		// screen, and DirectXTK spells CULL_BACK with FALSE "CullCounterClockwise".
		// TRUE therefore means "front is counter-clockwise as displayed", which is
		// precisely glFrontFace(GL_CCW). Inverting the flag here, on the theory
		// that a top-left target origin reverses the winding, would cull every
		// face the game keeps: that origin is already accounted for twice over.
		raster.FrontCounterClockwise = p.frontCCW ? TRUE : FALSE;
		raster.DepthClipEnable = TRUE;
		// D3D12 computes DepthBias * r + SlopeScaledDepthBias * maxDepthSlope with
		// r the smallest representable step of the UNORM depth target, which is
		// precisely GL's polygon-offset unit. Both terms therefore pass through.
		raster.DepthBias = p.polygonOffset ? INT(p.offsetUnits) : 0;
		raster.SlopeScaledDepthBias = p.polygonOffset ? p.offsetFactor : 0;
		auto &blendState = desc.BlendState.RenderTarget[0];
		blendState.BlendEnable = p.blend ? TRUE : FALSE;
		blendState.SrcBlend = blend(p.srcRGB, false);
		blendState.DestBlend = blend(p.dstRGB, false);
		blendState.BlendOp = blendOp(p.rgbOp);
		blendState.SrcBlendAlpha = blend(p.srcAlpha, true);
		blendState.DestBlendAlpha = blend(p.dstAlpha, true);
		blendState.BlendOpAlpha = blendOp(p.alphaOp);
		blendState.LogicOp = D3D12_LOGIC_OP_NOOP;
		// D3D12_COLOR_WRITE_ENABLE_RED/GREEN/BLUE/ALPHA are 1/2/4/8, the same bit
		// assignment as PipelineState::colorMask.
		blendState.RenderTargetWriteMask = p.colorMask;
		auto &depth = desc.DepthStencilState;
		depth.DepthEnable = p.depthTest ? TRUE : FALSE;
		depth.DepthWriteMask = p.depthTest && p.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		// Compare is declared in GL_NEVER..GL_ALWAYS order and
		// D3D12_COMPARISON_FUNC_NEVER is 1, so the offset is exact.
		depth.DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(unsigned(p.depthCompare) + 1);
		depth.StencilReadMask = depth.StencilWriteMask = 0xff;
		depth.FrontFace.StencilFailOp = depth.FrontFace.StencilDepthFailOp = depth.FrontFace.StencilPassOp =
			D3D12_STENCIL_OP_KEEP;
		depth.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		depth.BackFace = depth.FrontFace;
		desc.SampleMask = UINT_MAX;
		desc.SampleDesc.Count = 1;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = info.colorFormat;
		desc.DSVFormat = info.depthFormat;
		desc.PrimitiveTopologyType = primitive == Primitive::Triangles ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
									 : primitive == Primitive::Lines   ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
																	   : D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		ComPtr<ID3D12PipelineState> result;
		check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result)), "create legacy graphics pipeline");
		auto *pointer = result.Get();
		pipelines.emplace(key, std::move(result));
		return pointer;
	}

	void drawSlice(const MeshData &source, const Slice &slice, const Draw &d)
	{
		const auto &texture = textures.at(d.texture);
		ensureTexture(texture);
		lifetime.retain(texture);
		lifetime.retain(slice.buffer);
		Uniforms u = d.uniforms;
		applyClipConvention(u, ClipConvention::Direct3D);
		applyMeshUniforms(u, source);
		applyTextureUniforms(u, texture->desc);
		auto constants = upload(sizeof(u), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		std::memcpy(constants.pointer, &u, sizeof(u));
		const auto views = descriptors(d.texture, texture);
		ID3D12GraphicsCommandList *commands = current.commands;
		commands->SetPipelineState(pipeline(d.pipeline, source.primitive));
		commands->SetGraphicsRootConstantBufferView(0, constants.buffer->resource->GetGPUVirtualAddress() +
														   constants.offset);
		commands->SetGraphicsRootDescriptorTable(1, views.texture);
		commands->SetGraphicsRootDescriptorTable(2, views.sampler);
		const Rect viewport = topLeftRect(d.pipeline.viewport, current.height);
		D3D12_VIEWPORT v{float(viewport.x), float(viewport.y), float(viewport.width), float(viewport.height), 0, 1};
		commands->RSSetViewports(1, &v);
		const D3D12_RECT r = D3D12Detail::scissor(d.pipeline.scissor ? d.pipeline.scissorRect
																	 : Rect{0, 0, current.width, current.height},
												  current.width, current.height);
		commands->RSSetScissorRects(1, &r);
		const D3D12_GPU_VIRTUAL_ADDRESS address = slice.buffer->resource->GetGPUVirtualAddress() + slice.range.offset;
		D3D12_VERTEX_BUFFER_VIEW vb{address, slice.vertexBytes, sizeof(Vertex)};
		D3D12_INDEX_BUFFER_VIEW ib{address + slice.indexOffset, slice.count * 4, DXGI_FORMAT_R32_UINT};
		commands->IASetVertexBuffers(0, 1, &vb);
		commands->IASetIndexBuffer(&ib);
		commands->IASetPrimitiveTopology(source.primitive == Primitive::Triangles ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
										 : source.primitive == Primitive::Lines	  ? D3D_PRIMITIVE_TOPOLOGY_LINELIST
																				  : D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		commands->DrawIndexedInstanced(slice.count, 1, 0, 0, 0);
	}
};

D3D12Device::D3D12Device(const D3D12CreateInfo &info) : impl_(std::unique_ptr<Impl>(new Impl(info))) {}

D3D12Device::~D3D12Device() = default;

void D3D12Device::beginFrame(const D3D12Frame &frame)
{
	auto &i = *impl_;
	if (!frame.commands || !frame.colorResource || !frame.colorRTV.ptr || !frame.depthDSV.ptr || frame.width <= 0 ||
		frame.height <= 0)
		throw std::invalid_argument("invalid D3D12 frame target");
	const auto desc = frame.colorResource->GetDesc();
	if (desc.Format != i.info.colorFormat || desc.SampleDesc.Count != 1 || desc.Width != UINT64(frame.width) ||
		desc.Height != UINT(frame.height))
		throw std::invalid_argument("D3D12 target format/extent mismatch");
	i.lifetime.begin(frame.slot, frame.serial, frame.completedSerial);
	i.current = frame;
	auto &f = i.frame();
	f.bindings.clear();
	f.samplerBindings.clear();
	f.textureCount = f.samplerCount = 0;
	for (auto &page : f.uploads)
		page.used = 0;
	frame.commands->OMSetRenderTargets(1, &frame.colorRTV, FALSE, &frame.depthDSV);
	ID3D12DescriptorHeap *heaps[] = {f.textures.Get(), f.samplers.Get()};
	frame.commands->SetDescriptorHeaps(2, heaps);
	frame.commands->SetGraphicsRootSignature(i.root.Get());
}

void D3D12Device::endFrame()
{
	impl_->requireFrame();
	impl_->lifetime.end();
}

MeshId D3D12Device::createMesh(const MeshData &source)
{
	validateMesh(source);
	auto m = std::make_shared<Mesh>();
	m->source = source;
	auto &i = *impl_;
	const auto id = i.nextMesh++;
	i.meshes.emplace(id, std::move(m));
	return id;
}

void D3D12Device::destroyMesh(MeshId id)
{
	if (!impl_->meshes.erase(id))
		throw std::out_of_range("unknown D3D12 mesh");
}

TextureId D3D12Device::createTexture(const TextureDesc &desc, const void *rgba, std::size_t bytes, std::size_t stride)
{
	validateTexture(desc, false);
	auto data = unpackRGBA(rgba, bytes, desc.width, desc.height, stride);
	auto &i = *impl_;
	auto texture = i.makeTexture(desc, std::move(data));
	const auto id = i.nextTexture++;
	i.textures.emplace(id, std::move(texture));
	return id;
}

void D3D12Device::updateTexture(TextureId id, Rect r, const void *rgba, std::size_t bytes, std::size_t stride)
{
	auto &i = *impl_;
	auto texture = i.textures.at(id);
	if (!id || r.x < 0 || r.y < 0 || std::int64_t(r.x) + r.width > texture->desc.width ||
		std::int64_t(r.y) + r.height > texture->desc.height)
		throw std::out_of_range("texture subimage rectangle");
	auto data = unpackRGBA(rgba, bytes, r.width, r.height, stride);
	if (i.lifetime.active())
	{
		i.ensureTexture(texture);
		i.textureCopy(texture, r, data);
	}
	else
	{
		texture->pending.push_back({r, std::move(data)});
	}
}

void D3D12Device::destroyTexture(TextureId id)
{
	if (!id)
		throw std::invalid_argument("cannot destroy internal white texture");
	if (!impl_->textures.erase(id))
		throw std::out_of_range("unknown D3D12 texture");
}

void D3D12Device::draw(const Draw &d)
{
	auto &i = *impl_;
	i.requireFrame();
	validatePipeline(d.pipeline);
	const auto &mesh = i.meshes.at(d.mesh);
	if (!d.pipeline.viewport.width || !d.pipeline.viewport.height ||
		(mesh->source.primitive == Primitive::Triangles && d.pipeline.cull == Cull::Both))
		return;
	auto &slice = d.pipeline.flat ? mesh->flat : mesh->smooth;
	if (!slice)
	{
		if (d.pipeline.flat)
		{
			flatMesh(mesh->source, i.flatScratch);
			slice = i.uploadMesh(i.flatScratch);
		}
		else
			slice = i.uploadMesh(mesh->source);
	}
	i.lifetime.retain(mesh);
	i.drawSlice(mesh->source, *slice, d);
}

void D3D12Device::drawTransient(const MeshData &input, const Draw &state)
{
	auto &i = *impl_;
	i.requireFrame();
	validateMesh(input);
	validatePipeline(state.pipeline);
	if (!state.pipeline.viewport.width || !state.pipeline.viewport.height ||
		(input.primitive == Primitive::Triangles && state.pipeline.cull == Cull::Both))
		return;
	const MeshData *source = &input;
	if (state.pipeline.flat)
	{
		flatMesh(input, i.flatScratch);
		source = &i.flatScratch;
	}
	const std::uint64_t vertexBytes = source->vertices.size() * sizeof(Vertex);
	const std::uint64_t indexOffset = alignBytes(vertexBytes, 4);
	if (vertexBytes > UINT_MAX || source->indices.size() * 4 > UINT_MAX)
		throw std::length_error("transient buffer view too large");
	const std::uint64_t total = indexOffset + source->indices.size() * 4;
	auto upload = i.upload(total, 16);
	std::memcpy(upload.pointer, source->vertices.data(), std::size_t(vertexBytes));
	std::memcpy(static_cast<std::uint8_t *>(upload.pointer) + indexOffset, source->indices.data(),
				source->indices.size() * 4);
	// Immediate geometry is drawn straight out of the frame's upload page, which
	// the frame lease already keeps alive; the slice borrows that range and owns
	// no arena page, so no free list is touched for a transient draw.
	Slice slice;
	slice.buffer = upload.buffer;
	slice.range = {upload.offset, total};
	slice.indexOffset = indexOffset;
	slice.count = std::uint32_t(source->indices.size());
	slice.vertexBytes = std::uint32_t(vertexBytes);
	Draw d = state;
	d.pipeline.flat = false;
	i.drawSlice(*source, slice, d);
}

D3D12Readback D3D12Device::recordReadback()
{
	auto &i = *impl_;
	i.requireFrame();
	auto ticket = std::make_shared<D3D12Readback::Data>();
	ticket->owner = i.device.Get();
	ticket->serial = i.lifetime.serial();
	ticket->width = i.current.width;
	ticket->height = i.current.height;
	ticket->bgra = i.info.colorFormat == DXGI_FORMAT_B8G8R8A8_UNORM;
	const auto desc = i.current.colorResource->GetDesc();
	UINT rows = 0;
	UINT64 rowBytes = 0;
	i.device->GetCopyableFootprints(&desc, 0, 1, 0, &ticket->footprint, &rows, &rowBytes, &ticket->bytes);
	if (ticket->bytes == UINT64_MAX || rows != UINT(ticket->height) || rowBytes != UINT64(ticket->width) * 4)
		throw std::runtime_error("unexpected readback footprint");
	auto buffer = makeBuffer(i.device.Get(), ticket->bytes, D3D12_HEAP_TYPE_READBACK);
	ticket->resource = buffer->resource;
	// The color target is restored to RENDER_TARGET so the host can keep
	// recording into the very same attachment after it submits and waits.
	transition(i.current.commands, i.current.colorResource, D3D12_RESOURCE_STATE_RENDER_TARGET,
			   D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = i.current.colorResource;
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	D3D12_TEXTURE_COPY_LOCATION destination{};
	destination.pResource = ticket->resource.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destination.PlacedFootprint = ticket->footprint;
	i.current.commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	transition(i.current.commands, i.current.colorResource, D3D12_RESOURCE_STATE_COPY_SOURCE,
			   D3D12_RESOURCE_STATE_RENDER_TARGET);
	i.lifetime.retain(ticket);
	D3D12Readback result;
	result.data = ticket;
	return result;
}

std::vector<std::uint8_t> D3D12Device::resolveReadback(const D3D12Readback &ticket, std::uint64_t completed)
{
	if (!ticket.data || completed < ticket.data->serial)
		throw std::logic_error("readback fence has not completed");
	const auto &t = *ticket.data;
	if (t.owner != impl_->device.Get())
		throw std::invalid_argument("readback belongs to another device");
	D3D12_RANGE range{0, SIZE_T(t.bytes)};
	void *mapped = nullptr;
	check(t.resource->Map(0, &range, &mapped), "map completed readback");
	try
	{
		auto pixels = readbackRGBA(static_cast<std::uint8_t *>(mapped) + t.footprint.Offset,
								   std::size_t(t.bytes - t.footprint.Offset), t.width, t.height,
								   t.footprint.Footprint.RowPitch, true, t.bgra);
		const D3D12_RANGE noWrite{0, 0};
		t.resource->Unmap(0, &noWrite);
		return pixels;
	}
	catch (...)
	{
		const D3D12_RANGE noWrite{0, 0};
		t.resource->Unmap(0, &noWrite);
		throw;
	}
}
} // namespace render
} // namespace b173
