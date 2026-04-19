#include "client/gui/ChatLine.h"

#include "client/gui/Font.h"

namespace
{
	// Extract the last color/format code from a string (e.g. "\u00a7c" for red)
	jstring getFormatFromString(const jstring &str)
	{
		jstring result = u"";
		int_t lastPos = -1;
		int_t len = str.length();

		while (true)
		{
			int_t pos = str.find(167, lastPos + 1);
			if (pos == jstring::npos)
				return result;

			if (pos < len - 1)
			{
				char_t codeChar = str[pos + 1];
				if ((codeChar >= u'0' && codeChar <= u'9') ||
				    (codeChar >= u'a' && codeChar <= u'f') ||
				    (codeChar >= u'A' && codeChar <= u'F'))
				{
					result = jstring(1, 167) + jstring(1, codeChar);
				}
			}

			lastPos = pos;
		}
	}

	// Find the character position where the string exceeds maxWidth pixels
	int_t sizeStringToWidth(Font &font, const jstring &text, int_t maxWidth)
	{
		int_t len = text.length();
		int_t width = 0;
		int_t lastSpace = -1;
		bool bold = false;

		int_t i = 0;
		for (i = 0; i < len; i++)
		{
			char_t ch = text[i];

			if (ch == 167) // § color code
			{
				if (i < len - 1)
				{
					i++;
					char_t codeChar = text[i];
					if (codeChar == u'l' || codeChar == u'L')
						bold = true;
					else if (codeChar == u'r' || codeChar == u'R')
						bold = false;
				}
				continue;
			}

			if (ch == u'\n')
			{
				i++;
				lastSpace = i;
				break;
			}

			if (ch == u' ')
				lastSpace = i;

			int_t charWidth = font.width(jstring(1, ch));
			width += charWidth;
			if (bold)
				width++;

			if (width > maxWidth)
				break;
		}

		int_t finalPos = i;
		if (finalPos != len && lastSpace != -1 && lastSpace < finalPos)
			return lastSpace;
		return finalPos;
	}

	// Recursively wrap a string, preserving color codes across line breaks
	jstring wrapFormattedStringToWidth(Font &font, const jstring &text, int_t maxWidth)
	{
		int_t splitPos = sizeStringToWidth(font, text, maxWidth);
		if (text.length() <= splitPos)
			return text;

		jstring firstPart = text.substr(0, splitPos);
		jstring format = getFormatFromString(firstPart);

		int_t remainderStart = splitPos;
		if (splitPos < text.length() && text[splitPos] == u' ')
			remainderStart++;

		jstring remainder = text.substr(remainderStart);
		jstring formattedRemainder = format + remainder;

		return firstPart + u"\n" + wrapFormattedStringToWidth(font, formattedRemainder, maxWidth);
	}
}

std::vector<jstring> ChatLine::split(Font &font, const jstring &text, int_t maxWidth)
{
	jstring wrapped = wrapFormattedStringToWidth(font, text, maxWidth);
	std::vector<jstring> lines;

	jstring currentLine;
	for (int_t i = 0; i < wrapped.length(); i++)
	{
		char_t ch = wrapped[i];
		if (ch == u'\n')
		{
			if (!currentLine.empty())
			{
				lines.push_back(currentLine);
				currentLine.clear();
			}
		}
		else
		{
			currentLine.push_back(ch);
		}
	}

	if (!currentLine.empty())
		lines.push_back(currentLine);

	return lines;
}