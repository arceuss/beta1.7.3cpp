#include "client/renderer/backend/BackendHost.h"
#include "client/renderer/backend/gl/GLDevice.h"
#include "SDL.h"
#include <glad/glad.h>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

namespace b173
{
namespace render
{
static GLProc loadGL(const char *name)
{
	return reinterpret_cast<GLProc>(SDL_GL_GetProcAddress(name));
}

class GLHost final : public BackendHost
{
	SDL_Window *window;
	GLDevice renderer;
	bool es;
	float maxWidth = 1;
	PFNGLFINISHPROC finishGL;
	PFNGLGETERRORPROC getErrorGL;
	PFNGLGENQUERIESPROC genQueriesGL = nullptr;
	PFNGLDELETEQUERIESPROC deleteQueriesGL = nullptr;
	PFNGLBEGINQUERYPROC beginQueryGL = nullptr;
	PFNGLENDQUERYPROC endQueryGL = nullptr;
	PFNGLGETQUERYOBJECTUIVPROC queryResultGL = nullptr;

  public:
	GLHost(SDL_Window *window, bool es, const GLCreateInfo &info) : window(window), renderer(info), es(es)
	{
		float minimumWidth = 1;
		renderer.lineWidthRange(&minimumWidth, &maxWidth);
		finishGL = reinterpret_cast<PFNGLFINISHPROC>(SDL_GL_GetProcAddress("glFinish"));
		getErrorGL = reinterpret_cast<PFNGLGETERRORPROC>(SDL_GL_GetProcAddress("glGetError"));
		if (!finishGL || !getErrorGL)
			throw std::runtime_error("Missing required GL synchronization entry point");
		if (!es)
		{
			genQueriesGL = reinterpret_cast<PFNGLGENQUERIESPROC>(SDL_GL_GetProcAddress("glGenQueries"));
			deleteQueriesGL = reinterpret_cast<PFNGLDELETEQUERIESPROC>(SDL_GL_GetProcAddress("glDeleteQueries"));
			beginQueryGL = reinterpret_cast<PFNGLBEGINQUERYPROC>(SDL_GL_GetProcAddress("glBeginQuery"));
			endQueryGL = reinterpret_cast<PFNGLENDQUERYPROC>(SDL_GL_GetProcAddress("glEndQuery"));
			queryResultGL = reinterpret_cast<PFNGLGETQUERYOBJECTUIVPROC>(SDL_GL_GetProcAddress("glGetQueryObjectuiv"));
			if (!genQueriesGL || !deleteQueriesGL || !beginQueryGL || !endQueryGL || !queryResultGL)
				throw std::runtime_error("Missing required Core occlusion query entry point");
		}
	}

	Device &device() override
	{
		return renderer;
	}

	bool beginFrame() override
	{
		int width = 0, height = 0;
		drawableSize(&width, &height);
		if (width <= 0 || height <= 0)
			return false;
		renderer.beginFrame(0, width, height);
		return true;
	}

	void present() override
	{
		const GLenum error = getErrorGL();
		if (error)
			throw std::runtime_error("OpenGL frame error " + std::to_string(error));
		SDL_GL_SwapWindow(window);
	}

	void finish() override
	{
		finishGL();
	}

	void clear(Vec4 color, float depth, bool colorBit, bool depthBit, const PipelineState &state) override
	{
		renderer.clear(color, depth, colorBit, depthBit, state);
	}

	std::vector<std::uint8_t> readPixels(Rect region) override
	{
		return renderer.readPixels(region);
	}

	void drawableSize(int *width, int *height) const override
	{
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
		{
			*width = *height = 0;
			return;
		}
		SDL_GL_GetDrawableSize(window, width, height);
	}

	std::string description() const override
	{
		return renderer.description();
	}

	float maximumLineWidth() const override
	{
		return maxWidth;
	}

	bool supportsOcclusion() const override
	{
		return !es;
	}

	void genQueries(int count, std::uint32_t *ids) override
	{
		if (es)
			throw std::logic_error("ES2 baseline has no occlusion queries");
		genQueriesGL(count, ids);
	}

	void deleteQueries(int count, const std::uint32_t *ids) override
	{
		if (es)
			throw std::logic_error("ES2 baseline has no occlusion queries");
		deleteQueriesGL(count, ids);
	}

	void beginQuery(std::uint32_t id) override
	{
		if (es)
			throw std::logic_error("ES2 baseline has no occlusion queries");
		beginQueryGL(GL_SAMPLES_PASSED, id);
	}

	void endQuery() override
	{
		if (es)
			throw std::logic_error("ES2 baseline has no occlusion queries");
		endQueryGL(GL_SAMPLES_PASSED);
	}

	std::uint32_t queryResult(std::uint32_t id, bool availability) override
	{
		if (es)
			throw std::logic_error("ES2 baseline has no occlusion queries");
		GLuint value = 0;
		queryResultGL(id, availability ? GL_QUERY_RESULT_AVAILABLE : GL_QUERY_RESULT, &value);
		return value;
	}

	unsigned error() override
	{
		return getErrorGL();
	}
};

std::unique_ptr<BackendHost> createGLHost(SDL_Window *window, bool es)
{
	GLCreateInfo info;
	info.profile = es ? GLProfile::ES20 : GLProfile::Core33;
	info.getProc = loadGL;
	const char *validation = std::getenv("B173_RENDER_VALIDATION");
	info.validateDraws = validation && std::strcmp(validation, "1") == 0;
	return std::unique_ptr<BackendHost>(new GLHost(window, es, info));
}
} // namespace render
} // namespace b173
