#include "lwjgl/GLContext.h"

#include <iostream>
#include <stdexcept>
#include <csignal>
#include <string>
#include <cstring>

#include "external/SDLException.h"
#include "BetaGL.h"

#include "SDL.h"

// #define MC_DEBUG_GL

#ifdef MC_DEBUG_GL
static void GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *msg, const void *data)
{
    const char* _source;
    const char* _type;
    const char* _severity;

    switch (source) {
        case GL_DEBUG_SOURCE_API:
        _source = "API";
        break;

        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        _source = "WINDOW SYSTEM";
        break;

        case GL_DEBUG_SOURCE_SHADER_COMPILER:
        _source = "SHADER COMPILER";
        break;

        case GL_DEBUG_SOURCE_THIRD_PARTY:
        _source = "THIRD PARTY";
        break;

        case GL_DEBUG_SOURCE_APPLICATION:
        _source = "APPLICATION";
        break;

        case GL_DEBUG_SOURCE_OTHER:
        _source = "UNKNOWN";
        break;

        default:
        _source = "UNKNOWN";
        break;
    }

    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        _type = "ERROR";
        break;

        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        _type = "DEPRECATED BEHAVIOR";
        break;

        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        _type = "UDEFINED BEHAVIOR";
        break;

        case GL_DEBUG_TYPE_PORTABILITY:
        _type = "PORTABILITY";
        break;

        case GL_DEBUG_TYPE_PERFORMANCE:
        _type = "PERFORMANCE";
        break;

        case GL_DEBUG_TYPE_OTHER:
        _type = "OTHER";
        break;

        case GL_DEBUG_TYPE_MARKER:
        _type = "MARKER";
        break;

        default:
        _type = "UNKNOWN";
        break;
    }

    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
        _severity = "HIGH";
        break;

        case GL_DEBUG_SEVERITY_MEDIUM:
        _severity = "MEDIUM";
        break;

        case GL_DEBUG_SEVERITY_LOW:
        _severity = "LOW";
        break;

        case GL_DEBUG_SEVERITY_NOTIFICATION:
        _severity = "NOTIFICATION";
        break;

        default:
        _severity = "UNKNOWN";
        break;
    }

    printf("%d: %s of %s severity, raised from %s: %s\n",
            id, _type, _severity, _source, msg);
	std::raise(SIGINT);
}
#endif

