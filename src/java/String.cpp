#include "java/String.h"

#include <locale>
#include <codecvt>
#include <type_traits>

namespace String
{

jstring fromUTF8(const std::string &str)
{
	std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
	return converter.from_bytes(str);
}
std::string toUTF8(const jstring &str)
{
	std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
	return converter.to_bytes(str);
}

template <typename T>
static jstring intToStringImpl(T v, int_t base)
{
	if (base < 2 || base > 36)
		base = 10;
	jstring out;
	bool negative = v < 0;
	typedef typename std::make_unsigned<T>::type Unsigned;
	Unsigned magnitude = static_cast<Unsigned>(v);
	if (negative)
		magnitude = Unsigned(0) - magnitude;

	while (magnitude)
	{
		int_t digit = static_cast<int_t>(magnitude % static_cast<Unsigned>(base));
		magnitude /= static_cast<Unsigned>(base);
		out.insert(out.begin(), static_cast<char16_t>(digit + (digit < 10 ? '0' : 'a' - 10)));
	}

	if (out.empty())
		out.push_back('0');

	if (negative)
		out.insert(out.begin(), '-');

	return out;
}

jstring toString(int_t v, int_t base)
{
	return intToStringImpl<int_t>(v, base);
}

jstring toString(long_t v, int_t base)
{
	return intToStringImpl<long_t>(v, base);
}

jstring toString(uint_t v, int_t base)
{
	return intToStringImpl<uint_t>(v, base);
}

jstring toString(ulong_t v, int_t base)
{
	return intToStringImpl<ulong_t>(v, base);
}

jstring toString(float v)
{
	return fromUTF8(std::to_string(v));
}

jstring toString(double v)
{
	return fromUTF8(std::to_string(v));
}

}
