#include "client/gui/GuiTextField.h"

#include "client/gui/Screen.h"

#include "lwjgl/Keyboard.h"

namespace
{

bool isPasteShortcutDown()
{
	return lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_LCONTROL)
		|| lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_RCONTROL)
		|| lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_LMETA)
		|| lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_RMETA);
}

}

GuiTextField::GuiTextField(Screen &parentScreen, Font &font, int_t x, int_t y, int_t width, int_t height, const jstring &text)
	: parentScreen(parentScreen), font(font), xPos(x), yPos(y), width(width), height(height)
{
	setText(text);
}

void GuiTextField::setText(const jstring &text)
{
	this->text = Font::sanitize(text);
	if (maxStringLength > 0 && this->text.size() > static_cast<size_t>(maxStringLength))
		this->text.resize(maxStringLength);
}

const jstring &GuiTextField::getText() const
{
	return text;
}

void GuiTextField::updateCursorCounter()
{
	cursorCounter++;
}

void GuiTextField::textboxKeyTyped(char_t eventCharacter, int_t eventKey)
{
	if (!isEnabled || !isFocused)
		return;

	if (eventKey == lwjgl::Keyboard::KEY_TAB)
	{
		parentScreen.selectNextField();
		return;
	}

	if (eventKey == lwjgl::Keyboard::KEY_V && isPasteShortcutDown())
	{
		appendAllowedText(parentScreen.getClipboard());
		return;
	}

	if (eventKey == lwjgl::Keyboard::KEY_BACK)
	{
		if (!text.empty())
			text.pop_back();
		return;
	}

	if (eventCharacter == 0)
		return;

	jstring typedText;
	typedText.push_back(eventCharacter);
	appendAllowedText(typedText);
}

void GuiTextField::mouseClicked(int_t x, int_t y, int_t buttonNum)
{
	bool focused = isEnabled && x >= xPos && x < xPos + width && y >= yPos && y < yPos + height;
	setFocused(focused);
}

void GuiTextField::setFocused(bool focused)
{
	if (focused && !isFocused)
		cursorCounter = 0;
	isFocused = focused;
}

void GuiTextField::drawTextBox()
{
	fill(xPos - 1, yPos - 1, xPos + width + 1, yPos + height + 1, 0xFFA0A0A0);
	fill(xPos, yPos, xPos + width, yPos + height, 0xFF000000);

	if (isEnabled)
	{
		bool showCursor = isFocused && (cursorCounter / 6 % 2 == 0);
		jstring displayText = text;
		if (showCursor)
			displayText += u"_";
		drawString(font, displayText, xPos + 4, yPos + (height - 8) / 2, 0xE0E0E0);
	}
	else
	{
		drawString(font, text, xPos + 4, yPos + (height - 8) / 2, 0x707070);
	}
}

void GuiTextField::setMaxStringLength(int_t maxStringLength)
{
	this->maxStringLength = maxStringLength;
	if (this->maxStringLength > 0 && text.size() > static_cast<size_t>(this->maxStringLength))
		text.resize(this->maxStringLength);
}

void GuiTextField::appendAllowedText(const jstring &text)
{
	jstring filtered = Font::sanitize(text);
	if (filtered.empty())
		return;

	if (maxStringLength > 0)
	{
		if (this->text.size() >= static_cast<size_t>(maxStringLength))
			return;

		size_t remaining = static_cast<size_t>(maxStringLength) - this->text.size();
		if (filtered.size() > remaining)
			filtered.resize(remaining);
	}

	this->text += filtered;
}