namespace lwjgl
{
namespace GLContext
{

// Detail implementation
namespace detail
{

// GL 3.0/3.2 enumerants; the bundled GLAD headers stop at OpenGL 2.1
static const GLenum NUM_EXTENSIONS = 0x821D;
static const GLenum CONTEXT_PROFILE_MASK = 0x9126;
static const GLint CORE_PROFILE_BIT = 0x00000001;

// Entry points GLAD cannot hand out: the loader has no glGetStringi, and the
// OpenGL ES entry points belong to BetaGL.
typedef const GLubyte *(APIENTRY *GetStringProc)(GLenum name);
typedef const GLubyte *(APIENTRY *GetStringiProc)(GLenum name, GLuint index);
typedef void (APIENTRY *GetIntegervProc)(GLenum pname, GLint *data);

static void *loadEntryPoint(const char *name)
{
	void *proc = SDL_GL_GetProcAddress(name);
	if (proc == nullptr)
		throw std::runtime_error(std::string("The driver does not export ") + name);
	return proc;
}

// The API a backend asks the window system for
struct ContextRequest
{
	int major;
	int minor;
	int profile;
	bool modern;
};

static ContextRequest getContextRequest(BetaGL::Backend backend)
{
	switch (backend)
	{
		case BetaGL::Backend::Compatibility:
			// B173 - Fixed-function rendering uses the OpenGL 2.1 compatibility API.
			return ContextRequest{2, 1, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY, false};
		case BetaGL::Backend::Core33:
			return ContextRequest{3, 3, SDL_GL_CONTEXT_PROFILE_CORE, true};
		case BetaGL::Backend::ES20:
			return ContextRequest{2, 0, SDL_GL_CONTEXT_PROFILE_ES, true};
		case BetaGL::Backend::Vulkan:
		case BetaGL::Backend::D3D12:
			throw std::logic_error("A native renderer does not use an OpenGL context");
	}
	throw std::runtime_error("Unknown renderer backend");
}

static const char *getProfileName(int profile)
{
	switch (profile)
	{
		case SDL_GL_CONTEXT_PROFILE_CORE:
			return "core profile";
		case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY:
			return "compatibility profile";
		case SDL_GL_CONTEXT_PROFILE_ES:
			return "ES profile";
		default:
			return "unknown profile";
	}
}

static void setModernFramebufferAttributes()
{
	// The shader pipeline presents into a plain RGB8 + depth24 default
	// framebuffer and writes non-linear colour itself, so sRGB encoding by the
	// window system would double-correct it.
	//
	// Destination alpha is asked for as 0, so the modern backends get the same
	// canonical RGB target the compatibility path gets, where the TNT blend's
	// GL_DST_ALPHA reads back as 1. A swapchain that carries alpha regardless
	// is not a problem: the renderer normalises those semantics itself, as the
	// native backends have to anyway.
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);
}

// GL_VERSION is "<major>.<minor>[.<release>] <vendor>", prefixed with
// "OpenGL ES " (or "OpenGL ES-CM ") on OpenGL ES contexts. Returns whether the
// context speaks OpenGL ES.
static bool parseVersion(const char *version, int &major, int &minor)
{
	const char *version_p = version;

	bool es = std::strncmp(version_p, "OpenGL ES", 9) == 0;
	if (es)
	{
		// Skip the API name and an optional "-CM"/"-CL" profile marker.
		version_p += 9;
		while (*version_p != '\0' && *version_p != ' ')
			version_p++;
		while (*version_p == ' ')
			version_p++;
	}

	major = 0;
	minor = 0;
	while (*version_p >= '0' && *version_p <= '9')
		major = major * 10 + (*version_p++ - '0');
	if (*version_p == '.')
	{
		version_p++;
		while (*version_p >= '0' && *version_p <= '9')
			minor = minor * 10 + (*version_p++ - '0');
	}

	return es;
}

// The requested attributes are a wish, and SDL_GL_GetAttribute hands the
// context version and profile mask straight back out of that wish, so the
// created context is interrogated through GL itself instead.
static void verifyContext(const ContextRequest &request)
{
	GetStringProc getStringProc = reinterpret_cast<GetStringProc>(loadEntryPoint("glGetString"));

	const char *version = reinterpret_cast<const char *>(getStringProc(GL_VERSION));
	if (version == nullptr)
		throw std::runtime_error("The created context reports no GL_VERSION");

	int major = 0, minor = 0;
	bool es = parseVersion(version, major, minor);
	bool wants_es = request.profile == SDL_GL_CONTEXT_PROFILE_ES;

	if (es != wants_es || major < request.major || (major == request.major && minor < request.minor))
	{
		throw std::runtime_error("Requested " + std::string(wants_es ? "OpenGL ES " : "OpenGL ") +
			std::to_string(request.major) + "." + std::to_string(request.minor) + " " + getProfileName(request.profile) +
			", but the driver created \"" + version + "\"; no fallback context is created");
	}

	// A compatibility context also satisfies the version test, so desktop core
	// requests check the profile the context actually reports.
	if (request.profile == SDL_GL_CONTEXT_PROFILE_CORE)
	{
		GetIntegervProc getIntegervProc = reinterpret_cast<GetIntegervProc>(loadEntryPoint("glGetIntegerv"));

		GLint profile_mask = 0;
		getIntegervProc(CONTEXT_PROFILE_MASK, &profile_mask);
		if ((profile_mask & CORE_PROFILE_BIT) == 0)
			throw std::runtime_error("Requested an OpenGL core profile, but the driver created \"" + std::string(version) +
				"\" with profile mask " + std::to_string(profile_mask) + "; no fallback context is created");
	}

	// SDL queries the pixel format of the context it created, so this is the
	// framebuffer the game will actually present into.
	int red = 0, green = 0, blue = 0, alpha = 0, depth = 0;
	if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &red) ||
		SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &green) ||
		SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &blue) ||
		SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &alpha) ||
		SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth))
		throw SDLException();

	// Destination alpha is asked for as 0 to match the compatibility format,
	// but a swapchain that carries alpha anyway is fine: the renderer
	// normalises destination alpha semantics above this layer. Depth is not
	// negotiable the same way - OpenGL ES baselines are allowed to hand out a
	// 16 bit depth buffer, desktop core is held to the 24 bits it asked for.
	int depth_minimum = wants_es ? 16 : 24;
	if (red < 8 || green < 8 || blue < 8 || depth < depth_minimum)
	{
		throw std::runtime_error("Requested an RGB8 depth" + std::to_string(depth_minimum) +
			" framebuffer, but the driver created R" + std::to_string(red) + "G" + std::to_string(green) +
			"B" + std::to_string(blue) + "A" + std::to_string(alpha) + " depth" + std::to_string(depth) +
			"; no fallback framebuffer is created");
	}

	// The depth precision the game actually got is reported rather than
	// assumed, so a 16 bit OpenGL ES buffer never passes for 24 bits.
	std::cout << "B173 renderer context: " << version << ", R" << red << " G" << green << " B" << blue
		<< " A" << alpha << " depth" << depth << '\n';
}

