#include "client/renderer/TileRenderer.h"

#include "client/renderer/Tesselator.h"
#include "world/level/tile/LiquidTile.h"
#include "world/level/tile/TallGrassTile.h"
#include "world/level/material/LiquidMaterial.h"

#include <cmath>

namespace
{
	constexpr double FLOW_TEXTURE_NONE = -1000.0;
	constexpr double HALF_PI = 3.14159265358979323846 * 0.5;

	int_t getEffectiveFlowDepth(LevelSource &level, int_t x, int_t y, int_t z, const Material &material)
	{
		if (&level.getMaterial(x, y, z) != &material)
			return -1;

		int_t depth = level.getData(x, y, z);
		if (depth >= 8)
			depth = 0;
		return depth;
	}

	bool blocksFlowAngle(LevelSource &level, int_t x, int_t y, int_t z, const Material &material)
	{
		const Material &adjacent = level.getMaterial(x, y, z);
		if (&adjacent == &material || &adjacent == &Material::ice())
			return false;
		return adjacent.isSolid();
	}

	double getLiquidFlowAngle(LevelSource &level, int_t x, int_t y, int_t z, const Material &material)
	{
		int_t depth = getEffectiveFlowDepth(level, x, y, z, material);
		if (depth < 0)
			return FLOW_TEXTURE_NONE;

		double flowX = 0.0;
		double flowZ = 0.0;

		for (int_t direction = 0; direction < 4; ++direction)
		{
			int_t nx = x;
			int_t nz = z;
			if (direction == 0) nx = x - 1;
			if (direction == 1) nz = z - 1;
			if (direction == 2) nx = x + 1;
			if (direction == 3) nz = z + 1;

			int_t adjacentDepth = getEffectiveFlowDepth(level, nx, y, nz, material);
			if (adjacentDepth < 0)
			{
				if (!blocksFlowAngle(level, nx, y, nz, material))
				{
					adjacentDepth = getEffectiveFlowDepth(level, nx, y - 1, nz, material);
					if (adjacentDepth >= 0)
					{
						int_t delta = adjacentDepth - (depth - 8);
						flowX += (nx - x) * delta;
						flowZ += (nz - z) * delta;
					}
				}
			}
			else
			{
				int_t delta = adjacentDepth - depth;
				flowX += (nx - x) * delta;
				flowZ += (nz - z) * delta;
			}
		}

		if (flowX == 0.0 && flowZ == 0.0)
			return FLOW_TEXTURE_NONE;

		return std::atan2(flowZ, flowX) - HALF_PI;
	}
}

TileRenderer::TileRenderer(LevelSource *levelSource, bool ambientOcclusion, bool fancyGrass) : level(levelSource), ambientOcclusion(ambientOcclusion), fancyGrass(fancyGrass)
{

}

TileRenderer::TileRenderer(bool ambientOcclusion, bool fancyGrass) : ambientOcclusion(ambientOcclusion), fancyGrass(fancyGrass)
{

}

void TileRenderer::tesselateInWorld(Tile &tile, int_t x, int_t y, int_t z, int_t fixedTexture)
{
	this->fixedTexture = fixedTexture;
	tesselateInWorld(tile, x, y, z);
	this->fixedTexture = -1;
}

void TileRenderer::tesselateInWorldNoCulling(Tile &tile, int_t x, int_t y, int_t z)
{
	noCulling = true;
	tesselateInWorld(tile, x, y, z);
	noCulling = false;
}

bool TileRenderer::tesselateInWorld(Tile &tt, int_t x, int_t y, int_t z)
{
	int_t shape = tt.getRenderShape();
	tt.updateShape(*level, x, y, z);

	if (shape == Tile::SHAPE_BLOCK)
		return tesselateBlockInWorld(tt, x, y, z);
	if (shape == Tile::SHAPE_CROSS_TEXTURE)
		return tesselateCrossTextureInWorld(tt, x, y, z);
	if (shape == Tile::SHAPE_CACTUS)
		return tesselateCactusInWorld(tt, x, y, z);
	if (shape == Tile::SHAPE_WATER)
		return tesselateLiquidInWorld(tt, x, y, z);
	
	return false;
}

