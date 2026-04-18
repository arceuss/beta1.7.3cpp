#pragma once

#include "client/gui/Screen.h"

#include "java/Random.h"

class TitleScreen : public Screen
{
private:
	static Random random;
	jstring splash = u"missingno";

public:
	TitleScreen(Minecraft &minecraft);

	void init() override;

protected:
	void buttonClicked(Button &button) override;

public:
	void render(int_t xm, int_t ym, float a) override;
};