// Extension strings are space separated with no trailing separator, so the last
// token has to be flushed on the terminator too.
static void addExtensionList(GLCapabilities &capabilities, const char *extensions)
{
	if (extensions == nullptr)
		return;

	std::string cap;
	for (const char *extension_p = extensions;; extension_p++)
	{
		if (*extension_p != '\0' && *extension_p != ' ' && *extension_p != '\t' && *extension_p != '\n' && *extension_p != '\r')
		{
			cap.push_back(*extension_p);
			continue;
		}

		if (!cap.empty())
		{
			capabilities.add(cap);
			cap.clear();
		}

		if (*extension_p == '\0')
			break;
	}
}

// OpenGL ES entry points are not in GLAD, so ask the driver directly.
static void addLoadedExtensionList(GLCapabilities &capabilities)
{
	GetStringProc getStringProc = reinterpret_cast<GetStringProc>(loadEntryPoint("glGetString"));
	addExtensionList(capabilities, reinterpret_cast<const char *>(getStringProc(GL_EXTENSIONS)));
}

// Core profiles dropped the monolithic GL_EXTENSIONS string.
static void addIndexedExtensions(GLCapabilities &capabilities)
{
	GetIntegervProc getIntegervProc = reinterpret_cast<GetIntegervProc>(loadEntryPoint("glGetIntegerv"));
	GetStringiProc getStringiProc = reinterpret_cast<GetStringiProc>(loadEntryPoint("glGetStringi"));

	GLint count = 0;
	getIntegervProc(NUM_EXTENSIONS, &count);
	for (GLint i = 0; i < count; i++)
	{
		const GLubyte *extension = getStringiProc(GL_EXTENSIONS, static_cast<GLuint>(i));
		if (extension != nullptr)
			capabilities.add(reinterpret_cast<const char *>(extension));
	}
}

// Modern capability policy - the shader pipeline implements some fixed-function
// semantics itself, and the game asks for them by extension name.
static void addModernCapabilities(BetaGL::Backend backend, GLCapabilities &capabilities)
{
	// GameRenderer selects eye radial fog through NV_fog_distance; the shaders
	// always evaluate fog radially, on desktop GL and on OpenGL ES alike.
	capabilities.add("GL_NV_fog_distance");

	// OpenGL 3.3 has occlusion queries in core, so the ARB spelling the game
	// queries stays honest; OpenGL ES 2.0 has no query objects at all.
	if (backend == BetaGL::Backend::Core33)
		capabilities.add("GL_ARB_occlusion_query");
}

