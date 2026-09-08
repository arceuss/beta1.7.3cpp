#include "stb_image.h"
#include "stb_image_write.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		std::cerr << "Usage: McBetaCppFrameCompare reference.png candidate.png difference.png\n";
		return 2;
	}
	int width = 0, height = 0, channels = 0, candidateWidth = 0, candidateHeight = 0;
	std::unique_ptr<unsigned char, decltype(&stbi_image_free)> reference(
		stbi_load(argv[1], &width, &height, &channels, 3), stbi_image_free);
	std::unique_ptr<unsigned char, decltype(&stbi_image_free)> candidate(
		stbi_load(argv[2], &candidateWidth, &candidateHeight, &channels, 3), stbi_image_free);
	if (!reference || !candidate)
	{
		std::cerr << "Cannot decode an input image\n";
		return 2;
	}
	if (width != candidateWidth || height != candidateHeight)
	{
		std::cerr << "Framebuffer dimensions differ\n";
		return 2;
	}
	const std::size_t pixels = static_cast<std::size_t>(width) * height;
	std::vector<unsigned char> difference(pixels * 3);
	std::uint64_t changed = 0, sum = 0;
	int maximum = 0, x0 = width, y0 = height, x1 = 0, y1 = 0;
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const std::size_t base = (static_cast<std::size_t>(y) * width + x) * 3;
			bool different = false;
			for (int channel = 0; channel < 3; ++channel)
			{
				const int error = std::abs(int(reference.get()[base + channel]) - int(candidate.get()[base + channel]));
				difference[base + channel] = static_cast<unsigned char>(error);
				sum += error;
				maximum = std::max(maximum, error);
				different |= error != 0;
			}
			if (different)
			{
				++changed;
				x0 = std::min(x0, x);
				y0 = std::min(y0, y);
				x1 = std::max(x1, x + 1);
				y1 = std::max(y1, y + 1);
			}
		}
	}
	if (!stbi_write_png(argv[3], width, height, 3, difference.data(), width * 3))
	{
		std::cerr << "Cannot write the difference image\n";
		return 2;
	}
	std::cout << "{\"width\":" << width << ",\"height\":" << height
			  << ",\"channels\":3,\"differing_pixels\":" << changed << ",\"maximum_channel_error\":" << maximum
			  << ",\"mean_channel_error\":" << static_cast<double>(sum) / (pixels * 3) << ",\"difference_bbox\":";
	if (changed)
		std::cout << '[' << x0 << ',' << y0 << ',' << x1 << ',' << y1 << ']';
	else
		std::cout << "null";
	std::cout << ",\"classification\":\"" << (changed ? "parity bug until justified" : "exact match") << "\"}\n";
	return changed ? 1 : 0;
}