bool TileRenderer::tesselateCactusInWorld(Tile &tt, int_t x, int_t y, int_t z)
{
	float c10 = 0.5f;
	float c11 = 1.0f;
	float c2 = 0.8f;
	float c3 = 0.6f;
	float s = 1.0f / 16.0f;
	bool changed = false;

	int_t col = tt.getColor(*level, x, y, z);
	float r = static_cast<float>((col >> 16) & 0xFF) / 255.0f;
	float g = static_cast<float>((col >> 8) & 0xFF) / 255.0f;
	float b = static_cast<float>(col & 0xFF) / 255.0f;
	float lightValueOwn = tt.getBrightness(*level, x, y, z);

	double oldxx0 = tt.xx0;
	double oldyy0 = tt.yy0;
	double oldzz0 = tt.zz0;
	double oldxx1 = tt.xx1;
	double oldyy1 = tt.yy1;
	double oldzz1 = tt.zz1;
	double inset = 1.0 / 16.0;
	double cactusxx0 = inset;
	double cactusyy0 = 0.0;
	double cactuszz0 = inset;
	double cactusxx1 = 1.0 - inset;
	double cactusyy1 = 1.0;
	double cactuszz1 = 1.0 - inset;

	if (noCulling || tt.shouldRenderFace(*level, x, y - 1, z, Facing::DOWN))
	{
		float br = tt.getBrightness(*level, x, y - 1, z);
		tt.setShape(cactusxx0, cactusyy0, cactuszz0, cactusxx1, cactusyy1, cactuszz1);
		Tesselator::instance.color(c10 * r * br, c10 * g * br, c10 * b * br);
		renderFaceUp(tt, x, y, z, tt.getTexture(*level, x, y, z, Facing::DOWN));
		changed = true;
	}

	if (noCulling || tt.shouldRenderFace(*level, x, y + 1, z, Facing::UP))
	{
		float br = tt.getBrightness(*level, x, y + 1, z);
		if (cactusyy1 != 1.0 && !tt.material.isLiquid()) br = lightValueOwn;
		tt.setShape(cactusxx0, cactusyy0, cactuszz0, cactusxx1, cactusyy1, cactuszz1);
		Tesselator::instance.color(c11 * r * br, c11 * g * br, c11 * b * br);
		renderFaceDown(tt, x, y, z, tt.getTexture(*level, x, y, z, Facing::UP));
		changed = true;
	}

	tt.setShape(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	if (noCulling || tt.shouldRenderFace(*level, x, y, z - 1, Facing::NORTH))
	{
		float br = tt.getBrightness(*level, x, y, z - 1);
		Tesselator::instance.color(c2 * r * br, c2 * g * br, c2 * b * br);
		renderNorth(tt, x, y, z + s, tt.getTexture(*level, x, y, z, Facing::NORTH));
		changed = true;
	}

	if (noCulling || tt.shouldRenderFace(*level, x, y, z + 1, Facing::SOUTH))
	{
		float br = tt.getBrightness(*level, x, y, z + 1);
		Tesselator::instance.color(c2 * r * br, c2 * g * br, c2 * b * br);
		renderSouth(tt, x, y, z - s, tt.getTexture(*level, x, y, z, Facing::SOUTH));
		changed = true;
	}

	if (noCulling || tt.shouldRenderFace(*level, x - 1, y, z, Facing::WEST))
	{
		float br = tt.getBrightness(*level, x - 1, y, z);
		Tesselator::instance.color(c3 * r * br, c3 * g * br, c3 * b * br);
		renderWest(tt, x + s, y, z, tt.getTexture(*level, x, y, z, Facing::WEST));
		changed = true;
	}

	if (noCulling || tt.shouldRenderFace(*level, x + 1, y, z, Facing::EAST))
	{
		float br = tt.getBrightness(*level, x + 1, y, z);
		Tesselator::instance.color(c3 * r * br, c3 * g * br, c3 * b * br);
		renderEast(tt, x - s, y, z, tt.getTexture(*level, x, y, z, Facing::EAST));
		changed = true;
	}

	tt.setShape(oldxx0, oldyy0, oldzz0, oldxx1, oldyy1, oldzz1);
	enableAO = false;
	return changed;
}

bool TileRenderer::tesselateCrossTextureInWorld(Tile &tt, int_t x, int_t y, int_t z)
{
	Tesselator &t = Tesselator::instance;
	int_t tex = tt.getTexture(*level, x, y, z, Facing::NORTH);
	if (fixedTexture >= 0) tex = fixedTexture;

	int_t color = tt.getColor(*level, x, y, z);
	float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
	float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
	float b = static_cast<float>(color & 0xFF) / 255.0f;
	float bright = tt.getBrightness(*level, x, y, z);
	t.color(r * bright, g * bright, b * bright);

	int_t xt = (tex & 0xF) << 4;
	int_t yt = tex & 0xF0;
	double u0 = static_cast<double>(xt) / 256.0;
	double u1 = (static_cast<double>(xt) + 15.99) / 256.0;
	double v0 = static_cast<double>(yt) / 256.0;
	double v1 = (static_cast<double>(yt) + 15.99) / 256.0;

	// Beta renderCrossedSquares uses fixed-size quads; tall grass gets jitter and crops render 1 px lower.
	double xo = 0.0;
	double yo = 0.0;
	double zo = 0.0;
	double cropYOffset = tt.id == 59 ? -(1.0 / 16.0) : 0.0;
	if (tt.id == Tile::tallGrass.id)
	{
		long_t offset = (static_cast<long_t>(x) * 3129871LL) ^ (static_cast<long_t>(z) * 116129781LL) ^ static_cast<long_t>(y);
		offset = offset * offset * 42317861LL + offset * 11LL;
		xo = ((static_cast<double>((offset >> 16) & 15LL) / 15.0) - 0.5) * 0.5;
		yo = ((static_cast<double>((offset >> 20) & 15LL) / 15.0) - 1.0) * 0.2;
		zo = ((static_cast<double>((offset >> 24) & 15LL) / 15.0) - 0.5) * 0.5;
	}

	double x0 = x + xo + 0.5 - 0.45;
	double x1 = x + xo + 0.5 + 0.45;
	double y0 = y + yo + cropYOffset;
	double y1 = y + yo + cropYOffset + 1.0;
	double z0 = z + zo + 0.5 - 0.45;
	double z1 = z + zo + 0.5 + 0.45;

	auto renderQuad = [&](double ax, double az, double bx, double bz) {
		t.vertexUV(ax, y1, az, u0, v0);
		t.vertexUV(ax, y0, az, u0, v1);
		t.vertexUV(bx, y0, bz, u1, v1);
		t.vertexUV(bx, y1, bz, u1, v0);
		t.vertexUV(bx, y1, bz, u0, v0);
		t.vertexUV(bx, y0, bz, u0, v1);
		t.vertexUV(ax, y0, az, u1, v1);
		t.vertexUV(ax, y1, az, u1, v0);
	};

	renderQuad(x0, z0, x1, z1);
	renderQuad(x0, z1, x1, z0);
	return true;
}

bool TileRenderer::tesselateBlockInWorld(Tile &tt, int_t x, int_t y, int_t z)
{
	int_t col = tt.getColor(*level, x, y, z);
	float r = ((col >> 16) & 0xFF) / 255.0f;
	float g = ((col >> 8) & 0xFF) / 255.0f;
	float b = (col & 0xFF) / 255.0f;
	return tesselateBlockInWorld(tt, x, y, z, r, g, b);
}

bool TileRenderer::tesselateBlockInWorld(Tile &tt, int_t x, int_t y, int_t z, float r, float g, float b)
{
	Tesselator &t = Tesselator::instance;
	bool changed = false;
	float c10 = 0.5f;
	float c11 = 1.0f;
	float c2 = 0.8f;
	float c3 = 0.6f;

	float grassR = r;
	float grassG = g;
	float grassB = b;
	bool isGrass = &tt == reinterpret_cast<Tile*>(&Tile::grass);
	bool useFancyGrass = fancyGrass && fixedTexture < 0 && isGrass;

	bool cactusShape = tt.getRenderShape() == Tile::SHAPE_CACTUS;
	double cactusInset = cactusShape ? (1.0 / 16.0) : 0.0;

	float r11 = c11 * r;
	float g11 = c11 * g;
	float b11 = c11 * b;

	if (isGrass)
	{
		b = 1.0f;
		g = 1.0f;
		r = 1.0f;
	}

	float r10 = c10 * r;
	float r2 = c2 * r;
	float r3 = c3 * r;

	float g10 = c10 * g;
	float g2 = c2 * g;
	float g3 = c3 * g;

	float b10 = c10 * b;
	float b2 = c2 * b;
	float b3 = c3 * b;

	float lightValueOwn = tt.getBrightness(*level, x, y, z);
	bool useAO = ambientOcclusion && !noCulling && !cactusShape && tt.xx0 == 0.0 && tt.xx1 == 1.0 && tt.yy0 == 0.0 && tt.yy1 == 1.0 && tt.zz0 == 0.0 && tt.zz1 == 1.0;

	auto sample = [&](int_t sx, int_t sy, int_t sz) {
		return tt.getBrightness(*level, sx, sy, sz);
	};
	auto setVertexColors = [&](float v0, float v1, float v2, float v3, float cr, float cg, float cb) {
		enableAO = true;
		colorR0 = cr * v0; colorG0 = cg * v0; colorB0 = cb * v0;
		colorR1 = cr * v1; colorG1 = cg * v1; colorB1 = cb * v1;
		colorR2 = cr * v2; colorG2 = cg * v2; colorB2 = cb * v2;
		colorR3 = cr * v3; colorG3 = cg * v3; colorB3 = cb * v3;
	};
	auto tintVertexColors = [&](float tr, float tg, float tb) {
		colorR0 *= tr; colorG0 *= tg; colorB0 *= tb;
		colorR1 *= tr; colorG1 *= tg; colorB1 *= tb;
		colorR2 *= tr; colorG2 *= tg; colorB2 *= tb;
		colorR3 *= tr; colorG3 *= tg; colorB3 *= tb;
	};

	// Pre-compute face-adjacent brightness and corner transparency flags
	float aoXNeg = 0, aoYNeg = 0, aoZNeg = 0, aoXPos = 0, aoYPos = 0, aoZPos = 0;
	bool tXpYp = false, tXpYn = false, tXpZp = false, tXpZn = false;
	bool tXnYp = false, tXnYn = false, tXnZn = false, tXnZp = false;
	bool tYpZp = false, tYpZn = false, tYnZp = false, tYnZn = false;

	if (useAO)
	{
		aoXNeg = sample(x - 1, y, z);
		aoYNeg = sample(x, y - 1, z);
		aoZNeg = sample(x, y, z - 1);
		aoXPos = sample(x + 1, y, z);
		aoYPos = sample(x, y + 1, z);
		aoZPos = sample(x, y, z + 1);

		// canBlockGrass: true if block is transparent/air, false if opaque
		auto cbg = [&](int_t sx, int_t sy, int_t sz) { return !Tile::solid[level->getTile(sx, sy, sz)]; };
		tXpYp = cbg(x + 1, y + 1, z); tXpYn = cbg(x + 1, y - 1, z);
		tXpZp = cbg(x + 1, y, z + 1); tXpZn = cbg(x + 1, y, z - 1);
		tXnYp = cbg(x - 1, y + 1, z); tXnYn = cbg(x - 1, y - 1, z);
		tXnZn = cbg(x - 1, y, z - 1); tXnZp = cbg(x - 1, y, z + 1);
		tYpZp = cbg(x, y + 1, z + 1); tYpZn = cbg(x, y + 1, z - 1);
		tYnZp = cbg(x, y - 1, z + 1); tYnZn = cbg(x, y - 1, z - 1);
	}

	// BOTTOM face (y-1)
	if (noCulling || tt.shouldRenderFace(*level, x, y - 1, z, Facing::DOWN))
	{
		if (useAO)
		{
			float eXn = sample(x - 1, y - 1, z);
			float eZn = sample(x, y - 1, z - 1);
			float eZp = sample(x, y - 1, z + 1);
			float eXp = sample(x + 1, y - 1, z);
			// Corner occlusion: skip diagonal if both adjacent edges are opaque
			float cXnZn = (tYnZn || tXnYn) ? sample(x - 1, y - 1, z - 1) : eXn;
			float cXnZp = (tYnZp || tXnYn) ? sample(x - 1, y - 1, z + 1) : eXn;
			float cXpZn = (tYnZn || tXpYn) ? sample(x + 1, y - 1, z - 1) : eXp;
			float cXpZp = (tYnZp || tXpYn) ? sample(x + 1, y - 1, z + 1) : eXp;
			setVertexColors(
				(cXnZp + eXn + eZp + aoYNeg) * 0.25f,
				(eXn + cXnZn + aoYNeg + eZn) * 0.25f,
				(aoYNeg + eZn + eXp + cXpZn) * 0.25f,
				(eZp + aoYNeg + cXpZp + eXp) * 0.25f,
				r10, g10, b10);
		}
		else
		{
			enableAO = false;
			float br = sample(x, y - 1, z);
			t.color(r10 * br, g10 * br, b10 * br);
		}
		renderFaceUp(tt, x, y, z, tt.getTexture(*level, x, y, z, Facing::DOWN));
		changed = true;
	}

	// TOP face (y+1)
	if (noCulling || tt.shouldRenderFace(*level, x, y + 1, z, Facing::UP))
	{
		if (useAO)
		{
			float eXn = sample(x - 1, y + 1, z);
			float eXp = sample(x + 1, y + 1, z);
			float eZn = sample(x, y + 1, z - 1);
			float eZp = sample(x, y + 1, z + 1);
			float cXnZn = (tYpZn || tXnYp) ? sample(x - 1, y + 1, z - 1) : eXn;
			float cXpZn = (tYpZn || tXpYp) ? sample(x + 1, y + 1, z - 1) : eXp;
			float cXnZp = (tYpZp || tXnYp) ? sample(x - 1, y + 1, z + 1) : eXn;
			float cXpZp = (tYpZp || tXpYp) ? sample(x + 1, y + 1, z + 1) : eXp;
			setVertexColors(
				(eZp + aoYPos + cXpZp + eXp) * 0.25f,
				(aoYPos + eZn + eXp + cXpZn) * 0.25f,
				(eXn + cXnZn + aoYPos + eZn) * 0.25f,
				(cXnZp + eXn + eZp + aoYPos) * 0.25f,
				r11, g11, b11);
		}
		else
		{
			enableAO = false;
			float br = sample(x, y + 1, z);
			if (tt.yy1 != 1.0 && !tt.material.isLiquid()) br = lightValueOwn;
			t.color(r11 * br, g11 * br, b11 * br);
		}
		renderFaceDown(tt, x, y, z, tt.getTexture(*level, x, y, z, Facing::UP));
		changed = true;
	}

	// NORTH face (z-1)
	if (noCulling || tt.shouldRenderFace(*level, x, y, z - 1, Facing::NORTH))
	{
		int_t tex = tt.getTexture(*level, x, y, z, Facing::NORTH);
		float br = 0.0f;
		if (useAO)
		{
			float eXn = sample(x - 1, y, z - 1);
			float eYn = sample(x, y - 1, z - 1);
			float eYp = sample(x, y + 1, z - 1);
			float eXp = sample(x + 1, y, z - 1);
			float cXnYn = (tXnZn || tYnZn) ? sample(x - 1, y - 1, z - 1) : eXn;
			float cXnYp = (tXnZn || tYpZn) ? sample(x - 1, y + 1, z - 1) : eXn;
			float cXpYn = (tXpZn || tYnZn) ? sample(x + 1, y - 1, z - 1) : eXp;
			float cXpYp = (tXpZn || tYpZn) ? sample(x + 1, y + 1, z - 1) : eXp;
			setVertexColors(
				(eXn + cXnYp + aoZNeg + eYp) * 0.25f,
				(aoZNeg + eYp + eXp + cXpYp) * 0.25f,
				(eYn + aoZNeg + cXpYn + eXp) * 0.25f,
				(cXnYn + eXn + eYn + aoZNeg) * 0.25f,
				r2, g2, b2);
		}
		else
		{
			enableAO = false;
			br = sample(x, y, z - 1);
			if (tt.zz0 > 0.0) br = lightValueOwn;
			t.color(r2 * br, g2 * br, b2 * br);
		}
		renderNorth(tt, x, y, z + cactusInset, tex);
		if (useFancyGrass && tex == 3)
		{
			if (useAO)
			{
				tintVertexColors(grassR, grassG, grassB);
				renderNorth(tt, x, y, z + cactusInset, 38);
			}
			else
			{
				t.color(c2 * grassR * br, c2 * grassG * br, c2 * grassB * br);
				renderNorth(tt, x, y, z + cactusInset, 38);
			}
		}
		changed = true;
	}

	// SOUTH face (z+1)
	if (noCulling || tt.shouldRenderFace(*level, x, y, z + 1, Facing::SOUTH))
	{
		int_t tex = tt.getTexture(*level, x, y, z, Facing::SOUTH);
		float br = 0.0f;
		if (useAO)
		{
			float eXn = sample(x - 1, y, z + 1);
			float eXp = sample(x + 1, y, z + 1);
			float eYn = sample(x, y - 1, z + 1);
			float eYp = sample(x, y + 1, z + 1);
			float cXnYn = (tXnZp || tYnZp) ? sample(x - 1, y - 1, z + 1) : eXn;
			float cXnYp = (tXnZp || tYpZp) ? sample(x - 1, y + 1, z + 1) : eXn;
			float cXpYn = (tXpZp || tYnZp) ? sample(x + 1, y - 1, z + 1) : eXp;
			float cXpYp = (tXpZp || tYpZp) ? sample(x + 1, y + 1, z + 1) : eXp;
			setVertexColors(
				(eXn + cXnYp + aoZPos + eYp) * 0.25f,
				(cXnYn + eXn + eYn + aoZPos) * 0.25f,
				(eYn + aoZPos + cXpYn + eXp) * 0.25f,
				(aoZPos + eYp + eXp + cXpYp) * 0.25f,
				r2, g2, b2);
		}
		else
		{
			enableAO = false;
			br = sample(x, y, z + 1);
			if (tt.zz1 < 1.0) br = lightValueOwn;
			t.color(r2 * br, g2 * br, b2 * br);
		}
		renderSouth(tt, x, y, z - cactusInset, tex);
		if (useFancyGrass && tex == 3)
		{
			if (useAO)
			{
				tintVertexColors(grassR, grassG, grassB);
				renderSouth(tt, x, y, z - cactusInset, 38);
			}
			else
			{
				t.color(c2 * grassR * br, c2 * grassG * br, c2 * grassB * br);
				renderSouth(tt, x, y, z - cactusInset, 38);
			}
		}
		changed = true;
	}

	// WEST face (x-1)
	if (noCulling || tt.shouldRenderFace(*level, x - 1, y, z, Facing::WEST))
	{
		int_t tex = tt.getTexture(*level, x, y, z, Facing::WEST);
		float br = 0.0f;
		if (useAO)
		{
			float eYn = sample(x - 1, y - 1, z);
			float eZn = sample(x - 1, y, z - 1);
			float eZp = sample(x - 1, y, z + 1);
			float eYp = sample(x - 1, y + 1, z);
			float cYnZn = (tXnZn || tXnYn) ? sample(x - 1, y - 1, z - 1) : eZn;
			float cYnZp = (tXnZp || tXnYn) ? sample(x - 1, y - 1, z + 1) : eZp;
			float cYpZn = (tXnZn || tXnYp) ? sample(x - 1, y + 1, z - 1) : eZn;
			float cYpZp = (tXnZp || tXnYp) ? sample(x - 1, y + 1, z + 1) : eZp;
			setVertexColors(
				(aoXNeg + eZp + eYp + cYpZp) * 0.25f,
				(eZn + aoXNeg + cYpZn + eYp) * 0.25f,
				(cYnZn + eYn + eZn + aoXNeg) * 0.25f,
				(eYn + cYnZp + aoXNeg + eZp) * 0.25f,
				r3, g3, b3);
		}
		else
		{
			enableAO = false;
			br = sample(x - 1, y, z);
			if (tt.xx0 > 0.0) br = lightValueOwn;
			t.color(r3 * br, g3 * br, b3 * br);
		}
		renderWest(tt, x + cactusInset, y, z, tex);
		if (useFancyGrass && tex == 3)
		{
			if (useAO)
			{
				tintVertexColors(grassR, grassG, grassB);
				renderWest(tt, x + cactusInset, y, z, 38);
			}
			else
			{
				t.color(c3 * grassR * br, c3 * grassG * br, c3 * grassB * br);
				renderWest(tt, x + cactusInset, y, z, 38);
			}
		}
		changed = true;
	}

	// EAST face (x+1)
	if (noCulling || tt.shouldRenderFace(*level, x + 1, y, z, Facing::EAST))
	{
		int_t tex = tt.getTexture(*level, x, y, z, Facing::EAST);
		float br = 0.0f;
		if (useAO)
		{
			float eYn = sample(x + 1, y - 1, z);
			float eZn = sample(x + 1, y, z - 1);
			float eZp = sample(x + 1, y, z + 1);
			float eYp = sample(x + 1, y + 1, z);
			float cYnZn = (tXpYn || tXpZn) ? sample(x + 1, y - 1, z - 1) : eZn;
			float cYnZp = (tXpYn || tXpZp) ? sample(x + 1, y - 1, z + 1) : eZp;
			float cYpZn = (tXpYp || tXpZn) ? sample(x + 1, y + 1, z - 1) : eZn;
			float cYpZp = (tXpYp || tXpZp) ? sample(x + 1, y + 1, z + 1) : eZp;
			setVertexColors(
				(eYn + cYnZp + aoXPos + eZp) * 0.25f,
				(cYnZn + eYn + eZn + aoXPos) * 0.25f,
				(eZn + aoXPos + cYpZn + eYp) * 0.25f,
				(aoXPos + eZp + eYp + cYpZp) * 0.25f,
				r3, g3, b3);
		}
		else
		{
			enableAO = false;
			br = sample(x + 1, y, z);
			if (tt.xx1 < 1.0) br = lightValueOwn;
			t.color(r3 * br, g3 * br, b3 * br);
		}
		renderEast(tt, x - cactusInset, y, z, tex);
		if (useFancyGrass && tex == 3)
		{
			if (useAO)
			{
				tintVertexColors(grassR, grassG, grassB);
				renderEast(tt, x - cactusInset, y, z, 38);
			}
			else
			{
				t.color(c3 * grassR * br, c3 * grassG * br, c3 * grassB * br);
				renderEast(tt, x - cactusInset, y, z, 38);
			}
		}
		changed = true;
	}

	enableAO = false;
	return changed;
}

void TileRenderer::renderFaceUp(Tile &tt, double x, double y, double z, int_t tex)
{
	Tesselator &t = Tesselator::instance;

	if (fixedTexture >= 0) tex = fixedTexture;

	int_t xt = (tex & 0xF) << 4;
	int_t yt = tex & 0xF0;

	double u0 = (xt + tt.xx0 * 16.0) / 256.0;
	double u1 = (xt + tt.xx1 * 16.0 - 0.01) / 256.0;
	double v0 = (yt + tt.zz0 * 16.0) / 256.0;
	double v1 = (yt + tt.zz1 * 16.0 - 0.01) / 256.0;

	if (tt.xx0 < 0.0 || tt.xx1 > 1.0)
	{
		u0 = ((xt + 0.0f) / 256.0f);
		u1 = ((xt + 15.99f) / 256.0f);
	}

	if (tt.zz0 < 0.0 || tt.zz1 > 1.0)
	{
		v0 = ((yt + 0.0f) / 256.0f);
		v1 = ((yt + 15.99f) / 256.0f);
	}

	double x0 = x + tt.xx0;
	double x1 = x + tt.xx1;
	double y0 = y + tt.yy0;
	double z0 = z + tt.zz0;
	double z1 = z + tt.zz1;

	if (enableAO)
	{
		t.color(colorR0, colorG0, colorB0);
		t.vertexUV(x0, y0, z1, u0, v1);
		t.color(colorR1, colorG1, colorB1);
		t.vertexUV(x0, y0, z0, u0, v0);
		t.color(colorR2, colorG2, colorB2);
		t.vertexUV(x1, y0, z0, u1, v0);
		t.color(colorR3, colorG3, colorB3);
		t.vertexUV(x1, y0, z1, u1, v1);
	}
	else
	{
		t.vertexUV(x0, y0, z1, u0, v1);
		t.vertexUV(x0, y0, z0, u0, v0);
		t.vertexUV(x1, y0, z0, u1, v0);
		t.vertexUV(x1, y0, z1, u1, v1);
	}
}

void TileRenderer::renderFaceDown(Tile &tt, double x, double y, double z, int_t tex)
{
	Tesselator &t = Tesselator::instance;

	if (fixedTexture >= 0) tex = fixedTexture;

	int_t xt = (tex & 0xF) << 4;
	int_t yt = tex & 0xF0;

	double u0 = (xt + tt.xx0 * 16.0) / 256.0;
	double u1 = (xt + tt.xx1 * 16.0 - 0.01) / 256.0;
	double v0 = (yt + tt.zz0 * 16.0) / 256.0;
	double v1 = (yt + tt.zz1 * 16.0 - 0.01) / 256.0;

	if (tt.xx0 < 0.0 || tt.xx1 > 1.0)
	{
		u0 = ((xt + 0.0f) / 256.0f);
		u1 = ((xt + 15.99f) / 256.0f);
	}

	if (tt.zz0 < 0.0 || tt.zz1 > 1.0)
	{
		v0 = ((yt + 0.0f) / 256.0f);
		v1 = ((yt + 15.99f) / 256.0f);
	}

	double x0 = x + tt.xx0;
	double x1 = x + tt.xx1;
	double y1 = y + tt.yy1;
	double z0 = z + tt.zz0;
	double z1 = z + tt.zz1;

	if (enableAO)
	{
		t.color(colorR0, colorG0, colorB0);
		t.vertexUV(x1, y1, z1, u1, v1);
		t.color(colorR1, colorG1, colorB1);
		t.vertexUV(x1, y1, z0, u1, v0);
		t.color(colorR2, colorG2, colorB2);
		t.vertexUV(x0, y1, z0, u0, v0);
		t.color(colorR3, colorG3, colorB3);
		t.vertexUV(x0, y1, z1, u0, v1);
	}
	else
	{
		t.vertexUV(x1, y1, z1, u1, v1);
		t.vertexUV(x1, y1, z0, u1, v0);
		t.vertexUV(x0, y1, z0, u0, v0);
		t.vertexUV(x0, y1, z1, u0, v1);
	}
}

void TileRenderer::renderNorth(Tile &tt, double x, double y, double z, int_t tex)
{
	Tesselator &t = Tesselator::instance;

	if (fixedTexture >= 0) tex = fixedTexture;
	int_t xt = (tex & 0xF) << 4;
	int_t yt = tex & 0xF0;

	double u0 = (xt + tt.xx0 * 16.0) / 256.0;
	double u1 = (xt + tt.xx1 * 16.0 - 0.01) / 256.0;
	double v0 = (yt + tt.yy0 * 16.0) / 256.0;
	double v1 = (yt + tt.yy1 * 16.0 - 0.01) / 256.0;
	if (xFlipTexture)
	{
		double tmp = u0;
		u0 = u1;
		u1 = tmp;
	}

	if (tt.xx0 < 0.0 || tt.xx1 > 1.0)
	{
		u0 = ((xt + 0.0f) / 256.0f);
		u1 = ((xt + 15.99f) / 256.0f);
	}
	if (tt.yy0 < 0.0 || tt.yy1 > 1.0)
	{
		v0 = ((yt + 0.0f) / 256.0f);
		v1 = ((yt + 15.99f) / 256.0f);
	}

	double x0 = x + tt.xx0;
	double x1 = x + tt.xx1;
	double y0 = y + tt.yy0;
	double y1 = y + tt.yy1;
	double z0 = z + tt.zz0;

	if (enableAO)
	{
		t.color(colorR0, colorG0, colorB0);
		t.vertexUV(x0, y1, z0, u1, v0);
		t.color(colorR1, colorG1, colorB1);
		t.vertexUV(x1, y1, z0, u0, v0);
		t.color(colorR2, colorG2, colorB2);
		t.vertexUV(x1, y0, z0, u0, v1);
		t.color(colorR3, colorG3, colorB3);
		t.vertexUV(x0, y0, z0, u1, v1);
	}
	else
	{
		t.vertexUV(x0, y1, z0, u1, v0);
		t.vertexUV(x1, y1, z0, u0, v0);
		t.vertexUV(x1, y0, z0, u0, v1);
		t.vertexUV(x0, y0, z0, u1, v1);
	}
}

void TileRenderer::renderSouth(Tile &tt, double x, double y, double z, int_t tex)
{
	Tesselator &t = Tesselator::instance;

	if (fixedTexture >= 0) tex = fixedTexture;
	int_t xt = (tex & 0xF) << 4;
	int_t yt = tex & 0xF0;

	double u0 = (xt + tt.xx0 * 16.0) / 256.0;
	double u1 = (xt + tt.xx1 * 16.0 - 0.01) / 256.0;
	double v0 = (yt + tt.yy0 * 16.0) / 256.0;
	double v1 = (yt + tt.yy1 * 16.0 - 0.01) / 256.0;
	if (xFlipTexture)
	{
		double tmp = u0;
		u0 = u1;
		u1 = tmp;
	}

	if (tt.xx0 < 0.0 || tt.xx1 > 1.0)
	{
		u0 = ((xt + 0.0f) / 256.0f);
		u1 = ((xt + 15.99f) / 256.0f);
	}
	if (tt.yy0 < 0.0 || tt.yy1 > 1.0)
	{
		v0 = ((yt + 0.0f) / 256.0f);
		v1 = ((yt + 15.99f) / 256.0f);
	}

	double x0 = x + tt.xx0;
	double x1 = x + tt.xx1;
	double y0 = y + tt.yy0;
	double y1 = y + tt.yy1;
	double z1 = z + tt.zz1;

	if (enableAO)
	{
		t.color(colorR0, colorG0, colorB0);
		t.vertexUV(x0, y1, z1, u0, v0);
		t.color(colorR1, colorG1, colorB1);
		t.vertexUV(x0, y0, z1, u0, v1);
		t.color(colorR2, colorG2, colorB2);
		t.vertexUV(x1, y0, z1, u1, v1);
		t.color(colorR3, colorG3, colorB3);
		t.vertexUV(x1, y1, z1, u1, v0);
	}
	else
	{
		t.vertexUV(x0, y1, z1, u0, v0);
		t.vertexUV(x0, y0, z1, u0, v1);
		t.vertexUV(x1, y0, z1, u1, v1);
		t.vertexUV(x1, y1, z1, u1, v0);
	}
}

void TileRenderer::renderWest(Tile &tt, double x, double y, double z, int_t tex)
{
	Tesselator &t = Tesselator::instance;

	if (fixedTexture >= 0) tex = fixedTexture;
	int_t xt = (tex & 0xF) << 4;
	int_t yt = tex & 0xF0;

	double u0 = (xt + tt.zz0 * 16.0) / 256.0;
	double u1 = (xt + tt.zz1 * 16.0 - 0.01) / 256.0;
	double v0 = (yt + tt.yy0 * 16.0) / 256.0;
	double v1 = (yt + tt.yy1 * 16.0 - 0.01) / 256.0;
	if (xFlipTexture)
	{
		double tmp = u0;
		u0 = u1;
		u1 = tmp;
	}

	if (tt.zz0 < 0.0 || tt.zz1 > 1.0)
	{
		u0 = ((xt + 0.0f) / 256.0f);
		u1 = ((xt + 15.99f) / 256.0f);
	}
	if (tt.yy0 < 0.0 || tt.yy1 > 1.0)
	{
		v0 = ((yt + 0.0f) / 256.0f);
		v1 = ((yt + 15.99f) / 256.0f);
	}

	double x0 = x + tt.xx0;
	double y0 = y + tt.yy0;
	double y1 = y + tt.yy1;
	double z0 = z + tt.zz0;
	double z1 = z + tt.zz1;

	if (enableAO)
	{
		t.color(colorR0, colorG0, colorB0);
		t.vertexUV(x0, y1, z1, u1, v0);
		t.color(colorR1, colorG1, colorB1);
		t.vertexUV(x0, y1, z0, u0, v0);
		t.color(colorR2, colorG2, colorB2);
		t.vertexUV(x0, y0, z0, u0, v1);
		t.color(colorR3, colorG3, colorB3);
		t.vertexUV(x0, y0, z1, u1, v1);
	}
	else
	{
		t.vertexUV(x0, y1, z1, u1, v0);
		t.vertexUV(x0, y1, z0, u0, v0);
		t.vertexUV(x0, y0, z0, u0, v1);
		t.vertexUV(x0, y0, z1, u1, v1);
	}
}

void TileRenderer::renderEast(Tile &tt, double x, double y, double z, int_t tex)
{
	Tesselator &t = Tesselator::instance;

	if (fixedTexture >= 0) tex = fixedTexture;
	int_t xt = (tex & 0xF) << 4;
	int_t yt = tex & 0xF0;

	double u0 = (xt + tt.zz0 * 16.0) / 256.0;
	double u1 = (xt + tt.zz1 * 16.0 - 0.01) / 256.0;
	double v0 = (yt + tt.yy0 * 16.0) / 256.0;
	double v1 = (yt + tt.yy1 * 16.0 - 0.01) / 256.0;
	if (xFlipTexture)
	{
		double tmp = u0;
		u0 = u1;
		u1 = tmp;
	}

	if (tt.zz0 < 0.0 || tt.zz1 > 1.0)
	{
		u0 = ((xt + 0.0f) / 256.0f);
		u1 = ((xt + 15.99f) / 256.0f);
	}
	if (tt.yy0 < 0.0 || tt.yy1 > 1.0)
	{
		v0 = ((yt + 0.0f) / 256.0f);
		v1 = ((yt + 15.99f) / 256.0f);
	}

	double x1 = x + tt.xx1;
	double y0 = y + tt.yy0;
	double y1 = y + tt.yy1;
	double z0 = z + tt.zz0;
	double z1 = z + tt.zz1;

	if (enableAO)
	{
		t.color(colorR0, colorG0, colorB0);
		t.vertexUV(x1, y0, z1, u0, v1);
		t.color(colorR1, colorG1, colorB1);
		t.vertexUV(x1, y0, z0, u1, v1);
		t.color(colorR2, colorG2, colorB2);
		t.vertexUV(x1, y1, z0, u1, v0);
		t.color(colorR3, colorG3, colorB3);
		t.vertexUV(x1, y1, z1, u0, v0);
	}
	else
	{
		t.vertexUV(x1, y0, z1, u0, v1);
		t.vertexUV(x1, y0, z0, u1, v1);
		t.vertexUV(x1, y1, z0, u1, v0);
		t.vertexUV(x1, y1, z1, u0, v0);
	}
}

void TileRenderer::renderCube(Tile &tile, float alpha)
{
	int shape = tile.getRenderShape();
	Tesselator &t = Tesselator::instance;
	

	if (shape == Tile::SHAPE_BLOCK)
	{
		tile.updateDefaultShape();
		glTranslatef(-0.5f, -0.5f, -0.5f);
		
		float sd = 0.5f;
		float su = 1.0f;
		float sns = 0.8f;
		float sew = 0.6f;
		
		t.begin();
		
		t.color(su, su, su, alpha);
		renderFaceUp(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::DOWN));
		t.color(sd, sd, sd, alpha);
		renderFaceDown(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::UP));
		t.color(sns, sns, sns, alpha);
		renderNorth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::NORTH));
		renderSouth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::SOUTH));
		t.color(sew, sew, sew, alpha);
		renderWest(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::WEST));
		renderEast(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::EAST));
		
		t.end();
		
		glTranslatef(0.5f, 0.5f, 0.5f);
	}
	else if (shape == Tile::SHAPE_CACTUS)
	{
		tile.updateDefaultShape();
		float s = 1.0f / 16.0f;
		glTranslatef(-0.5f, -0.5f, -0.5f);
		
		t.begin();
		t.normal(0.0f, -1.0f, 0.0f);
		renderFaceUp(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::DOWN));
		t.end();
		
		t.begin();
		t.normal(0.0f, 1.0f, 0.0f);
		renderFaceDown(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::UP));
		t.end();
		
		t.begin();
		t.normal(0.0f, 0.0f, -1.0f);
		t.addOffset(0.0f, 0.0f, s);
		renderNorth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::NORTH));
		t.addOffset(0.0f, 0.0f, -s);
		t.end();
		
		t.begin();
		t.normal(0.0f, 0.0f, 1.0f);
		t.addOffset(0.0f, 0.0f, -s);
		renderSouth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::SOUTH));
		t.addOffset(0.0f, 0.0f, s);
		t.end();
		
		t.begin();
		t.normal(-1.0f, 0.0f, 0.0f);
		t.addOffset(s, 0.0f, 0.0f);
		renderWest(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::WEST));
		t.addOffset(-s, 0.0f, 0.0f);
		t.end();
		
		t.begin();
		t.normal(1.0f, 0.0f, 0.0f);
		t.addOffset(-s, 0.0f, 0.0f);
		renderEast(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::EAST));
		t.addOffset(s, 0.0f, 0.0f);
		t.end();
		
		glTranslatef(0.5f, 0.5f, 0.5f);
	}
	
}

