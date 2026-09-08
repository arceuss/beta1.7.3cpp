#pragma once

#include "client/renderer/portable/Device.h"

#include <memory>

namespace b173
{
namespace render
{
using GLProc = void (*)();
using GLProcLoader = GLProc (*)(const char *name);
enum class GLProfile
{
	Core33,
	ES20
};

struct GLCreateInfo
{
	GLProfile profile = GLProfile::Core33;
	GLProcLoader getProc = nullptr;
	bool validateDraws = false;
};

// The platform must create/make current the requested context before construction,
// keep it current during all calls, and destroy this object BEFORE that context.
class GLDevice final : public Device
{
  public:
	explicit GLDevice(const GLCreateInfo &info);
	~GLDevice() override;
	void beginFrame(unsigned framebuffer, int width, int height);
	// Invalidates pipeline/program tracking only. External GL state users must
	// restore framebuffer, VAO, buffers, pixel stores and all untracked state.
	// Exclusive context-state ownership is the supported integration contract.
	void invalidateState();
	std::string description() const;
	// ES 2.0 only guarantees fragment mediump. The device always builds; this
	// reports whether the context offered fragment highp for the shared varyings.
	bool fragmentHighp() const;
	// GL_ALIASED_LINE_WIDTH_RANGE of the live context. Wide-line policy belongs to
	// the caller: a draw whose line width falls outside this range is rejected.
	void lineWidthRange(float *minimum, float *maximum) const;
	MeshId createMesh(const MeshData &mesh) override;
	void destroyMesh(MeshId id) override;
	TextureId createTexture(const TextureDesc &desc, const void *rgba, std::size_t bytes,
							std::size_t rowStride = 0) override;
	void updateTexture(TextureId id, Rect region, const void *rgba, std::size_t bytes,
					   std::size_t rowStride = 0) override;
	void destroyTexture(TextureId id) override;
	void draw(const Draw &draw) override;
	void drawTransient(const MeshData &mesh, const Draw &draw) override;
	void clear(Vec4 color, float depth, bool colorBit, bool depthBit, const PipelineState &writeState);
	std::vector<std::uint8_t> readPixels(Rect region);

  private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace render
} // namespace b173
