#pragma once

#include "client/gui/Screen.h"

class ContainerScreen : public Screen
{
protected:
	explicit ContainerScreen(Minecraft &minecraft);
	static bool shouldClose(bool alive, bool removed);

public:
	void tick() override;
};
