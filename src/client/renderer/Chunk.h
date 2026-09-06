#pragma once

#include <array>

#include "client/renderer/Tesselator.h"
#include "client/renderer/TerrainBufferPool.h"
#include "client/renderer/culling/Culler.h"

#include "world/level/Level.h"
#include "world/level/tile/entity/TileEntity.h"

#include "java/Type.h"

class Chunk
{
public:
	Level &level;

private:
	int_t lists = -1;

	static Tesselator &t;
	// B173 - Chunk geometry lives in buffer objects instead of compiled display lists.
	// The display list body (matrix setup, client arrays, draw) is replayed by draw().
	GLuint meshBuffers[2] = {0, 0};
	std::shared_ptr<TerrainBufferPool> meshPools[2];
	TerrainBufferPool::Range meshRanges[2];
	GLsizei meshVertices[2] = {0, 0};
	// B173 - Quad count for indexed terrain draws; 0 means legacy array draw.
	GLsizei meshQuads[2] = {0, 0};
	bool meshTexture[2] = {false, false};
	bool meshColor[2] = {false, false};
	bool meshNormal[2] = {false, false};
	GLenum meshMode[2] = {0, 0};

public:
	int_t x = 0, y = 0, z = 0;
	int_t xs = 0, ys = 0, zs = 0;

	static int_t updates;
	static bool useRegionBuffers;

	int_t xRender = 0, yRender = 0, zRender = 0;
	int_t xRenderOffs = 0, yRenderOffs = 0, zRenderOffs = 0;

	bool visible = false;
	std::array<bool, 2> empty = {};

	int_t xm = 0, ym = 0, zm = 0;

	float radius = 0.0f;
	bool dirty = false;
	// B173 - Mirrors membership in LevelRenderer::dirtyChunks so enqueue checks
	// are O(1). Set exactly when pushed, cleared exactly when the entry leaves
	// the queue; the queue itself stays an ordered vector.
	bool queuedDirty = false;

	std::unique_ptr<AABB> bb;

	int_t id = 0;

	bool occlusion_visible = false;
	bool occlusion_querying = false;
	int_t occlusion_id = 0;

	bool skyLit = false;

	// B173 - Stress-parity observers (compiled consumers live in tools/stress).
	// Called only when set; zero overhead otherwise.
	static void (*rebuildObserver)(int_t x, int_t y, int_t z);
	static void (*publishObserver)(int_t x, int_t y, int_t z, int_t layer, const unsigned char *data, std::size_t bytes, int_t vertices, int_t quads, int_t stride);

private:
	bool compiled = false;

public:
	std::vector<std::shared_ptr<TileEntity>> renderableTileEntities;

private:
	std::vector<std::shared_ptr<TileEntity>> &globalRenderableTileEntities;
	bool ambientOcclusion = false;
	bool fancyGraphics = false;

public:
	Chunk(Level &level, std::vector<std::shared_ptr<TileEntity>> &globalRenderableTileEntities, int_t x, int_t y, int_t z, int_t size, int_t lists, bool ambientOcclusion, bool fancyGraphics);
	~Chunk();
	Chunk(const Chunk &) = delete;
	Chunk &operator=(const Chunk &) = delete;

	void setPos(int_t x, int_t y, int_t z);

private:
	void translateToPos();
	void releasePooledMeshes();

public:
	void rebuild();

	float distanceToSqr(Entity &player);
	float squishedDistanceToSqr(Entity &player);

	void reset();
	void remove();

	bool hasMesh(int_t layer);
	void draw(int_t layer);

	void cull(Culler &culler);

	void renderBB();

	bool isEmpty();

	void setDirty();
};
