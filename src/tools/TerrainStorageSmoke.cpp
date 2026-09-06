#include "tools/TerrainStorageSmoke.h"

#include "client/renderer/TerrainBufferPool.h"
#include "client/renderer/TerrainIndexBuffer.h"
#include "lwjgl/GLContext.h"

#include <algorithm>
#include <iostream>
#include <vector>

static bool drawLastQuad(int quads)
{
	std::vector<float> vertices(static_cast<std::size_t>(quads) * 12, -3.0f);
	const float visibleQuad[] = {-0.5f,-0.5f,0.0f, 0.5f,-0.5f,0.0f, 0.5f,0.5f,0.0f, -0.5f,0.5f,0.0f};
	std::copy(visibleQuad, visibleQuad + 12, vertices.end() - 12);
	GLuint vbo = 0;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
	glVertexPointer(3, GL_FLOAT, 12, nullptr);
	glEnableClientState(GL_VERTEX_ARRAY);
	GLenum type = 0;
	GLuint indices = TerrainIndexBuffer::get(quads, type);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawElements(GL_TRIANGLES, quads * 6, type, nullptr);
	unsigned char pixel[3] = {};
	glReadPixels(32, 32, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
	glDisableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vbo);
	return pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255;
}

static bool checkPoolGrowthAndReuse()
{
	std::shared_ptr<TerrainBufferPool> pool = TerrainBufferPool::get(0, 0, 0);
	TerrainBufferPool::Range first, live, large;
	std::vector<unsigned char> small(128, 0x17), retained(128, 0x6a), growing(262144, 0x91);
	pool->upload(first, small.data(), small.size());
	pool->upload(live, retained.data(), retained.size());
	pool->release(first);
	pool->upload(large, growing.data(), growing.size());
	std::vector<unsigned char> readback(retained.size());
	glBindBuffer(GL_ARRAY_BUFFER, pool->getBuffer());
	glGetBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(live.offset), static_cast<GLsizeiptr>(readback.size()), readback.data());
	bool ok = readback == retained;
	pool->release(large);
	pool->upload(first, small.data(), small.size());
	std::fill(retained.begin(), retained.end(), 0x2c);
	pool->upload(live, retained.data(), retained.size());
	glBindBuffer(GL_ARRAY_BUFFER, pool->getBuffer());
	glGetBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(first.offset), static_cast<GLsizeiptr>(readback.size()), readback.data());
	ok &= readback == small;
	glGetBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(live.offset), static_cast<GLsizeiptr>(readback.size()), readback.data());
	ok &= readback == retained;
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	pool->release(first);
	pool->release(live);
	return ok;
}

int runTerrainStorageSmoke()
{
	lwjgl::GLContext::instantiate();
	glViewport(0, 0, 64, 64);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glClearColor(0, 0, 0, 1);
	glColor3f(1, 1, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	bool ok = drawLastQuad(16384) && drawLastQuad(16385);
	ok &= checkPoolGrowthAndReuse();
	TerrainIndexBuffer::release();
	ok &= drawLastQuad(16385);
	TerrainIndexBuffer::release();
	ok &= glGetError() == GL_NO_ERROR;
	std::cout << (ok ? "Terrain storage smoke passed: 16/32-bit boundary, index recreation, pooled growth and reuse\n" : "Terrain storage smoke FAILED\n");
	return ok ? 0 : 1;
}
