#include "client/gui/OptionsScreen.h"

#include "client/Minecraft.h"
#include "client/Options.h"
#include "client/locale/Language.h"

#include "client/gui/ControlsScreen.h"
#include "client/gui/VideoSettingsScreen.h"

#include "client/gui/SmallButton.h"
#include "client/gui/SlideButton.h"

namespace
{

Options::Option::Element *const OPTION_SCREEN_OPTIONS[] = {
	&Options::Option::MUSIC,
	&Options::Option::SOUND,
	&Options::Option::INVERT_MOUSE,
	&Options::Option::SENSITIVITY,
	&Options::Option::DIFFICULTY,
};

void addOptionButton(std::vector<std::shared_ptr<Button>> &buttons, int_t x, int_t y, int_t id, Options::Option::Element &option, Options &options)
{
	if (option.isProgress)
		buttons.push_back(Util::make_shared<SlideButton>(id, x, y, &option, options.getMessage(option), options.getProgressValue(option)));
	else
		buttons.push_back(Util::make_shared<SmallButton>(id, x, y, &option, options.getMessage(option)));
}

}

OptionsScreen::OptionsScreen(Minecraft &minecraft, std::shared_ptr<Screen> lastScreen, Options &options) : Screen(minecraft), lastScreen(lastScreen), options(options)
{

}

void OptionsScreen::init()
{
	Language &language = Language::getInstance();
	title = language.getElement(u"options.title");

	for (int_t i = 0; i < Util::size(OPTION_SCREEN_OPTIONS); i++)
	{
		Options::Option::Element &option = *OPTION_SCREEN_OPTIONS[i];
		addOptionButton(buttons, width / 2 - 155 + i % 2 * 160, height / 6 + 24 * (i >> 1), i, option, options);
	}

	buttons.push_back(Util::make_shared<Button>(101, width / 2 - 100, height / 6 + 108, language.getElement(u"options.video")));
	buttons.push_back(Util::make_shared<Button>(100, width / 2 - 100, height / 6 + 132, language.getElement(u"options.controls")));
	buttons.push_back(Util::make_shared<Button>(200, width / 2 - 100, height / 6 + 168, language.getElement(u"gui.done")));
}

void OptionsScreen::buttonClicked(Button &button)
{
	if (!button.active)
		return;

	if (button.id < Util::size(OPTION_SCREEN_OPTIONS) && button.isSmallButton())
	{
		auto &smallButton = reinterpret_cast<SmallButton &>(button);
		options.toggle(*smallButton.getOption(), 1);
		smallButton.msg = options.getMessage(*smallButton.getOption());
		return;
	}

	if (button.id == 101)
	{
		minecraft.options.save();
		minecraft.setScreen(Util::make_shared<VideoSettingsScreen>(minecraft, minecraft.screen, options));
	}
	else if (button.id == 100)
	{
		minecraft.options.save();
		minecraft.setScreen(Util::make_shared<ControlsScreen>(minecraft, minecraft.screen, options));
	}
	else if (button.id == 200)
	{
		minecraft.options.save();
		minecraft.setScreen(lastScreen);
	}
}

void OptionsScreen::render(int_t xm, int_t ym, float a)
{
	renderBackground();
	drawCenteredString(font, title, width / 2, 20, 0xFFFFFF);
	Screen::render(xm, ym, a);
}