void TileRenderer::renderTile(Tile &tile, int_t data)
	{
		Tesselator &t = Tesselator::instance;
		int_t shape = tile.getRenderShape();
		tile.updateDefaultShape();

		if (shape == Tile::SHAPE_BLOCK)
		{
			glTranslatef(-0.5f, -0.5f, -0.5f);
			t.begin();
			t.normal(0.0f, -1.0f, 0.0f);
			renderFaceUp(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::DOWN, data));
			t.end();
			t.begin();
			t.normal(0.0f, 1.0f, 0.0f);
			renderFaceDown(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::UP, data));
			t.end();
			t.begin();
			t.normal(0.0f, 0.0f, -1.0f);
			renderNorth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::NORTH, data));
			t.end();
			t.begin();
			t.normal(0.0f, 0.0f, 1.0f);
			renderSouth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::SOUTH, data));
			t.end();
			t.begin();
			t.normal(-1.0f, 0.0f, 0.0f);
			renderWest(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::WEST, data));
			t.end();
			t.begin();
			t.normal(1.0f, 0.0f, 0.0f);
			renderEast(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::EAST, data));
			t.end();
			glTranslatef(0.5f, 0.5f, 0.5f);
		}
		else if (shape == Tile::SHAPE_CACTUS)
		{
			float s = 1.0f / 16.0f;
			glTranslatef(-0.5f, -0.5f, -0.5f);
			t.begin();
			t.normal(0.0f, -1.0f, 0.0f);
			renderFaceUp(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::DOWN));
			t.end();
			t.begin();
			t.normal(0.0f, 1.0f, 0.0f);
			renderFaceDown(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::UP));
			t.end();
			t.begin();
			t.normal(0.0f, 0.0f, -1.0f);
			t.addOffset(0.0f, 0.0f, s);
			renderNorth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::NORTH));
			t.addOffset(0.0f, 0.0f, -s);
			t.end();
			t.begin();
			t.normal(0.0f, 0.0f, 1.0f);
			t.addOffset(0.0f, 0.0f, -s);
			renderSouth(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::SOUTH));
			t.addOffset(0.0f, 0.0f, s);
			t.end();
			t.begin();
			t.normal(-1.0f, 0.0f, 0.0f);
			t.addOffset(s, 0.0f, 0.0f);
			renderWest(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::WEST));
			t.addOffset(-s, 0.0f, 0.0f);
			t.end();
			t.begin();
			t.normal(1.0f, 0.0f, 0.0f);
			t.addOffset(-s, 0.0f, 0.0f);
			renderEast(tile, 0.0, 0.0, 0.0, tile.getTexture(Facing::EAST));
			t.addOffset(s, 0.0f, 0.0f);
			t.end();
			glTranslatef(0.5f, 0.5f, 0.5f);
		}
	}

	bool TileRenderer::canRender(int_t renderShape)
	{
		return renderShape == Tile::SHAPE_BLOCK || renderShape == Tile::SHAPE_CACTUS;
	}


