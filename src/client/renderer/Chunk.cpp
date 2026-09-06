#include "client/renderer/Chunk.h"

#include "client/renderer/Tesselator.h"
#include "client/renderer/entity/EntityRenderer.h"
#include "client/renderer/TileRenderer.h"

#include "world/level/Region.h"

#include "util/Mth.h"
#include "util/Profiler.h"
#include "client/renderer/TerrainIndexBuffer.h"
#include "client/renderer/TerrainVertex.h"
#include <algorithm>
#include <cstdint>

int_t Chunk::updates = 0;
bool Chunk::useRegionBuffers = B173_REGION_RENDERER != 0;

Tesselator &Chunk::t = Tesselator::instance;

void (*Chunk::rebuildObserver)(int_t x, int_t y, int_t z) = nullptr;
void (*Chunk::publishObserver)(int_t x, int_t y, int_t z, int_t layer, const unsigned char *data, std::size_t bytes, int_t vertices, int_t quads, int_t stride) = nullptr;

// B173 - Reusable staging capture (report: retain terrain mesh staging
// allocations). Rebuilds run on the render thread only; a future snapshot
// worker pipeline needs one scratch per worker instead.
static MeshCapture chunkMeshScratch;

Chunk::Chunk(Level &level, std::vector<std::shared_ptr<TileEntity>> &globalRenderableTileEntities, int_t x, int_t y, int_t z, int_t size, int_t lists, bool ambientOcclusion, bool fancyGraphics) : level(level), globalRenderableTileEntities(globalRenderableTileEntities), ambientOcclusion(ambientOcclusion), fancyGraphics(fancyGraphics)
{
	xs = ys = zs = size;
	radius = Mth::sqrt(static_cast<float>(xs * xs + ys * ys + zs * zs)) / 2.0f;
	this->lists = lists;

	this->x = -999;
	setPos(x, y, z);

	dirty = false;
}

void Chunk::setPos(int_t x, int_t y, int_t z)
{
	if (this->x == x && this->y == y && this->z == z) return;

	reset();
	this->x = x;
	this->y = y;
	this->z = z;
	xm = x + xs / 2;
	ym = y + ys / 2;
	zm = z + zs / 2;

	xRenderOffs = x & 0x3FF;
	yRenderOffs = y;
	zRenderOffs = z & 0x3FF;
	xRender = x - xRenderOffs;
	yRender = y - yRenderOffs;
	zRender = z - zRenderOffs;

	float g = 6.0f;
	bb.reset(AABB::newPermanent(x - g, y - g, z - g, x + xs + g, y + ys + g, z + zs + g));

	glNewList(lists + 2, GL_COMPILE);
	EntityRenderer::renderFlat(*bb);
	glEndList();

	setDirty();
}

void Chunk::translateToPos()
{
	glTranslatef(static_cast<float>(xRenderOffs), static_cast<float>(yRenderOffs), static_cast<float>(zRenderOffs));
}

