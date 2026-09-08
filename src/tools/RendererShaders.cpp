#include "client/renderer/portable/ShaderSource.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
try
{
	if (argc != 3)
		throw std::invalid_argument("Expected vertex and fragment output paths");
	std::ofstream vertex(argv[1], std::ios::binary);
	std::ofstream fragment(argv[2], std::ios::binary);
	if (!vertex || !fragment)
		throw std::runtime_error("Cannot create shader source outputs");
	vertex << b173::render::vertexShader(b173::render::ShaderLanguage::Vulkan450);
	fragment << b173::render::fragmentShader(b173::render::ShaderLanguage::Vulkan450);
	vertex.close();
	fragment.close();
	if (!vertex || !fragment)
		throw std::runtime_error("Cannot write shader source outputs");
	return 0;
}
catch (const std::exception &error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
