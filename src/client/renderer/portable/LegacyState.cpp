#include "client/renderer/portable/LegacyState.h"

#include <algorithm>
#include <cmath>

namespace b173
{
namespace render
{
LegacyState::LegacyState()
{
	for (std::vector<Mat4> &s : stacks_)
		s.push_back(Mat4::identity());
	lights[0].diffuse = {1, 1, 1, 1};
}

void LegacyState::loadIdentity()
{
	loadMatrix(Mat4::identity());
}

void LegacyState::loadMatrix(const Mat4 &m)
{
	stacks_[unsigned(mode_)].back() = m;
}

void LegacyState::multMatrix(const Mat4 &m)
{
	Mat4 &current = stacks_[unsigned(mode_)].back();
	current = current * m;
}

void LegacyState::pushMatrix()
{
	std::vector<Mat4> &s = stacks_[unsigned(mode_)];
	const std::size_t maxDepth = mode_ == MatrixMode::ModelView ? 64 : 8;
	if (s.size() == maxDepth)
		throw std::overflow_error("matrix stack overflow");
	s.push_back(s.back());
}

void LegacyState::popMatrix()
{
	std::vector<Mat4> &s = stacks_[unsigned(mode_)];
	if (s.size() == 1)
		throw std::underflow_error("matrix stack underflow");
	s.pop_back();
}

const Mat4 &LegacyState::matrix(MatrixMode mode) const
{
	if (static_cast<unsigned>(mode) > static_cast<unsigned>(MatrixMode::Texture))
		throw std::invalid_argument("invalid matrix mode");
	return stacks_[unsigned(mode)].back();
}

void LegacyState::setLightPosition(unsigned index, Vec4 p)
{
	if (index >= lights.size())
		throw std::out_of_range("only the game's two directional lights are supported");
	if (p.w != 0)
		throw std::invalid_argument("point lights are not part of this fixed-function subset");
	lights[index].eyePosition = matrix(MatrixMode::ModelView) * p;
}

void applyMeshUniforms(Uniforms &u, const MeshData &m)
{
	u.v[19] = {float(m.hasTexture), float(m.hasColor), float(m.hasNormal), 0};
}

void applyClipConvention(Uniforms &u, ClipConvention c)
{
	u.v[30].x = c == ClipConvention::OpenGL ? 0.0f : 1.0f;
	u.v[30].y = c == ClipConvention::Vulkan ? 1.0f : 0.0f;
}

void applyTextureUniforms(Uniforms &u, const TextureDesc &d, bool emulateNpotRepeat)
{
	const bool npot = (d.width & (d.width - 1)) || (d.height & (d.height - 1));
	const bool emulate = emulateNpotRepeat && npot;
	u.f[3] = {float(d.width), float(d.height), float(d.minFilter == Filter::Linear), 0};
	u.f[4] = {float(d.wrapS == Wrap::LegacyClamp), float(d.wrapT == Wrap::LegacyClamp),
			  float(emulate && d.wrapS == Wrap::Repeat), float(emulate && d.wrapT == Wrap::Repeat)};
	u.f[5] = d.border;
}

Uniforms LegacyState::uniforms(const MeshData &mesh, const TextureDesc *texture, ClipConvention clip) const
{
	Uniforms u;
	for (unsigned matrixIndex = 0; matrixIndex < 3; ++matrixIndex)
	{
		const Mat4 &m = stacks_[matrixIndex].back();
		for (unsigned col = 0; col < 4; ++col)
			u.v[matrixIndex * 4 + col] = {m.v[col * 4], m.v[col * 4 + 1], m.v[col * 4 + 2], m.v[col * 4 + 3]};
	}
	std::array<Vec4, 3> n =
		lighting ? inverseTranspose3(matrix(MatrixMode::ModelView)) : inverseTranspose3(Mat4::identity());
	for (unsigned i = 0; i < 3; ++i)
		u.v[12 + i] = n[i];
	float rescale = 1;
	if (rescaleNormals)
	{
		// GL_RESCALE_NORMAL is 1/|third row of the inverse modelview 3x3|. Row k of
		// the inverse transpose holds column k of the inverse, so the third row of
		// the inverse is the .z component of the three rows returned above.
		const float d = std::sqrt(n[0].z * n[0].z + n[1].z * n[1].z + n[2].z * n[2].z);
		if (!(d > 0))
			throw std::domain_error("invalid rescale-normal factor");
		rescale = 1 / d;
	}
	u.v[15] = currentColor;
	u.v[16] = currentNormal;
	u.v[17] = currentTexCoord;
	u.v[18] = {float(lighting), float(normalizeNormals), rescale,
			   float(normalConversion == NormalConversion::ModernSnorm)};
	applyMeshUniforms(u, mesh);
	u.v[20] = globalAmbient;
	u.v[21] = materialAmbient;
	u.v[22] = materialDiffuse;
	u.v[23] = materialEmission;
	for (unsigned i = 0; i < 2; ++i)
	{
		const DirectionalLight &light = lights[i];
		Vec4 p = light.eyePosition;
		p.w = float(light.enabled);
		u.v[24 + 3 * i] = p;
		u.v[25 + 3 * i] = light.diffuse;
		u.v[26 + 3 * i] = light.ambient;
	}
	u.v[30].z = float(radialFog);
	// x enables color-material tracking; y and z carry the per-term masks, so
	// GL_AMBIENT and GL_AMBIENT_AND_DIFFUSE stay distinguishable in one lane set.
	u.v[31].x = float(colorMaterial);
	u.v[31].y = float(colorMaterialAmbient);
	u.v[31].z = float(colorMaterialDiffuse);
	applyClipConvention(u, clip);
	u.f[0] = fogColor;
	u.f[1] = {fogStart, fogEnd, fogDensity, float(fog)};
	u.f[2] = {float(textureEnabled), float(alphaTest ? alphaCompare : Compare::Always),
			  std::max(0.0f, std::min(1.0f, alphaReference)), float(textureEnv)};
	u.f[6] = textureEnvColor;
	if (texture)
	{
		applyTextureUniforms(u, *texture);
	}
	else
	{
		TextureDesc white;
		white.width = white.height = 1;
		applyTextureUniforms(u, white);
	}
	return u;
}
} // namespace render
} // namespace b173