void Chunk::rebuild()
{
	if (!dirty) return;
	if (rebuildObserver)
		rebuildObserver(x, y, z);
	updates++;

	int_t x0 = x;
	int_t y0 = y;
	int_t z0 = z;
	int_t x1 = x + xs;
	int_t y1 = y + ys;
	int_t z1 = z + zs;

	empty.fill(true);

	LevelChunk::touchedSky = false;
	Profiler::Scope captureProfile(Profiler::Section::ChunkRebuildCapture);

	std::unordered_set<std::shared_ptr<TileEntity>> oldTileEntities;
	oldTileEntities.insert(renderableTileEntities.begin(), renderableTileEntities.end());
	renderableTileEntities.clear();

	int_t r = 1;
	Region region(level, x0 - r, y0 - r, z0 - r, x1 + r, y1 + r, z1 + r);
	TileRenderer tileRenderer(&region, ambientOcclusion, fancyGraphics);
	captureProfile.finish();

	MeshCapture &mesh = chunkMeshScratch;
	for (int_t i = 0; i < 2; i++)
	{
		Profiler::Scope tessellationProfile(Profiler::Section::ChunkTessellation);
		bool renderNextLayer = false;
		bool rendered = false;

		bool started = false;

		for (int_t y = y0; y < y1; y++)
		{
			for (int_t z = z0; z < z1; z++)
			{
				for (int_t x = x0; x < x1; x++)
				{
					int_t tileId = region.getTile(x, y, z);
					if (tileId > 0)
					{
						if (!started)
						{
							started = true;

							mesh.resetKeepCapacity();
							t.captureTo(&mesh);
							t.begin();
							t.offset(-this->x, -this->y, -this->z);
						}

						if (i == 0 && Tile::isEntityTile[tileId])
						{
							auto tileEntity = level.getTileEntity(x, y, z);
							if (tileEntity != nullptr)
								renderableTileEntities.push_back(tileEntity);
						}

						Tile *tile = Tile::tiles[tileId];
						int_t renderLayer = tile->getRenderLayer();
						if (renderLayer != i)
						{
							renderNextLayer = true;
						}
						else if (renderLayer == i)
						{
							rendered |= tileRenderer.tesselateInWorld(*tile, x, y, z);
						}
					}
				}
			}
		}
		tessellationProfile.finish();

		if (started)
		{
			Profiler::Scope publicationProfile(Profiler::Section::ChunkPublication);
			t.end();
			t.captureTo(nullptr);
			t.offset(0.0, 0.0, 0.0);
			if (publishObserver)
				publishObserver(this->x, this->y, this->z, i, reinterpret_cast<const unsigned char *>(mesh.data.data()), mesh.data.size(), mesh.vertices, mesh.quads, TERRAIN_VERTEX_STRIDE);
			if (useRegionBuffers)
			{
				if (!meshPools[i])
					meshPools[i] = TerrainBufferPool::get(xRender, zRender, i);
				meshPools[i]->upload(meshRanges[i], mesh.data.data(), mesh.data.size());
				if (meshBuffers[i] != 0)
				{
					glDeleteBuffers(1, &meshBuffers[i]);
					meshBuffers[i] = 0;
				}
			}
			else
			{
				if (meshPools[i])
				{
					meshPools[i]->release(meshRanges[i]);
					meshPools[i].reset();
				}
				if (meshBuffers[i] == 0)
					glGenBuffers(1, &meshBuffers[i]);
				glBindBuffer(GL_ARRAY_BUFFER, meshBuffers[i]);
				glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.data.size()), mesh.data.data(), GL_STATIC_DRAW);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			meshVertices[i] = mesh.vertices;
			meshQuads[i] = mesh.quads;
			meshTexture[i] = mesh.hasTexture;
			meshColor[i] = mesh.hasColor;
			meshNormal[i] = mesh.hasNormal;
			meshMode[i] = mesh.mode;
		}
		else
		{
			rendered = false;
		}

		if (rendered) empty[i] = false;
		if (!renderNextLayer) break;
	}

	if (!oldTileEntities.empty())
	{
		globalRenderableTileEntities.erase(
			std::remove_if(globalRenderableTileEntities.begin(), globalRenderableTileEntities.end(),
				[&](const std::shared_ptr<TileEntity> &tileEntity) { return oldTileEntities.find(tileEntity) != oldTileEntities.end(); }),
			globalRenderableTileEntities.end());
	}
	globalRenderableTileEntities.insert(globalRenderableTileEntities.end(), renderableTileEntities.begin(), renderableTileEntities.end());

	skyLit = LevelChunk::touchedSky;
	compiled = true;
}

float Chunk::distanceToSqr(Entity &player)
{
	float dx = static_cast<float>(player.x - static_cast<double>(xm));
	float dy = static_cast<float>(player.y - static_cast<double>(ym));
	float dz = static_cast<float>(player.z - static_cast<double>(zm));
	return dx * dx + dy * dy + dz * dz;
}

