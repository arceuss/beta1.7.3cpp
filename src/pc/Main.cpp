#define SDL_MAIN_HANDLED
#include "SDL.h"

#include <cstring>

#include "client/Minecraft.h"
#include "tools/BlockSmoke.h"

#include "external/SDLException.h"

#include "lwjgl/GLContext.h"

int main(int argc, char *argv[])
{
	if (argc >= 2 && std::strcmp(argv[1], "--block-smoke") == 0)
		return runBlockSmoke();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
		throw SDLException();
	lwjgl::GLContext::instantiate();

	jstring username = u"arceus413";

	jstring auth = u"-";
	if (argc >= 3 && std::strlen(argv[2]) > 1)
		auth = String::fromUTF8(argv[2]);

	Minecraft::start(&username, &auth);

	return 0;
}
