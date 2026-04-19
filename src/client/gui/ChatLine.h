#pragma once

#include <vector>
#include "java/String.h"
#include "java/Type.h"

class Font;

// Splits a chat message string into rendered lines, preserving color codes across wraps
namespace ChatLine
{
	// Split text into lines that fit within maxWidth pixels
	std::vector<jstring> split(Font &font, const jstring &text, int_t maxWidth);
}