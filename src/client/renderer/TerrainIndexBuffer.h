#pragma once

#include "OpenGL.h"

#include "java/Type.h"

// B173 - Shared element buffer for indexed terrain quads (report: indexed quads).
// The per-quad pattern {0,1,2, 0,2,3} is derived from the actual duplication in
// Tesselator::vertex(): on the fourth vertex it copies slot 0 then slot 2 before
// writing the final vertex, emitting the stream v0,v1,v2,v0,v2,v3.
class TerrainIndexBuffer
{
private:
	// [0] = GL_UNSIGNED_SHORT buffer, [1] = GL_UNSIGNED_INT buffer.
	static GLuint buffers[2];
	static int_t capacity[2]; // in quads

public:
	// Returns an element buffer covering at least `quads` sequential quads and
	// sets `type` to the index width required by that many unique vertices.
	// Leaves GL_ELEMENT_ARRAY_BUFFER unbound.
	static GLuint get(int_t quads, GLenum &type);
	static void release();
};
