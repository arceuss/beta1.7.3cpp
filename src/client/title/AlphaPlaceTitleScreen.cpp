#include "client/title/AlphaPlaceTitleScreen.h"

#include "client/Minecraft.h"
#include "client/gui/ConnectingScreen.h"
#include "client/gui/MultiplayerScreen.h"
#include "client/gui/OptionsScreen.h"
#include "client/gui/ScreenSizeCalculator.h"
#include "client/gui/SelectWorldScreen.h"
#include "client/renderer/TileRenderer.h"
#include "client/skins/TexturePackSelectScreen.h"

#include "java/System.h"

#include "util/GLU.h"
#include "util/Mth.h"

#include "world/level/tile/Tile.h"
// Tile.h only forward-declares StoneTile, so Tile::rock needs the definition
// before it can convert to Tile & for renderCube.
#include "world/level/tile/StoneTile.h"

#include "OpenGL.h"

Random AlphaPlaceTitleScreen::random;

const std::array<std::string, 5> AlphaPlaceTitleScreen::LOGO_ROWS = {{
	" *  *   **  * *  *  **  *    *   ** ***",
	"* * *   * * * * * * * * *   * * *   *",
	"*** *   **  *** *** **  *   *** *   **",
	"* * *   *   * * * * *   *   * * *   *",
	"* * *** *   * * * * *   *** * *  ** ***",
}};

constexpr std::size_t AlphaPlaceTitleScreen::LOGO_WIDTH;
constexpr std::size_t AlphaPlaceTitleScreen::LOGO_HEIGHT;

void AlphaPlaceTitleScreen::LogoEffect::initialize(int_t x, int_t y)
{
	height = previousHeight = 10.0 + static_cast<double>(y) + AlphaPlaceTitleScreen::random.nextDouble() * 32.0 + static_cast<double>(x);
	velocity = 0.0;
}

void AlphaPlaceTitleScreen::LogoEffect::tick()
{
	previousHeight = height;
	if (height > 0.0)
		velocity -= 0.6;

	height += velocity;
	velocity *= 0.9;
	if (height < 0.0)
	{
		height = 0.0;
		velocity = 0.0;
	}
}

AlphaPlaceTitleScreen::AlphaPlaceTitleScreen(Minecraft &minecraft) : Screen(minecraft)
{
	// APClient V2.02 chooses one of these two strings once when the menu is created.
	if (random.nextBoolean())
		splash = u"I love Cave Story!";
}

AlphaPlaceTitleScreen::LogoEffect &AlphaPlaceTitleScreen::logoEffect(std::size_t x, std::size_t y)
{
	return logoEffects[x * LOGO_HEIGHT + y];
}

void AlphaPlaceTitleScreen::initializeLogoEffects()
{
	if (logoEffectsInitialized)
		return;

	// Preserve the Java allocation/initialization order: X first, then Y.
	// This matters because each cell consumes one Random.nextDouble().
	for (std::size_t x = 0; x < LOGO_WIDTH; ++x)
	{
		for (std::size_t y = 0; y < LOGO_HEIGHT; ++y)
			logoEffect(x, y).initialize(static_cast<int_t>(x), static_cast<int_t>(y));
	}

	logoEffectsInitialized = true;
}

void AlphaPlaceTitleScreen::tick()
{
	if (!logoEffectsInitialized)
		return;

	for (LogoEffect &effect : logoEffects)
		effect.tick();
}

void AlphaPlaceTitleScreen::keyPressed(char_t, int_t)
{
	// GuiMainMenu.keyTyped is intentionally empty: Escape does nothing here.
}

void AlphaPlaceTitleScreen::init()
{
	const int_t x = width / 2 - 100;
	const int_t y = height / 4;

	buttons.push_back(Util::make_shared<Button>(BUTTON_SINGLEPLAYER, x, y + 48, u"Singleplayer"));
	buttons.push_back(Util::make_shared<Button>(BUTTON_ALPHAPLACE, x, y + 72, u"Connect to AlphaPlace!"));
	buttons.push_back(Util::make_shared<Button>(BUTTON_TEXTURE_PACKS, x, y + 96, u"Mods and Texture Packs"));
	buttons.push_back(Util::make_shared<Button>(BUTTON_MULTIPLAYER, x, y + 120, u"Multiplayer"));
	buttons.push_back(Util::make_shared<Button>(BUTTON_OPTIONS, x, y + 144, 98, 20, u"Options..."));
	buttons.push_back(Util::make_shared<Button>(BUTTON_QUIT, width / 2 + 2, y + 144, 98, 20, u"Quit Game"));
}

