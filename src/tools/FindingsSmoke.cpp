#include "tools/FindingsSmoke.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>

#include "client/Minecraft.h"
#include "client/renderer/Tesselator.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/tile/CropsTile.h"
#include "world/level/tile/SlabTile.h"
#include "world/level/tile/entity/MobSpawnerTileEntity.h"
#include "world/level/tile/entity/PistonTileEntity.h"
#include "world/level/tile/entity/SignTileEntity.h"
#include "world/item/Items.h"
#include "world/stats/StatList.h"

namespace FindingsSmoke
{
bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "findings-smoke FAILED: " << message << '\n';
	return condition;
}

bool near(double a, double b)
{
	return std::abs(a - b) < 1.0e-9;
}

class Source : public ChunkSource
{
public:
	std::map<std::pair<int_t, int_t>, std::shared_ptr<LevelChunk>> chunks;
	bool hasChunk(int_t x, int_t z) override { return chunks.count({x, z}) != 0; }
	std::shared_ptr<LevelChunk> getChunk(int_t x, int_t z) override { return chunks.at({x, z}); }
	void postProcess(ChunkSource &, int_t, int_t) override {}
	bool save(bool, std::shared_ptr<ProgressListener>) override { return true; }
	bool tick() override { return false; }
	bool shouldSave() override { return false; }
	jstring gatherStats() override { return u"findings-smoke"; }
};

// Stop at the actual renderer's first brightness read, before it draws the
// selected tile entity. This tests the production distance gate without
// duplicating its comparison or requiring a test hook in LevelRenderer.
struct DispatchReached {};

class World : public Level
{
public:
	std::shared_ptr<Source> source = std::make_shared<Source>();
	int_t cropStage = 0;
	float brightness = 0.6f;
	bool probeDispatch = false;
	World(int_t offset = 0) : Level(u"findings-smoke", Dimension::Id_Normal, 1234567, false)
	{
		setChunkSource(source);
		for (int_t cx = offset / 16 - 2; cx <= offset / 16 + 2; ++cx)
			for (int_t cz = offset / 16 - 2; cz <= offset / 16 + 2; ++cz)
				source->chunks[{cx, cz}] = std::make_shared<LevelChunk>(*this, cx, cz);
	}
	void tile(int_t x, int_t y, int_t z, int_t id)
	{
		source->chunks.at({x >> 4, z >> 4})->blocks[((x & 15) << 11) | ((z & 15) << 7) | y] = static_cast<ubyte_t>(id);
	}
	float getBrightness(int_t, int_t, int_t) override
	{
		if (probeDispatch) throw DispatchReached();
		return brightness;
	}
	int_t getData(int_t, int_t, int_t) override { return cropStage; }
};

class Actor : public Entity
{
public:
	using Entity::fallDistance;
	bool sneaking = false;
	Actor(Level &level, int_t offset, double z0 = 0.85) : Entity(level)
	{
		bb.set(offset + 0.4, 64.0, offset + z0, offset + 1.0, 65.8, offset + z0 + 0.6);
		x = (bb.x0 + bb.x1) / 2.0;
		y = bb.y0;
		z = (bb.z0 + bb.z1) / 2.0;
		onGround = true;
		footSize = 0.5f;
		makeStepSound = false;
	}
	bool isSneaking() override { return sneaking; }
};