// Context singleton
class GLContext
{
private:
	BetaGL::Backend backend;
	SDL_Window *window = nullptr;
	SDL_GLContext gl_context = nullptr;
	GLCapabilities capabilities;
	bool ownsVideo = false;

public:
	// The backend is chosen before any API object exists, as the window and
	// context attributes differ per backend.
	GLContext() : backend(BetaGL::backend())
	{
		try
		{
		// SDL video initialization resets GL attributes. The headless game
		// runner enters here without SDL_Init, so initialize before setting them.
		if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0)
		{
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) throw SDLException();
			ownsVideo = true;
		}
		if (backend == BetaGL::Backend::Vulkan || backend == BetaGL::Backend::D3D12)
		{
			Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
			if (backend == BetaGL::Backend::Vulkan) flags |= SDL_WINDOW_VULKAN;
			window = SDL_CreateWindow("McBetaCpp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 854, 480, flags);
			if (!window) throw SDLException();
			BetaGL::initialize(window);
			capabilities.add("GL_NV_fog_distance");
			if (BetaGL::supportsOcclusion()) capabilities.add("GL_ARB_occlusion_query");
			return;
		}
		const ContextRequest request = getContextRequest(backend);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, request.major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, request.minor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, request.profile);

		if (request.modern)
			setModernFramebufferAttributes();

#ifdef MC_DEBUG_GL
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

		// Create SDL window
		window = SDL_CreateWindow("McBetaCpp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 854, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
		if (window == nullptr)
			throw SDLException();

		// Create OpenGL context
		gl_context = SDL_GL_CreateContext(window);
		if (gl_context == nullptr)
			throw SDLException();

		if (SDL_GL_MakeCurrent(window, gl_context))
			throw SDLException();

		// A driver may hand back something older than what was asked for; the
		// modern backends need the real API, never a silent downgrade.
		if (request.modern)
			verifyContext(request);

		// The bundled GLAD targets 2.1 and queries GL_EXTENSIONS, which Core
		// removed. Both shader backends load their own entry points.
		bool use_glad = backend == BetaGL::Backend::Compatibility;
		if (use_glad)
		{
			if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
				throw std::runtime_error("Failed to load glad");
		}

		// Disable VSync
		SDL_GL_SetSwapInterval(0);

		// Parse capabilities
		if (backend == BetaGL::Backend::Core33)
			addIndexedExtensions(capabilities);
		else if (use_glad)
			addExtensionList(capabilities, reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));
		else
			addLoadedExtensionList(capabilities);

		if (request.modern)
			addModernCapabilities(backend, capabilities);

		// Bring the portable renderer up on the current context
		BetaGL::initialize(window);

#ifdef MC_DEBUG_GL
		// Enable debugging
		if (use_glad)
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(GLDebugMessageCallback, nullptr);
		}
#endif
		}
		catch (...)
		{
			BetaGL::shutdown();
			if (gl_context) SDL_GL_DeleteContext(gl_context);
			if (window) SDL_DestroyWindow(window);
			if (ownsVideo) SDL_QuitSubSystem(SDL_INIT_VIDEO);
			throw;
		}
	}

	GLContext(const GLContext &) = delete;
	GLContext &operator=(const GLContext &) = delete;

	~GLContext()
	{
		// The renderer owns GL objects, so it has to release them while its
		// context is still current.
		BetaGL::shutdown();

		if (gl_context != nullptr)
			SDL_GL_DeleteContext(gl_context);
		if (window != nullptr)
			SDL_DestroyWindow(window);
		if (ownsVideo)
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	SDL_Window *getWindow() const { return window; }
	SDL_GLContext getGLContext() const { return gl_context; }
	const GLCapabilities &getCapabilities() const { return capabilities; }
};

// Context singletons
static GLContext &getContext()
{
	static GLContext context;
	return context;
}

SDL_Window *getWindow()
{
	return getContext().getWindow();
}
SDL_GLContext getGLContext()
{
	return getContext().getGLContext();
}

}

// GL capabilities
void instantiate()
{
	SDL_GLContext context = detail::getContext().getGLContext();
	if (context && SDL_GL_MakeCurrent(detail::getContext().getWindow(), context))
		throw SDLException();
}

const detail::GLCapabilities &getCapabilities()
{
	return detail::getContext().getCapabilities();
}

}
}