void AlphaPlaceTitleScreen::buttonClicked(Button &button)
{
	if (!button.active)
		return;

	switch (button.id)
	{
	case BUTTON_OPTIONS:
		minecraft.setScreen(Util::make_shared<OptionsScreen>(minecraft, minecraft.screen, minecraft.options));
		break;

	case BUTTON_SINGLEPLAYER:
		minecraft.setScreen(Util::make_shared<SelectWorldScreen>(minecraft, minecraft.screen));
		break;

	case BUTTON_MULTIPLAYER:
		minecraft.setScreen(Util::make_shared<MultiplayerScreen>(minecraft, minecraft.screen));
		break;

	case BUTTON_TEXTURE_PACKS:
		minecraft.setScreen(Util::make_shared<TexturePackSelectScreen>(minecraft, minecraft.screen));
		break;

	case BUTTON_ALPHAPLACE:
		minecraft.setScreen(Util::make_shared<ConnectingScreen>(minecraft, "alphaplace.net", 25565));
		break;

	case BUTTON_QUIT:
		minecraft.stop();
		break;
	}
}

void AlphaPlaceTitleScreen::render(int_t xm, int_t ym, float partialTick)
{
	renderBackground();
	glEnable(GL_DEPTH_TEST);
	renderLogo(partialTick);

	// The bytecode also binds /gui/logo.png here, but never draws from it.
	// Keeping only the state reset preserves the visible behavior without a dead texture load.
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glPushMatrix();
	glTranslatef(static_cast<float>(width / 2 + 90), 70.0f, 0.0f);
	glRotatef(-20.0f, 0.0f, 0.0f, 1.0f);
	float scale = 1.8f - Mth::abs(Mth::sin(static_cast<float>(System::currentTimeMillis() % 1000L) / 1000.0f * Mth::PI * 2.0f) * 0.1f);
	scale = scale * 100.0f / static_cast<float>(font.width(splash) + 32);
	glScalef(scale, scale, scale);
	drawCenteredString(font, splash, 0, -8, 0xFFFF00);
	glPopMatrix();

	drawString(font, Minecraft::VERSION_STRING, 2, 2, 0x505050);

	const jstring copyright = u"Copyright Mojang Specifications. Do not distribute.";
	drawString(font, copyright, width - font.width(copyright) - 2, height - 10, 0xFFFFFF);

	const jstring clientVersion = u"v2.02 by Strultz";
	drawString(font, clientVersion, width - font.width(clientVersion) - 2, height - 20, 0xF7342B);

	Screen::render(xm, ym, partialTick);
}

void AlphaPlaceTitleScreen::renderLogo(float partialTick)
{
	initializeLogoEffects();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	ScreenSizeCalculator resolution(minecraft.options, minecraft.width, minecraft.height);
	const int_t logoViewportHeight = 120 * resolution.scale;
	gluPerspective(70.0f,
		static_cast<float>(minecraft.width) / static_cast<float>(logoViewportHeight),
		0.05f,
		100.0f);
	glViewport(0, minecraft.height - logoViewportHeight, minecraft.width, logoViewportHeight);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDepthMask(true);

	TileRenderer tileRenderer;

	for (int_t pass = 0; pass < 3; ++pass)
	{
		glPushMatrix();
		glTranslatef(0.4f, 0.6f, -13.0f);

		if (pass == 0)
		{
			glClear(GL_DEPTH_BUFFER_BIT);
			glTranslatef(0.0f, -0.4f, 0.0f);
			glScalef(0.98f, 1.0f, 1.0f);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else if (pass == 1)
		{
			glDisable(GL_BLEND);
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_COLOR, GL_ONE);
		}

		glScalef(1.0f, -1.0f, 1.0f);
		glRotatef(15.0f, 1.0f, 0.0f, 0.0f);
		glScalef(0.89f, 1.0f, 0.4f);
		glTranslatef(
			-static_cast<float>(LOGO_WIDTH) * 0.5f,
			-static_cast<float>(LOGO_HEIGHT) * 0.5f,
			0.0f);

		const jstring texture = (pass == 0) ? u"/title/black.png" : u"/terrain.png";
		glBindTexture(GL_TEXTURE_2D, minecraft.textures.loadTexture(texture));

		for (std::size_t y = 0; y < LOGO_HEIGHT; ++y)
		{
			const std::string &row = LOGO_ROWS[y];
			for (std::size_t x = 0; x < row.size(); ++x)
			{
				if (row[x] == ' ')
					continue;

				glPushMatrix();

				LogoEffect &effect = logoEffect(x, y);
				float z = static_cast<float>(effect.previousHeight + (effect.height - effect.previousHeight) * static_cast<double>(partialTick));
				float blockScale = 1.0f;
				float alpha = 1.0f;

				if (pass == 0)
				{
					blockScale = z * 0.04f + 1.0f;
					alpha = 1.0f / blockScale;
					z = 0.0f;
				}

				glTranslatef(static_cast<float>(x), static_cast<float>(y), z);
				glScalef(blockScale, blockScale, blockScale);
				tileRenderer.renderCube(Tile::rock, alpha);

				glPopMatrix();
			}
		}

		glPopMatrix();
	}

	glDisable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glViewport(0, 0, minecraft.width, minecraft.height);
	glEnable(GL_CULL_FACE);
}
