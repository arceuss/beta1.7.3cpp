#include "client/renderer/Tesselator.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

#include "lwjgl/GLContext.h"
#include "java/Number.h"

Tesselator Tesselator::instance(MAX_FLOATS);

Tesselator::Tesselator(int_t size)
{
	lwjgl::GLContext::instantiate();

	// Initialize buffer
	this->size = size;

	buffer = std::make_unique<char[]>(size * 4);
	buffer_p = buffer.get();
	buffer_e = buffer.get() + (size * 4);

	// Setup VBO
	vboMode = USE_VBO && lwjgl::GLContext::getCapabilities()["GL_ARB_vertex_buffer_object"];
	if (vboMode)
	{
		vboIds = std::make_unique<GLuint[]>(vboCounts);
		glGenBuffers(vboCounts, vboIds.get());
	}
}

Tesselator Tesselator::getUniqueInstance(int_t size)
{
	return Tesselator(size);
}

void Tesselator::end()
{
	if (!tesselating)
		throw std::runtime_error("Not tesselating!");
	tesselating = false;

	if (vertices > 0 && capture != nullptr)
	{
		if (B173_COMPACT_TERRAIN_VERTEX && capture->format == MeshCapture::Format::Terrain)
		{
			std::size_t oldSize = capture->data.size();
			capture->data.resize(oldSize + static_cast<std::size_t>(vertices) * TERRAIN_VERTEX_STRIDE);
			char *dst = capture->data.data() + oldSize;
			const char *src = buffer.get();
			for (int_t i = 0; i < vertices; i++, src += 32, dst += TERRAIN_VERTEX_STRIDE)
				std::memcpy(dst, src, TERRAIN_VERTEX_STRIDE);
		}
		else
		{
			capture->data.insert(capture->data.end(), buffer.get(), buffer_p);
		}
		capture->vertices += vertices;
#if B173_INDEXED_TERRAIN
		// vertex() skipped the quad-to-triangle duplication for captured quads,
		// so the data holds exactly four vertices per quad.
		if (capture->format == MeshCapture::Format::Terrain && draw_mode == GL_QUADS && TRIANGLE_MODE)
			capture->quads += vertices / 4;
#endif
		capture->hasTexture = hasTexture;
		capture->hasColor = hasColor;
		capture->hasNormal = hasNormal;
		capture->mode = (draw_mode == GL_QUADS && TRIANGLE_MODE) ? GL_TRIANGLES : draw_mode;
	}
	else if (vertices > 0)
	{
		// Bind VBO
		if (vboMode)
		{
			vboId = (vboId + 1) % vboCounts;
			glBindBuffer(GL_ARRAY_BUFFER, vboIds[vboId]);
			glBufferData(GL_ARRAY_BUFFER, buffer_p - buffer.get(), buffer.get(), GL_STREAM_DRAW);
		}

		// Setup attributes
		char *vbo_base = vboMode ? reinterpret_cast<char *>(0) : reinterpret_cast<char *>(buffer.get());

		if (hasTexture)
		{
			glTexCoordPointer(2, GL_FLOAT, 32, reinterpret_cast<GLvoid *>(vbo_base + 12));
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		if (hasColor)
		{
			glColorPointer(4, GL_UNSIGNED_BYTE, 32, reinterpret_cast<GLvoid *>(vbo_base + 20));
			glEnableClientState(GL_COLOR_ARRAY);
		}

		if (hasNormal)
		{
			glNormalPointer(GL_BYTE, 32, reinterpret_cast<GLvoid *>(vbo_base + 24));
			glEnableClientState(GL_NORMAL_ARRAY);
		}

		glVertexPointer(3, GL_FLOAT, 32, reinterpret_cast<GLvoid *>(vbo_base + 0));
		glEnableClientState(GL_VERTEX_ARRAY);

		// Draw arrays
		if (draw_mode == GL_QUADS && TRIANGLE_MODE)
			glDrawArrays(GL_TRIANGLES, 0, vertices);
		else
			glDrawArrays(draw_mode, 0, vertices);

		// Reset attributes
		glDisableClientState(GL_VERTEX_ARRAY);
		if (hasTexture)
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		if (hasColor)
			glDisableClientState(GL_COLOR_ARRAY);
		if (hasNormal)
			glDisableClientState(GL_NORMAL_ARRAY);
	}

	clear();
}

void Tesselator::clear()
{
	vertices = 0;
	buffer_p = buffer.get();
	count = 0;
}

void Tesselator::begin()
{
	begin(GL_QUADS);
}

void Tesselator::begin(GLenum mode)
{
	if (tesselating)
		throw std::runtime_error("Already tesselating!");
	tesselating = true;

	clear();
	draw_mode = mode;
	hasNormal = false;
	hasColor = false;
	hasTexture = false;
	noColorFlag = false;
}

void Tesselator::tex(double u, double v)
{
	hasTexture = true;
	this->u = u;
	this->v = v;
}

void Tesselator::color(float r, float g, float b)
{
	color(Java::numberToInt(r * 255.0f), Java::numberToInt(g * 255.0f), Java::numberToInt(b * 255.0f));
}

void Tesselator::color(float r, float g, float b, float a)
{
	color(Java::numberToInt(r * 255.0f), Java::numberToInt(g * 255.0f), Java::numberToInt(b * 255.0f), Java::numberToInt(a * 255.0f));
}

void Tesselator::color(int_t r, int_t g, int_t b)
{
	color(r, g, b, 255);
}

void Tesselator::color(int_t r, int_t g, int_t b, int_t a)
{
	if (noColorFlag)
		return;

	if (r > 255)
		r = 255;
	if (g > 255)
		g = 255;
	if (b > 255)
		b = 255;
	if (a > 255)
		a = 255;

	if (r < 0)
		r = 0;
	if (g < 0)
		g = 0;
	if (b < 0)
		b = 0;
	if (a < 0)
		a = 0;

	hasColor = true;
	const unsigned char rgba[] = {static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
	std::memcpy(&col, rgba, sizeof(col));
}

void Tesselator::vertexUV(double x, double y, double z, double u, double v)
{
	tex(u, v);
	vertex(x, y, z);
}

void Tesselator::vertex(double x, double y, double z)
{
	// Convert quads to triangles
	count++;

	bool duplicateQuad = draw_mode == GL_QUADS && TRIANGLE_MODE && (count % 4) == 0;
#if B173_INDEXED_TERRAIN
	// B173 - Captured terrain keeps four vertices per quad; the shared index
	// buffer replays the exact duplication order (v0,v1,v2,v0,v2,v3) at draw time.
	if (capture != nullptr && capture->format == MeshCapture::Format::Terrain)
		duplicateQuad = false;
#endif
	if (duplicateQuad)
	{
		for (int_t i = 0; i < 2; i++)
		{
			int_t j = 8 * (3 - i);
			char *from_p = buffer_p - j * 4;

			if (hasTexture)
				std::memcpy(buffer_p + 3 * 4, from_p + 3 * 4, 2 * 4);
			if (hasColor)
				std::memcpy(buffer_p + 5 * 4, from_p + 5 * 4, 4);
			std::memcpy(buffer_p + 0 * 4, from_p + 0 * 4, 3 * 4);

			vertices++;
			buffer_p += 8 * 4;
		}
	}

	if (hasTexture)
	{
		float fu = static_cast<float>(u);
		float fv = static_cast<float>(v);
		std::memcpy(buffer_p + 3 * 4, &fu, 1 * 4);
		std::memcpy(buffer_p + 4 * 4, &fv, 1 * 4);
	}
	if (hasColor)
		std::memcpy(buffer_p + 5 * 4, &col, 1 * 4);
	if (hasNormal)
		std::memcpy(buffer_p + 6 * 4, &normalValue, 1 * 4);

	float fx = static_cast<float>(x + xo);
	float fy = static_cast<float>(y + yo);
	float fz = static_cast<float>(z + zo);

	std::memcpy(buffer_p + 0 * 4, &fx, 1 * 4);
	std::memcpy(buffer_p + 1 * 4, &fy, 1 * 4);
	std::memcpy(buffer_p + 2 * 4, &fz, 1 * 4);

	buffer_p += 8 * 4;

	vertices++;
	if ((vertices % 4) == 0 && buffer_p >= (buffer_e - 32 * 4))
	{
		end();
		tesselating = true;
	}
}

void Tesselator::color(int_t rgb)
{
	int r = (rgb >> 16) & 0xFF;
	int g = (rgb >> 8) & 0xFF;
	int b = (rgb >> 0) & 0xFF;
	color(r, g, b);
}

void Tesselator::color(int_t rgb, int_t a)
{
	int r = (rgb >> 16) & 0xFF;
	int g = (rgb >> 8) & 0xFF;
	int b = (rgb >> 0) & 0xFF;
	color(r, g, b, a);
}

void Tesselator::noColor()
{
	noColorFlag = true;
}

void Tesselator::normal(float x, float y, float z)
{
	if (!tesselating)
		std::cout << "But..\n";

	hasNormal = true;
	uint_t bx = static_cast<uint_t>(Java::numberToInt(x * 128.0f)) & 255u;
	uint_t by = static_cast<uint_t>(Java::numberToInt(y * 127.0f)) & 255u;
	uint_t bz = static_cast<uint_t>(Java::numberToInt(z * 127.0f)) & 255u;
	// Java promotes each signed byte before the shifts and ORs.
	if (bx & 128u) bx |= 0xffffff00u;
	if (by & 128u) by |= 0xffffff00u;
	if (bz & 128u) bz |= 0xffffff00u;
	normalValue = Java::intFromBits(bx | (by << 8) | (bz << 16));
}

void Tesselator::offset(double x, double y, double z)
{
	xo = x;
	yo = y;
	zo = z;
}

void Tesselator::addOffset(float x, float y, float z)
{
	xo += x;
	yo += y;
	zo += z;
}

void Tesselator::captureTo(MeshCapture *target)
{
	capture = target;
}
