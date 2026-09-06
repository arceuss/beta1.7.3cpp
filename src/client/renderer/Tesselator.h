#pragma once

#include <memory>
#include <vector>

#include "java/Type.h"

#include "client/renderer/TerrainVertex.h"

#include "OpenGL.h"

#include "util/Memory.h"

// B173 - CPU copy of one begin/end sequence, used to upload chunk meshes as buffer objects.
// Terrain captures hold four vertices per quad at TERRAIN_VERTEX_STRIDE bytes and
// draw through the shared TerrainIndexBuffer. Arrays captures keep the raw 32-byte
// triangle stream the immediate path would have drawn.
struct MeshCapture
{
	enum class Format { Terrain, Arrays };
	std::vector<char> data;
	int_t vertices = 0;
	int_t quads = 0;
	bool hasTexture = false;
	bool hasColor = false;
	bool hasNormal = false;
	GLenum mode = 0;
	Format format = Format::Terrain;

	// Report: retain staging allocations across rebuilds; clear() keeps capacity.
	void resetKeepCapacity()
	{
		data.clear();
		vertices = 0;
		quads = 0;
		hasTexture = false;
		hasColor = false;
		hasNormal = false;
		mode = 0;
	}
};

class Tesselator
{
private:
	static constexpr bool TRIANGLE_MODE = true;
	static constexpr bool USE_VBO = false;
	static constexpr int_t MAX_MEMORY_USE = 0x1000000;
	static constexpr int_t MAX_FLOATS = 0x200000;

	// Buffer
	std::unique_ptr<char[]> buffer;
	char *buffer_p;
	char *buffer_e;

	// Tesselation state
	int_t vertices = 0;

	double u = 0.0, v = 0.0;
	int_t col = 0;

	bool hasColor = false;
	bool hasTexture = false;
	bool hasNormal = false;

	int_t count = 0;
	bool noColorFlag = false;

	GLenum draw_mode = 0;

	double xo = 0.0;
	double yo = 0.0;
	double zo = 0.0;

	int_t normalValue = 0;

public:
	static Tesselator instance;

private:
	// State
	bool tesselating = false;
	bool vboMode = false;

	// VBO state
	std::unique_ptr<GLuint[]> vboIds;
	int_t vboId = 0;
	int_t vboCounts = 10;

	// Buffer state
	int_t size = 0;
	MeshCapture *capture = nullptr;

public:
	Tesselator(int_t size);

	Tesselator getUniqueInstance(int_t size);

	// Tessellator functions
	void end();
	void clear();
	void begin();
	void begin(GLenum mode);
	void tex(double u, double v);
	void color(float r, float g, float b);
	void color(float r, float g, float b, float a);
	void color(int_t r, int_t g, int_t b);
	void color(int_t r, int_t g, int_t b, int_t a);
	void vertexUV(double x, double y, double z, double u, double v);
	void vertex(double x, double y, double z);
	void color(int_t rgb);
	void color(int_t rgb, int_t a);
	void noColor();
	void normal(float x, float y, float z);
	void offset(double x, double y, double z);
	void addOffset(float x, float y, float z);
	// While set, end() appends the vertex bytes to the capture instead of drawing.
	void captureTo(MeshCapture *target);
};