bool movement()
{
	bool ok = true;
	for (int_t offset : {0, 1024, -1024})
	{
		World world(offset);
		for (int_t x = -10; x < 10; ++x)
			for (int_t z = -10; z < 10; ++z)
				world.tile(offset + x, 63, offset + z, Tile::cobblestone.id);
		world.tile(offset + 1, 64, offset, Tile::cobblestone.id);
		for (bool stepping : {false, true})
		{
			AABB::resetPool();
			Actor actor(world, offset);
			actor.footSize = stepping ? 0.5f : 0.0f;
			actor.move(0.2, -0.08, 0.2);
			ok &= expect(near(actor.bb.x0, offset + 0.4) && near(actor.bb.y0, 64.0) && near(actor.bb.z0, offset + 1.05),
				"corner displacement must be (0,0,0.2), including translated and no-step controls");
		}
		world.tile(offset + 1, 64, offset, 0);
		Actor unobstructed(world, offset);
		unobstructed.move(0.2, -0.08, 0.2);
		ok &= expect(near(unobstructed.bb.x0, offset + 0.6) && near(unobstructed.bb.z0, offset + 1.05), "unobstructed movement");

		world.tile(offset + 1, 64, offset, Tile::slabSingle.id);
		for (bool sneaking : {false, true})
		{
			AABB::resetPool();
			Actor actor(world, offset, 0.1);
			actor.sneaking = sneaking;
			actor.ySlideOffset = sneaking ? 0.2f : 0.1f;
			const float damped = actor.ySlideOffset * 0.4f;
			actor.move(0.2, -0.08, 0.0);
			const float expectedOffset = static_cast<float>(static_cast<double>(damped) + 0.51);
			ok &= expect(near(actor.bb.x0, offset + 0.6) && near(actor.bb.y0, 64.5), "slab step with pre-move damping and sneaking exception");
			ok &= expect(actor.ySlideOffset == expectedOffset && near(actor.y, 64.5 - expectedOffset), "step smoothing uses Java update order");
		}
		Actor overSlab(world, offset, 0.1);
		overSlab.move(2.0, 0.0, 0.0);
		ok &= expect(near(overSlab.bb.x0, offset + 2.4) && near(overSlab.bb.y0, 64.0) &&
			!overSlab.onGround && overSlab.fallDistance == 0.5f,
			"step settling supplies resolved downward motion to fall-distance accumulation");
		Actor noSlide(world, offset, 0.1);
		noSlide.slide = false;
		noSlide.move(0.2, -0.08, 0.0);
		ok &= expect(near(noSlide.bb.x0, offset + 0.4) && near(noSlide.bb.y0, 64.0), "non-sliding entity retains the ordinary collision result");
		Actor noPhysics(world, offset);
		noPhysics.noPhysics = true;
		noPhysics.ySlideOffset = 0.2f;
		noPhysics.move(0.2, -0.08, 0.2);
		ok &= expect(noPhysics.ySlideOffset == 0.2f && near(noPhysics.bb.y0, 63.92), "noPhysics bypasses damping and clipping");
	}
	std::cout << "findings-smoke movement: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

bool cropMesh(TileRenderer &renderer, World &world, MeshCapture::Format format, int_t overrideTexture)
{
	MeshCapture mesh;
	mesh.format = format;
	Tesselator &t = Tesselator::instance;
	t.offset(0.0, 0.0, 0.0);
	t.captureTo(&mesh);
	t.begin();
	bool emitted = true;
	if (overrideTexture >= 0)
		renderer.tesselateInWorld(Tile::crops, -3, 64, 5, overrideTexture);
	else
		emitted = renderer.tesselateInWorld(Tile::crops, -3, 64, 5);
	t.end();
	t.captureTo(nullptr);
	const bool indexed = B173_INDEXED_TERRAIN && format == MeshCapture::Format::Terrain;
	const int_t stride = format == MeshCapture::Format::Terrain ? TERRAIN_VERTEX_STRIDE : 32;
	const int_t perQuad = indexed ? 4 : 6;
	if (!emitted || mesh.vertices != 8 * perQuad || mesh.data.size() != static_cast<std::size_t>(mesh.vertices * stride) || !mesh.hasTexture || !mesh.hasColor)
		return false;
	// Endpoint order from Java TileRenderer.tesselateRowTexture (1417-1470).
	const float edges[8][4] = {
		{0.25f, 0, 0.25f, 1}, {0.25f, 1, 0.25f, 0},
		{0.75f, 1, 0.75f, 0}, {0.75f, 0, 0.75f, 1},
		{0, 0.25f, 1, 0.25f}, {1, 0.25f, 0, 0.25f},
		{1, 0.75f, 0, 0.75f}, {0, 0.75f, 1, 0.75f}
	};
	const int_t triangles[6] = {0, 1, 2, 0, 2, 3};
	const int_t texture = overrideTexture >= 0 ? overrideTexture : Tile::crops.tex + world.cropStage;
	const float u0 = ((texture & 15) << 4) / 256.0f;
	const float u1 = (((texture & 15) << 4) + 15.99f) / 256.0f;
	const float v0 = (texture & 240) / 256.0f;
	const float v1 = ((texture & 240) + 15.99f) / 256.0f;
	const unsigned char shade = static_cast<unsigned char>(world.brightness * 255.0f);
	for (int_t q = 0; q < 8; ++q)
		for (int_t i = 0; i < perQuad; ++i)
		{
			const int_t corner = indexed ? i : triangles[i];
			const bool end = corner >= 2;
			const bool top = corner == 0 || corner == 3;
			const float expected[5] = {-3 + edges[q][end ? 2 : 0], 63.9375f + (top ? 1 : 0), 5 + edges[q][end ? 3 : 1], end ? u1 : u0, top ? v0 : v1};
			const char *bytes = mesh.data.data() + (q * perQuad + i) * stride;
			float actual[5];
			std::memcpy(actual, bytes, sizeof(actual));
			if (std::memcmp(actual, expected, sizeof(expected)) != 0) return false;
			for (int_t c = 0; c < 4; ++c)
				if (static_cast<unsigned char>(bytes[20 + c]) != (c == 3 ? 255 : shade)) return false;
		}
	return true;
}

bool crops()
{
	World world;
	bool ok = true;
	for (bool fancy : {false, true})
	{
		TileRenderer renderer(&world, fancy, fancy);
		for (int_t stage = 0; stage < 8; ++stage)
		{
			world.cropStage = stage;
			for (float brightness : {0.0f, 0.6f, 1.0f})
			{
				world.brightness = brightness;
				for (auto format : {MeshCapture::Format::Terrain, MeshCapture::Format::Arrays})
				{
					ok &= cropMesh(renderer, world, format, -1);
					ok &= cropMesh(renderer, world, format, 240);
					ok &= cropMesh(renderer, world, format, -1); // override must reset
				}
			}
		}
	}
	expect(ok, "crop stage geometry, double-sided UV order, brightness and overlay reset");
	std::cout << "findings-smoke crops: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}

class Visible : public Culler
{
public:
	bool isVisible(AABB &) override { return true; }
	bool cubeInFrustum(double, double, double, double, double, double) override { return true; }
	bool cubeFullyInFrustum(double, double, double, double, double, double) override { return true; }
	void prepare(double, double, double) override {}
};

bool tileEntityDistance()
{
	auto world = std::make_shared<World>();
	Minecraft mc(64, 64, false);
	mc.options.viewDistance = 3;
	mc.font = std::make_unique<Font>(mc.options, u"/font/default.png", mc.textures);
	mc.player = std::make_shared<LocalPlayer>(mc, *world, nullptr, Dimension::Id_Normal);
	mc.levelRenderer.setLevel(world);
	Visible culler;
	Vec3 cam(0.0, 64.0, 0.0);
	// Consume the normal renderer startup delay with no tile entities.
	mc.levelRenderer.renderEntities(cam, culler, 0.0f);
	mc.levelRenderer.renderEntities(cam, culler, 0.0f);
	std::shared_ptr<TileEntity> entities[] = {
		std::make_shared<SignTileEntity>(), std::make_shared<PistonTileEntity>(), std::make_shared<MobSpawnerTileEntity>()
	};
	bool ok = true;
	for (auto &te : entities)
	{
		te->level = world;
		te->x = 0; te->y = 64; te->z = 0;
		mc.levelRenderer.renderableTileEntities = {te};
		for (int_t axis = 0; axis < 3; ++axis)
			for (double distance : {63.999, 64.0, 64.001, -64.0, std::numeric_limits<double>::quiet_NaN()})
			{
				double position[3] = {0.5, 64.5, 0.5};
				position[axis] += distance;
				// The old and current positions straddle the tested interpolated
				// camera, so using either endpoint would fail the boundary tests.
				mc.player->xOld = position[0] - 2.0; mc.player->x = position[0] + 2.0;
				mc.player->yOld = position[1] - 2.0; mc.player->y = position[1] + 2.0;
				mc.player->zOld = position[2] - 2.0; mc.player->z = position[2] + 2.0;
				bool reached = false;
				world->probeDispatch = true;
				try { mc.levelRenderer.renderEntities(cam, culler, 0.5f); }
				catch (const DispatchReached &) { reached = true; }
				world->probeDispatch = false;
				ok &= expect(reached == (distance == 63.999), "tile-entity dispatch strict center distance on all axes and interpolated camera");
			}
	}
	mc.levelRenderer.renderableTileEntities.clear();
	mc.levelRenderer.setLevel(nullptr);
	std::cout << "findings-smoke tile entities: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok;
}
}

int runFindingsSmoke()
{
	Tile::initTiles();
	Items::initItems();
	StatList::init();
	bool ok = FindingsSmoke::movement();
	ok &= FindingsSmoke::crops();
	ok &= FindingsSmoke::tileEntityDistance();
	std::cout << "findings-smoke: " << (ok ? "PASS" : "FAIL") << '\n';
	return ok ? 0 : 1;
}