float Chunk::squishedDistanceToSqr(Entity &player)
{
	float dx = static_cast<float>(player.x - static_cast<double>(xm));
	float dy = static_cast<float>(player.y - static_cast<double>(ym)) * 2.0f;
	float dz = static_cast<float>(player.z - static_cast<double>(zm));
	return dx * dx + dy * dy + dz * dz;
}

void Chunk::reset()
{
	empty.fill(true);
	visible = false;
	compiled = false;
	releasePooledMeshes();
}

void Chunk::remove()
{
	reset();
}

bool Chunk::hasMesh(int_t layer)
{
	return visible && !empty[layer];
}

void Chunk::draw(int_t layer)
{
	// Same sequence the compiled display list contained: WorldRenderer.updateRenderer setup, then Tessellator.draw.
	glPushMatrix();
	translateToPos();

	float ss = 1.0000001f;
	glTranslatef(-zs / 2.0f, -ys / 2.0f, -zs / 2.0f);
	glScalef(ss, ss, ss);
	glTranslatef(zs / 2.0f, ys / 2.0f, zs / 2.0f);

	if (meshVertices[layer] > 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, meshPools[layer] ? meshPools[layer]->getBuffer() : meshBuffers[layer]);
		const std::uintptr_t base = meshPools[layer] ? meshRanges[layer].offset : 0;
		if (meshTexture[layer])
		{
			glTexCoordPointer(2, GL_FLOAT, TERRAIN_VERTEX_STRIDE, reinterpret_cast<const void *>(base + TERRAIN_UV_OFFSET));
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		if (meshColor[layer])
		{
			glColorPointer(4, GL_UNSIGNED_BYTE, TERRAIN_VERTEX_STRIDE, reinterpret_cast<const void *>(base + TERRAIN_COLOR_OFFSET));
			glEnableClientState(GL_COLOR_ARRAY);
		}
		if (meshNormal[layer])
		{
			glNormalPointer(GL_BYTE, TERRAIN_VERTEX_STRIDE, reinterpret_cast<const void *>(base + TERRAIN_NORMAL_OFFSET));
			glEnableClientState(GL_NORMAL_ARRAY);
		}
		glVertexPointer(3, GL_FLOAT, TERRAIN_VERTEX_STRIDE, reinterpret_cast<const void *>(base + TERRAIN_POS_OFFSET));
		glEnableClientState(GL_VERTEX_ARRAY);

		if (meshQuads[layer] > 0)
		{
			// B173 - Indexed terrain: the shared IBO replays the exact
			// Tesselator duplication order over four unique vertices per quad.
			GLenum indexType = 0;
			GLuint ibo = TerrainIndexBuffer::get(meshQuads[layer], indexType);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
			glDrawElements(GL_TRIANGLES, meshQuads[layer] * 6, indexType, nullptr);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
		else
		{
			glDrawArrays(meshMode[layer], 0, meshVertices[layer]);
		}

		glDisableClientState(GL_VERTEX_ARRAY);
		if (meshTexture[layer])
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		if (meshColor[layer])
			glDisableClientState(GL_COLOR_ARRAY);
		if (meshNormal[layer])
			glDisableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	glPopMatrix();
}

Chunk::~Chunk()
{
	releasePooledMeshes();
	for (GLuint &buffer : meshBuffers)
	{
		if (buffer != 0)
			glDeleteBuffers(1, &buffer);
	}
}

void Chunk::releasePooledMeshes()
{
	for (int layer = 0; layer < 2; ++layer)
	{
		if (meshPools[layer])
		{
			meshPools[layer]->release(meshRanges[layer]);
			meshPools[layer].reset();
		}
	}
}

void Chunk::cull(Culler &culler)
{
	visible = culler.isVisible(*bb);
}

void Chunk::renderBB()
{
	glCallList(lists + 2);
}

bool Chunk::isEmpty()
{
	if (!compiled) return false;
	return empty[0] && empty[1];
}

void Chunk::setDirty()
{
	dirty = true;
}