float TileRenderer::getLiquidHeight(int_t x, int_t y, int_t z, const Material &material)
{
	int_t count = 0;
	float total = 0.0f;

	for (int_t i = 0; i < 4; ++i)
	{
		int_t bx = x - (i & 1);
		int_t bz = z - (i >> 1 & 1);

		if (&level->getMaterial(bx, y + 1, bz) == &material)
			return 1.0f;

		const Material &mat = level->getMaterial(bx, y, bz);
		if (&mat != &material)
		{
			if (!mat.isSolid())
			{
				++total;
				++count;
			}
		}
		else
		{
			int_t data = level->getData(bx, y, bz);
			if (data >= 8 || data == 0)
			{
				total += LiquidTile::getHeight(data) * 10.0f;
				count += 10;
			}
			total += LiquidTile::getHeight(data);
			++count;
		}
	}

	return 1.0f - total / static_cast<float>(count);
}

bool TileRenderer::tesselateLiquidInWorld(Tile &tt, int_t x, int_t y, int_t z)
{
	Tesselator &t = Tesselator::instance;
	int_t col = tt.getColor(*level, x, y, z);
	float cr = static_cast<float>((col >> 16) & 0xFF) / 255.0f;
	float cg = static_cast<float>((col >> 8) & 0xFF) / 255.0f;
	float cb = static_cast<float>(col & 0xFF) / 255.0f;

	bool renderTop = tt.shouldRenderFace(*level, x, y + 1, z, Facing::UP);
	bool renderBot = tt.shouldRenderFace(*level, x, y - 1, z, Facing::DOWN);
	bool renderSides[4] = {
		tt.shouldRenderFace(*level, x, y, z - 1, Facing::NORTH),
		tt.shouldRenderFace(*level, x, y, z + 1, Facing::SOUTH),
		tt.shouldRenderFace(*level, x - 1, y, z, Facing::WEST),
		tt.shouldRenderFace(*level, x + 1, y, z, Facing::EAST)
	};

	if (!renderTop && !renderBot && !renderSides[0] && !renderSides[1] && !renderSides[2] && !renderSides[3])
		return false;

	bool changed = false;
	const Material &material = tt.material;

	float h00 = getLiquidHeight(x, y, z, material);
	float h01 = getLiquidHeight(x, y, z + 1, material);
	float h11 = getLiquidHeight(x + 1, y, z + 1, material);
	float h10 = getLiquidHeight(x + 1, y, z, material);

	if (renderTop)
	{
		changed = true;
		double flowAngle = getLiquidFlowAngle(*level, x, y, z, material);
		bool hasFlow = flowAngle > FLOW_TEXTURE_NONE;
		int_t texTop = tt.getTexture(Facing::UP);
		if (hasFlow)
			texTop = tt.getTexture(Facing::NORTH);
		int_t xt = (texTop & 0xF) << 4;
		int_t yt = texTop & 0xF0;
		double u = (static_cast<double>(xt) + 8.0) / 256.0;
		double v = (static_cast<double>(yt) + 8.0) / 256.0;
		if (!hasFlow)
		{
			flowAngle = 0.0;
		}
		else
		{
			u = (static_cast<double>(xt) + 16.0) / 256.0;
			v = (static_cast<double>(yt) + 16.0) / 256.0;
		}
		double uo = std::sin(flowAngle) * 8.0 / 256.0;
		double vo = std::cos(flowAngle) * 8.0 / 256.0;

		float bright = tt.getBrightness(*level, x, y, z);
		t.color(bright * cr, bright * cg, bright * cb);

		t.vertexUV(x + 0.0, y + h00, z + 0.0, u - vo - uo, v - vo + uo);
		t.vertexUV(x + 0.0, y + h01, z + 1.0, u - vo + uo, v + vo + uo);
		t.vertexUV(x + 1.0, y + h11, z + 1.0, u + vo + uo, v + vo - uo);
		t.vertexUV(x + 1.0, y + h10, z + 0.0, u + vo - uo, v - vo - uo);
	}

	if (renderBot)
	{
		changed = true;
		float bright = tt.getBrightness(*level, x, y - 1, z);
		t.color(0.5f * bright, 0.5f * bright, 0.5f * bright);
		renderFaceDown(tt, static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), tt.getTexture(Facing::DOWN));
	}

	for (int_t side = 0; side < 4; ++side)
	{
		if (!renderSides[side])
			continue;

		changed = true;

		int_t sx = x, sz = z;
		float sH0, sH1;
		float sx0, sx1, sz0, sz1;

		if (side == 0)
		{
			sH0 = h00; sH1 = h10;
			sx0 = static_cast<float>(x); sx1 = static_cast<float>(x + 1);
			sz0 = static_cast<float>(z); sz1 = static_cast<float>(z);
			sz = z - 1;
		}
		else if (side == 1)
		{
			sH0 = h11; sH1 = h01;
			sx0 = static_cast<float>(x + 1); sx1 = static_cast<float>(x);
			sz0 = static_cast<float>(z + 1); sz1 = static_cast<float>(z + 1);
			sz = z + 1;
		}
		else if (side == 2)
		{
			sH0 = h01; sH1 = h00;
			sx0 = static_cast<float>(x); sx1 = static_cast<float>(x);
			sz0 = static_cast<float>(z + 1); sz1 = static_cast<float>(z);
			sx = x - 1;
		}
		else
		{
			sH0 = h10; sH1 = h11;
			sx0 = static_cast<float>(x + 1); sx1 = static_cast<float>(x + 1);
			sz0 = static_cast<float>(z); sz1 = static_cast<float>(z + 1);
			sx = x + 1;
		}

		int_t texSide = tt.getTexture(static_cast<Facing>(side + 2));
		int_t txi = (texSide & 0xF) << 4;
		int_t tyi = texSide & 0xF0;
		double u0 = static_cast<double>(txi) / 256.0;
		double u1 = (static_cast<double>(txi) + 16.0 - 0.01) / 256.0;
		double v0 = (static_cast<double>(tyi) + (1.0f - sH0) * 16.0f) / 256.0;
		double v1 = (static_cast<double>(tyi) + (1.0f - sH1) * 16.0f) / 256.0;
		double vBot = (static_cast<double>(tyi) + 16.0 - 0.01) / 256.0;

		float bright = tt.getBrightness(*level, sx, y, sz);
		float shade = side < 2 ? 0.8f : 0.6f;
		t.color(shade * bright * cr, shade * bright * cg, shade * bright * cb);

		t.vertexUV(sx0, y + sH0, sz0, u0, v0);
		t.vertexUV(sx1, y + sH1, sz1, u1, v1);
		t.vertexUV(sx1, y + 0.0, sz1, u1, vBot);
		t.vertexUV(sx0, y + 0.0, sz0, u0, vBot);
	}

	return changed;
}