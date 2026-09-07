#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "client/gui/Screen.h"
#include "java/Random.h"

class AlphaPlaceTitleScreen : public Screen
{
private:
	struct LogoEffect
	{
		double height = 0.0;
		double previousHeight = 0.0;
		double velocity = 0.0;

		void initialize(int_t x, int_t y);
		void tick();
	};

	enum ButtonId
	{
		BUTTON_OPTIONS = 0,
		BUTTON_SINGLEPLAYER = 1,
		BUTTON_MULTIPLAYER = 2,
		BUTTON_TEXTURE_PACKS = 3,
		BUTTON_ALPHAPLACE = 4,
		BUTTON_QUIT = 5,
	};

	static Random random;

	static const std::array<std::string, 5> LOGO_ROWS;

	static constexpr std::size_t LOGO_WIDTH = 39;
	static constexpr std::size_t LOGO_HEIGHT = 5;

	std::array<LogoEffect, LOGO_WIDTH * LOGO_HEIGHT> logoEffects{};
	bool logoEffectsInitialized = false;
	jstring splash = u"I love AlphaPlace!";

	LogoEffect &logoEffect(std::size_t x, std::size_t y);
	void initializeLogoEffects();
	void renderLogo(float partialTick);

public:
	explicit AlphaPlaceTitleScreen(Minecraft &minecraft);

	void init() override;
	void tick() override;

protected:
	void keyPressed(char_t eventCharacter, int_t eventKey) override;
	void buttonClicked(Button &button) override;

public:
	void render(int_t xm, int_t ym, float partialTick) override;
};
