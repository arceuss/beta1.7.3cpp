#include "client/gui/CreateWorldScreen.h"

#include <stdexcept>

#include "client/Minecraft.h"
#include "client/locale/Language.h"

#include "client/gamemode/SurvivalMode.h"
#include "client/gui/GuiTextField.h"
#include "client/gui/SelectWorldScreen.h"

#include "java/File.h"
#include "java/Random.h"

#include "lwjgl/Keyboard.h"

namespace
{

bool isWhitespace(char_t c)
{
	return c == u' ' || c == u'\n' || c == u'\r' || c == u'\t' || c == u'\f';
}

jstring trim(const jstring &text)
{
	size_t start = 0;
	while (start < text.size() && isWhitespace(text[start]))
		start++;

	size_t end = text.size();
	while (end > start && isWhitespace(text[end - 1]))
		end--;

	return text.substr(start, end - start);
}

bool isReservedFolderChar(char_t c)
{
	switch (c)
	{
		case u'/':
		case u'\n':
		case u'\r':
		case u'\t':
		case 0:
		case u'\f':
		case u'`':
		case u'?':
		case u'*':
		case u'\\':
		case u'<':
		case u'>':
		case u'|':
		case u'"':
		case u':':
			return true;
		default:
			return false;
	}
}

jstring sanitizeFolderName(const jstring &text)
{
	jstring folderName = trim(text);
	for (auto &c : folderName)
	{
		if (isReservedFolderChar(static_cast<char_t>(c)))
			c = u'_';
	}
	if (folderName.empty())
		folderName = u"World";
	return folderName;
}

jstring generateUnusedFolderName(File &workingDirectory, const jstring &baseName)
{
	std::unique_ptr<File> saves(File::open(workingDirectory, u"saves"));
	saves->mkdirs();

	jstring folderName = baseName;
	while (true)
	{
		std::unique_ptr<File> world(File::open(*saves, folderName));
		if (!world->exists())
			return folderName;
		folderName += u"-";
	}
}

long_t hashSeedString(const jstring &text)
{
	int_t hash = 0;
	for (char_t c : text)
		hash = 31 * hash + c;
	return hash;
}

}

CreateWorldScreen::CreateWorldScreen(Minecraft &minecraft, std::shared_ptr<Screen> lastScreen) : Screen(minecraft), lastScreen(lastScreen)
{

}

void CreateWorldScreen::init()
{
	Language &language = Language::getInstance();
	lwjgl::Keyboard::enableRepeatEvents(true);
	buttons.push_back(Util::make_shared<Button>(0, width / 2 - 100, height / 4 + 108, language.getElement(u"selectWorld.create")));
	buttons.push_back(Util::make_shared<Button>(1, width / 2 - 100, height / 4 + 132, language.getElement(u"gui.cancel")));
	worldNameField = Util::make_shared<GuiTextField>(*this, font, width / 2 - 100, 60, 200, 20, language.getElement(u"selectWorld.newWorld"));
	worldNameField->isFocused = true;
	worldNameField->setMaxStringLength(32);
	seedField = Util::make_shared<GuiTextField>(*this, font, width / 2 - 100, 116, 200, 20, u"");
	updateFolderName();
	updateCreateButton();
}

void CreateWorldScreen::tick()
{
	if (worldNameField != nullptr)
		worldNameField->updateCursorCounter();
	if (seedField != nullptr)
		seedField->updateCursorCounter();
}

void CreateWorldScreen::removed()
{
	lwjgl::Keyboard::enableRepeatEvents(false);
}

void CreateWorldScreen::selectNextField()
{
	if (worldNameField == nullptr || seedField == nullptr)
		return;

	if (worldNameField->isFocused)
	{
		worldNameField->setFocused(false);
		seedField->setFocused(true);
	}
	else
	{
		worldNameField->setFocused(true);
		seedField->setFocused(false);
	}
}

void CreateWorldScreen::buttonClicked(Button &button)
{
	if (!button.active)
		return;

	if (button.id == 1)
	{
		minecraft.setScreen(Util::make_shared<SelectWorldScreen>(minecraft, lastScreen));
		return;
	}

	if (button.id == 0)
	{
		if (createClicked)
			return;

		createClicked = true;
		minecraft.setScreen(nullptr);
		minecraft.gameMode = Util::make_shared<SurvivalMode>(minecraft);
		minecraft.selectLevel(folderName, trim(worldNameField->getText()), parseSeed());
		minecraft.setScreen(nullptr);
	}
}

void CreateWorldScreen::keyPressed(char_t eventCharacter, int_t eventKey)
{
	if (worldNameField != nullptr && worldNameField->isFocused)
		worldNameField->textboxKeyTyped(eventCharacter, eventKey);
	else if (seedField != nullptr)
		seedField->textboxKeyTyped(eventCharacter, eventKey);

	if (eventKey == lwjgl::Keyboard::KEY_RETURN && !buttons.empty())
		buttonClicked(*buttons[0]);

	updateFolderName();
	updateCreateButton();
}

void CreateWorldScreen::mouseClicked(int_t x, int_t y, int_t buttonNum)
{
	Screen::mouseClicked(x, y, buttonNum);
	if (worldNameField != nullptr)
		worldNameField->mouseClicked(x, y, buttonNum);
	if (seedField != nullptr)
		seedField->mouseClicked(x, y, buttonNum);
}

void CreateWorldScreen::render(int_t xm, int_t ym, float a)
{
	Language &language = Language::getInstance();
	renderBackground();
	drawCenteredString(font, language.getElement(u"selectWorld.create"), width / 2, height / 4 - 40, 0xFFFFFF);
	drawString(font, language.getElement(u"selectWorld.enterName"), width / 2 - 100, 47, 0xA0A0A0);
	drawString(font, language.getElement(u"selectWorld.resultFolder") + u" " + folderName, width / 2 - 100, 85, 0xA0A0A0);
	drawString(font, language.getElement(u"selectWorld.enterSeed"), width / 2 - 100, 104, 0xA0A0A0);
	drawString(font, language.getElement(u"selectWorld.seedInfo"), width / 2 - 100, 140, 0xA0A0A0);
	if (worldNameField != nullptr)
		worldNameField->drawTextBox();
	if (seedField != nullptr)
		seedField->drawTextBox();
	Screen::render(xm, ym, a);
}

void CreateWorldScreen::updateFolderName()
{
	folderName = generateUnusedFolderName(*minecraft.getWorkingDirectory(), sanitizeFolderName(worldNameField->getText()));
}

void CreateWorldScreen::updateCreateButton()
{
	if (!buttons.empty())
		buttons[0]->active = !trim(worldNameField->getText()).empty();
}

long_t CreateWorldScreen::parseSeed() const
{
	Random random;
	long_t seed = random.nextLong();
	jstring seedText = trim(seedField->getText());
	if (seedText.empty())
		return seed;

	try
	{
		long_t parsed = std::stoll(String::toUTF8(seedText));
		return parsed != 0 ? parsed : seed;
	}
	catch (const std::exception &)
	{
		return hashSeedString(seedText);
	}
}
