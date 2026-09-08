#define SDL_MAIN_HANDLED
#include "OpenGL.h"
#include "lwjgl/GLContext.h"
#include "lwjgl/Display.h"
#include "client/renderer/Tesselator.h"
#include "SDL.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void quad(float z = 0)
{
	Tesselator &t = Tesselator::instance;
	t.begin();
	t.vertexUV(-1, -1, z, 0, 0);
	t.vertexUV(1, -1, z, 1, 0);
	t.vertexUV(1, 1, z, 1, 1);
	t.vertexUV(-1, 1, z, 0, 1);
	t.end();
}

static std::array<unsigned char, 4> pixel(int x, int y)
{
	std::array<unsigned char, 4> p{};
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
	return p;
}

static void expect(int x, int y, int r, int g, int b, const char *message)
{
	const auto p = pixel(x, y);
	if (std::abs(int(p[0]) - r) > 1 || std::abs(int(p[1]) - g) > 1 || std::abs(int(p[2]) - b) > 1)
		throw std::runtime_error(std::string(message) + ": got " + std::to_string(p[0]) + "," + std::to_string(p[1]) +
								 "," + std::to_string(p[2]));
}
int main(int argc, char *argv[])
try
{
	for (int i = 1; i < argc; ++i)
	{
		if (!BetaGL::consumeBackendArgument(i, argc, argv))
			throw std::invalid_argument("Usage: McBetaCppRendererSmoke [--backend NAME]");
	}
	// The stress entry has no prior SDL_Init. Context attributes must survive
	// the renderer's own video-subsystem initialization.
	lwjgl::GLContext::instantiate();
	glViewport(0, 0, 64, 64);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glClearColor(0, 0, 0, 1);
	glClearDepth(1);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5f);
	glColor4f(1, 0, 0, 0.5f);
	quad(-0.5f);
	glColor4f(0, 1, 0, 1);
	quad(0.5f);
	expect(32, 32, 0, 255, 0, "discard must not write color or depth");
	glDisable(GL_ALPHA_TEST);
	glDepthMask(false);
	glColor4f(1, 0, 0, 1);
	quad(-0.75f);
	glDepthMask(true);
	glColor4f(0, 0, 1, 1);
	quad(0);
	expect(32, 32, 0, 0, 255, "depth write mask must not disable depth testing");
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_FLAT);
	Tesselator &t = Tesselator::instance;
	t.begin();
	t.color(255, 0, 0);
	t.vertex(-1, -1, 0);
	t.color(0, 255, 0);
	t.vertex(1, -1, 0);
	t.color(0, 0, 255);
	t.vertex(1, 1, 0);
	t.color(255, 255, 255);
	t.vertex(-1, 1, 0);
	t.end();
	expect(48, 16, 0, 0, 255, "first quad triangle provoking color");
	expect(16, 48, 255, 255, 255, "second quad triangle provoking color");
	glColor4f(1, 1, 1, 1);
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	const unsigned char texels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
	glEnable(GL_TEXTURE_2D);
	quad();
	expect(16, 16, 255, 0, 0, "texture lower-left orientation");
	expect(16, 48, 0, 0, 255, "texture upper-left orientation");
	const unsigned char update[] = {255, 255, 0, 255};
	glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, update);
	quad();
	expect(48, 48, 255, 255, 0, "ordered texture subimage update");
	const unsigned char npot[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255,
								  255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, npot);
	glMatrixMode(GL_TEXTURE);
	glScalef(2, 1, 1);
	glMatrixMode(GL_MODELVIEW);
	quad();
	expect(16, 16, 0, 255, 0, "NPOT nearest sampling");
	expect(48, 16, 0, 255, 0, "NPOT repetition without texture resizing");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	quad();
	expect(32, 16, 139, 0, 116, "NPOT linear repeat seam");
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	quad();
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1, &texture);
	expect(32, 16, 0, 249, 6, "texture deletion must retain submitted pixels");
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glColor4f(1, 0, 0, 1);
	quad();
	expect(32, 32, 255, 0, 0, "CCW exterior must render");
	glPushMatrix();
	glScalef(-1, 1, 1);
	glColor4f(0, 1, 0, 1);
	quad();
	glPopMatrix();
	expect(32, 32, 255, 0, 0, "mirrored back face must cull");
	glDisable(GL_CULL_FACE);
	glColor4f(0, 0, 1, 1);
	quad();
	expect(32, 32, 0, 0, 255, "culling restoration");
	glColorMask(true, false, false, false);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	expect(32, 32, 255, 0, 255, "clear must preserve masked channels");
	glColorMask(true, true, true, true);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	const float ambient[] = {1, 1, 1, 1};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	glColorMaterial(GL_FRONT, GL_AMBIENT);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5f);
	glColor4f(0.5f, 0.5f, 0.5f, 0.2f);
	quad();
	expect(32, 32, 128, 128, 128, "ambient-only color material must preserve diffuse alpha");
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_ALPHA_TEST);
	glClear(GL_COLOR_BUFFER_BIT);
	GLuint list = glGenLists(1);
	glNewList(list, GL_COMPILE);
	t.begin();
	t.vertex(-0.5, -0.5, 0);
	t.vertex(0.5, -0.5, 0);
	t.vertex(0.5, 0.5, 0);
	t.vertex(-0.5, 0.5, 0);
	t.end();
	glTranslatef(0.5f, 0, 0);
	glEndList();
	glColor4f(1, 0, 0, 1);
	glCallList(list);
	glColor4f(0, 1, 0, 1);
	glCallList(list);
	expect(20, 32, 255, 0, 0, "cached geometry must use call-time color");
	expect(50, 32, 0, 255, 0, "glyph advance must persist after cached draw");
	glDeleteLists(list, 1);
	glLoadIdentity();
	list = glGenLists(1);
	glNewList(list, GL_COMPILE);
	t.begin();
	t.color(255, 0, 0, 255);
	t.vertex(-1, -1, 0);
	t.vertex(1, -1, 0);
	t.vertex(1, 1, 0);
	t.vertex(-1, 1, 0);
	t.end();
	t.begin();
	t.color(0, 255, 0, 128);
	t.vertex(-1, -1, 0);
	t.vertex(1, -1, 0);
	t.vertex(1, 1, 0);
	t.vertex(-1, 1, 0);
	t.end();
	glEndList();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glCallList(list);
	expect(32, 32, 127, 128, 0, "cached mesh batching must preserve transparent primitive order");
	glDeleteLists(list, 1);
	glDisable(GL_BLEND);
	glColor4f(1, 1, 1, 1);
	glClearColor(0, 0, 1, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
	glColor4f(1, 0, 0, 0.5f);
	quad();
	expect(32, 32, 128, 0, 255, "TNT blend must see opaque destination alpha");
	if (pixel(32, 32)[3] != 255)
		throw std::runtime_error("RGB target readback must have opaque alpha");
	glDisable(GL_BLEND);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glColor4f(1, 1, 1, 1);
	glLineWidth(2);
	t.begin(GL_LINES);
	t.vertex(-0.5, 0, 0);
	t.vertex(0.5, 0, 0);
	t.end();
	int coverage = 0;
	for (int y = 28; y <= 35; ++y)
		if (pixel(32, y)[0] > 127)
			++coverage;
	if (coverage != 2)
		throw std::runtime_error("selection line is not two framebuffer pixels wide");
	glLineWidth(1);
	glClear(GL_COLOR_BUFFER_BIT);
	const float triangle[] = {-0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0, 0.5f, 0};
	GLuint buffer = 0;
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle, GL_STATIC_DRAW);
	glVertexPointer(3, GL_FLOAT, 12, nullptr);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDeleteBuffers(1, &buffer);
	expect(32, 24, 255, 255, 255, "mesh deletion must defer native resource destruction");
	const float black[] = {0, 0, 0, 1}, white[] = {1, 1, 1, 1}, lightX[] = {1, 0, 0, 0};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, black);
	glLightfv(GL_LIGHT0, GL_AMBIENT, black);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
	glLightfv(GL_LIGHT0, GL_POSITION, lightX);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glClear(GL_COLOR_BUFFER_BIT);
	t.begin();
	t.normal(0, 0, 1);
	t.vertex(-1, -1, 0);
	t.vertex(1, -1, 0);
	t.vertex(1, 1, 0);
	t.vertex(-1, 1, 0);
	t.end();
	const auto normalPixel = pixel(32, 32);
	if (normalPixel[0] || normalPixel[1] || normalPixel[2])
		throw std::runtime_error("zero packed normal component must not add diffuse light");
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	if (lwjgl::GLContext::getCapabilities()["GL_ARB_occlusion_query"])
	{
		GLuint query = 0, available = 0, samples = 1;
		glGenQueries(1, &query);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(true);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		quad(-0.5f);
		glColorMask(false, false, false, false);
		glDepthMask(false);
		glBeginQuery(GL_SAMPLES_PASSED, query);
		quad(0);
		glEndQuery(GL_SAMPLES_PASSED);
		glFinish();
		glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
		if (!available)
			throw std::runtime_error("finished occlusion query is unavailable");
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &samples);
		if (samples != 0)
			throw std::runtime_error("occluded geometry reported visible");
		glBeginQuery(GL_SAMPLES_PASSED, query);
		quad(-0.75f);
		glEndQuery(GL_SAMPLES_PASSED);
		glFinish();
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &samples);
		if (!samples)
			throw std::runtime_error("visible geometry reported occluded");
		glBeginQuery(GL_SAMPLES_PASSED, query);
		quad(-0.75f);
		glEndQuery(GL_SAMPLES_PASSED);
		glDeleteQueries(1, &query);
		glFinish();
		glColorMask(true, true, true, true);
		glDepthMask(true);
		glDisable(GL_DEPTH_TEST);
	}
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	const unsigned char green[] = {0, 255, 0, 255}, red[] = {255, 0, 0, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
	quad();
	expect(32, 32, 0, 255, 0, "texture before minimize");
	SDL_Window *window = lwjgl::GLContext::detail::getWindow();
	SDL_ShowWindow(window);
	SDL_MinimizeWindow(window);
	SDL_PumpEvents();
	lwjgl::Display::swapBuffers();
	if (BetaGL::modern() && BetaGL::hasDrawableTarget())
		throw std::runtime_error("minimized renderer still reports a drawable target");
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, red);
	SDL_RestoreWindow(window);
	SDL_PumpEvents();
	lwjgl::Display::swapBuffers();
	glViewport(0, 0, 64, 64);
	quad();
	expect(32, 32, 255, 0, 0, "texture updates must survive minimize and restore");
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1, &texture);
	const int sizes[][2] = {{640, 360}, {320, 240}, {854, 480}};
	for (const auto &size : sizes)
	{
		SDL_SetWindowSize(window, size[0], size[1]);
		SDL_PumpEvents();
		lwjgl::Display::swapBuffers();
		int width = 0, height = 0;
		BetaGL::drawableSize(&width, &height);
		if (width != size[0] || height != size[1])
			throw std::runtime_error("drawable size did not follow resize");
		glViewport(0, 0, width, height);
		glColor4f(1, 0, 0, 1);
		quad();
		expect(width - 2, height - 2, 255, 0, 0, "resized render target coverage");
	}
	if (glGetError() != GL_NO_ERROR)
		throw std::runtime_error("renderer left an API error");
	std::cout << "renderer pixel regressions passed: " << BetaGL::description() << '\n';
	return 0;
}
catch (const std::exception &error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
