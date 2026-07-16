#include "client/gui/ContainerScreen.h"

#include "client/Minecraft.h"
#include "world/entity/player/Player.h"

ContainerScreen::ContainerScreen(Minecraft &minecraft) : Screen(minecraft)
{
}

bool ContainerScreen::shouldClose(bool alive, bool removed)
{
	return !alive || removed;
}

void ContainerScreen::tick()
{
	Screen::tick();
	if (minecraft.player != nullptr &&
		shouldClose(minecraft.player->isAlive(), minecraft.player->removed))
	{
		minecraft.player->closeContainer();
	}
}
