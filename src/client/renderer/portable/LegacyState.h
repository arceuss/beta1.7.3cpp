#pragma once

#include "client/renderer/portable/RenderTypes.h"

namespace b173
{
namespace render
{
enum class MatrixMode
{
	ModelView,
	Projection,
	Texture
};

struct DirectionalLight
{
	Vec4 eyePosition{0, 0, 1, 0};
	Vec4 diffuse{0, 0, 0, 1}, ambient{0, 0, 0, 1};
	bool enabled = false;
};

// B173 - The fixed-function subset used by the pinned renderer, not a complete
// OpenGL implementation. Display-list recording and client-array capture belong
// above this class. State is evaluated at replay/draw time, not compilation time.
class LegacyState
{
  public:
	LegacyState();

	void matrixMode(MatrixMode mode)
	{
		if (static_cast<unsigned>(mode) > static_cast<unsigned>(MatrixMode::Texture))
			throw std::invalid_argument("invalid matrix mode");
		mode_ = mode;
	}

	void loadIdentity();
	void loadMatrix(const Mat4 &matrix);
	void multMatrix(const Mat4 &matrix);
	void pushMatrix();
	void popMatrix();
	const Mat4 &matrix(MatrixMode mode) const;

	void translate(double x, double y, double z)
	{
		multMatrix(Mat4::translation(x, y, z));
	}

	void scale(double x, double y, double z)
	{
		multMatrix(Mat4::scale(x, y, z));
	}

	void rotate(double angle, double x, double y, double z)
	{
		multMatrix(Mat4::rotation(angle, x, y, z));
	}

	void setLightPosition(unsigned light, Vec4 objectPosition);
	// Material/specular/point lights outside the known game subset must not be
	// silently approximated. setLightPosition rejects non-directional inputs.
	Uniforms uniforms(const MeshData &mesh, const TextureDesc *texture, ClipConvention convention) const;

	Vec4 currentColor{1, 1, 1, 1}, currentNormal{0, 0, 1, 0}, currentTexCoord{0, 0, 0, 1};
	bool lighting = false, colorMaterial = false, normalizeNormals = false, rescaleNormals = false;
	// The game uses two different glColorMaterial modes: GL_FRONT_AND_BACK with
	// GL_AMBIENT_AND_DIFFUSE (Lighting, held items) and GL_FRONT with GL_AMBIENT
	// (GameRenderer's alpha-place path). Both masks are honored per material term,
	// including alpha: the lit alpha always comes from the diffuse material.
	bool colorMaterialAmbient = true, colorMaterialDiffuse = true;
	NormalConversion normalConversion = NormalConversion::Legacy21;
	Vec4 globalAmbient{0.2f, 0.2f, 0.2f, 1};
	Vec4 materialAmbient{0.2f, 0.2f, 0.2f, 1}, materialDiffuse{0.8f, 0.8f, 0.8f, 1};
	Vec4 materialEmission{0, 0, 0, 1};
	std::array<DirectionalLight, 2> lights{};
	bool textureEnabled = false;
	TextureEnv textureEnv = TextureEnv::Modulate;
	Vec4 textureEnvColor{0, 0, 0, 0};
	bool alphaTest = false;
	Compare alphaCompare = Compare::Always;
	float alphaReference = 0;
	Fog fog = Fog::None;
	bool radialFog = false;
	Vec4 fogColor{0, 0, 0, 0};
	float fogStart = 0, fogEnd = 1, fogDensity = 1;
	PipelineState pipeline;

  private:
	MatrixMode mode_ = MatrixMode::ModelView;
	std::array<std::vector<Mat4>, 3> stacks_;
};

// Adds the actual texture object's sampling state at draw time. This is also used
// by native backends, preventing stale texture-size uniforms after atlas reload.
// emulateNpotRepeat is set by a backend whose sampler cannot repeat an NPOT
// texture (ES 2.0 core); the shader then repeats those axes itself.
void applyTextureUniforms(Uniforms &u, const TextureDesc &texture, bool emulateNpotRepeat = false);
void applyMeshUniforms(Uniforms &u, const MeshData &mesh);
void applyClipConvention(Uniforms &u, ClipConvention convention);
} // namespace render
} // namespace b173
