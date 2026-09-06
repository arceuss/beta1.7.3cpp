#define SDL_MAIN_HANDLED
#include "SDL.h"

#include <cstring>

#include "client/Minecraft.h"
#include "java/System.h"
#include "tools/BlockSmoke.h"
#include "tools/MultiplayerScreenSmoke.h"
#include "tools/NetworkSmoke.h"
#include "tools/RegionIoSmoke.h"
#include "tools/SaveConverterSmoke.h"
#include "tools/SoundSmoke.h"
#include "tools/TerrainStorageSmoke.h"
#ifdef B173_PGO_STRESS_EMBED
#include "tools/stress/StressHarness.h"
#endif

#include "external/SDLException.h"

#include "lwjgl/GLContext.h"

int main(int argc, char *argv[])
{
#ifdef B173_PGO_STRESS_EMBED
	if (argc >= 2 && std::strcmp(argv[1], "--stress") == 0)
		return stress::runCommandLine(argc - 1, argv + 1);
#endif
	if (argc >= 2 && std::strcmp(argv[1], "--block-smoke") == 0)
		return runBlockSmoke();
	if (argc >= 2 && std::strcmp(argv[1], "--network-smoke") == 0)
		return runNetworkSmoke();
	if (argc >= 2 && std::strcmp(argv[1], "--region-io-smoke") == 0)
		return runRegionIoSmoke();
	if (argc >= 2 && std::strcmp(argv[1], "--terrain-storage-smoke") == 0)
		return runTerrainStorageSmoke();
	if (argc >= 2 && std::strcmp(argv[1], "--save-converter-smoke") == 0)
		return runSaveConverterSmoke();
	if (argc >= 2 && std::strcmp(argv[1], "--sound-smoke") == 0)
		return runSoundSmoke();
	if (argc >= 2 && std::strcmp(argv[1], "--multiplayer-screen-smoke") == 0)
		return runMultiplayerScreenSmoke();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
		throw SDLException();
	lwjgl::GLContext::instantiate();

	jstring username = u"Player" + String::toString(System::currentTimeMillis() % 1000);
	if (argc >= 2)
		username = String::fromUTF8(argv[1]);

	jstring auth = u"-";
	if (argc >= 3)
		auth = String::fromUTF8(argv[2]);

	if (argc >= 4)
	{
		jstring server = String::fromUTF8(argv[3]);
		Minecraft::startAndConnectTo(&username, &auth, &server);
	}
	else
	{
		Minecraft::start(&username, &auth);
	}

	return 0;
}
