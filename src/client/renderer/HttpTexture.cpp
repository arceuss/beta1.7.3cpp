#include "client/renderer/HttpTexture.h"

#include "httplib.h"
#include "java/BufferedImage.h"
#include "java/String.h"
#include "java/File.h"
#include "util/Memory.h"
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <iostream>
#include <memory>

namespace
{
BufferedImage readImageFromMemory(const unsigned char *data, int_t size)
{
	if (data == nullptr || size <= 0)
	{
		return BufferedImage();
	}

	int w = 0;
	int h = 0;
	int comp = 0;
	stbi_uc *rawData = stbi_load_from_memory(data, size, &w, &h, &comp, 0);
	if (rawData == nullptr)
	{
		return BufferedImage();
	}

	std::unique_ptr<unsigned char[]> pixels = Util::make_unique<unsigned char[]>(w * h * 4);

	if (comp == 1)
	{
		for (int i = 0; i < w * h; i++)
		{
			pixels[i * 4 + 0] = rawData[i];
			pixels[i * 4 + 1] = rawData[i];
			pixels[i * 4 + 2] = rawData[i];
			pixels[i * 4 + 3] = 255;
		}
	}
	else if (comp == 2)
	{
		for (int i = 0; i < w * h; i++)
		{
			pixels[i * 4 + 0] = rawData[i * 2 + 0];
			pixels[i * 4 + 1] = rawData[i * 2 + 0];
			pixels[i * 4 + 2] = rawData[i * 2 + 0];
			pixels[i * 4 + 3] = rawData[i * 2 + 1];
		}
	}
	else if (comp == 3)
	{
		for (int i = 0; i < w * h; i++)
		{
			pixels[i * 4 + 0] = rawData[i * 3 + 0];
			pixels[i * 4 + 1] = rawData[i * 3 + 1];
			pixels[i * 4 + 2] = rawData[i * 3 + 2];
			pixels[i * 4 + 3] = 255;
		}
	}
	else if (comp == 4)
	{
		for (int i = 0; i < w * h; i++)
		{
			pixels[i * 4 + 0] = rawData[i * 4 + 0];
			pixels[i * 4 + 1] = rawData[i * 4 + 1];
			pixels[i * 4 + 2] = rawData[i * 4 + 2];
			pixels[i * 4 + 3] = rawData[i * 4 + 3];
		}
	}
	else
	{
		stbi_image_free(rawData);
		return BufferedImage();
	}

	stbi_image_free(rawData);
	return BufferedImage(w, h, std::move(pixels));
}
}

HttpTexture::HttpTexture(const jstring &url, HttpTextureProcessor *processor)
{
	std::thread downloadThread(&HttpTexture::downloadThread, this, url, processor);
	downloadThread.detach();
}

void HttpTexture::downloadThread(const jstring &url, HttpTextureProcessor *processor)
{
	try
	{
		std::string urlStr = String::toUTF8(url);

		BufferedImage image;
		bool fromCache = false;

		if (url.find(u"/skin/") != jstring::npos)
		{
			size_t skinPos = urlStr.find("/skin/");
			if (skinPos != std::string::npos)
			{
				std::string filename = urlStr.substr(skinPos + 6);
				std::unique_ptr<File> workDir(File::openWorkingDirectory(u".mcbetacpp"));
				if (workDir != nullptr)
				{
					std::unique_ptr<File> cacheDir(File::open(*workDir, u"skins"));
					if (cacheDir != nullptr)
					{
						if (!cacheDir->exists())
						{
							cacheDir->mkdirs();
						}

						std::unique_ptr<File> cacheFile(File::open(*cacheDir, String::fromUTF8(filename)));
						if (cacheFile != nullptr && cacheFile->exists() && cacheFile->isFile())
						{
							try
							{
								std::unique_ptr<std::istream> is(cacheFile->toStreamIn());
								if (is != nullptr)
								{
									image = BufferedImage::ImageIO_read(*is);
									if (image.getWidth() > 0 && image.getHeight() > 0)
									{
										fromCache = true;
									}
								}
							}
							catch (...)
							{
							}
						}
					}
				}
			}
		}

		if (!fromCache)
		{
			size_t protocolEnd = urlStr.find("://");
			if (protocolEnd == std::string::npos)
				return;

			std::string hostPort = urlStr.substr(protocolEnd + 3);
			size_t pathStart = hostPort.find('/');
			if (pathStart == std::string::npos)
				return;

			std::string host = hostPort.substr(0, pathStart);
			std::string path = hostPort.substr(pathStart);

			size_t portSep = host.find(':');
			std::string hostname = host;
			int port = 80;
			if (portSep != std::string::npos)
			{
				hostname = host.substr(0, portSep);
				port = std::stoi(host.substr(portSep + 1));
			}

			httplib::Client client(hostname.c_str(), port);
			client.set_connection_timeout(30);
			client.set_read_timeout(30);

			if (!client.is_valid())
			{
				std::cerr << "HTTP client invalid for skin: " << urlStr << " (host: " << hostname << ", port: " << port << ")" << std::endl;
			}
			else
			{
				auto res = client.Get(path.c_str());
				if (res)
				{
					if (res->status < 400 && res->status >= 200)
					{
						image = readImageFromMemory(reinterpret_cast<const unsigned char *>(res->body.data()), static_cast<int_t>(res->body.size()));
					}
					else
					{
						std::cerr << "HTTP request failed for skin: " << urlStr << " (status: " << res->status << ")" << std::endl;
					}
				}
				else
				{
					std::cerr << "HTTP request failed for skin: " << urlStr << " (request returned null)" << std::endl;
				}
			}
		}

		if (image.getWidth() > 0 && image.getHeight() > 0)
		{
			if (processor != nullptr)
			{
				loadedImage = processor->process(image);
			}
			else
			{
				loadedImage = std::move(image);
			}

			if (url.find(u"/skin/") != jstring::npos && loadedImage.getWidth() > 0 && loadedImage.getHeight() > 0)
			{
				try
				{
					size_t skinPos = urlStr.find("/skin/");
					if (skinPos != std::string::npos)
					{
						std::string filename = urlStr.substr(skinPos + 6);
						std::unique_ptr<File> workDir(File::openWorkingDirectory(u".mcbetacpp"));
						if (workDir != nullptr)
						{
							std::unique_ptr<File> cacheDir(File::open(*workDir, u"skins"));
							if (cacheDir != nullptr)
							{
								if (!cacheDir->exists())
								{
									if (!cacheDir->mkdirs())
									{
										std::cerr << "Failed to create skins directory: " << String::toUTF8(cacheDir->toString()) << std::endl;
									}
								}

								std::unique_ptr<File> cacheFile(File::open(*cacheDir, String::fromUTF8(filename)));
								if (cacheFile != nullptr)
								{
									std::string cachePath = String::toUTF8(cacheFile->toString());
									const unsigned char *pixels = loadedImage.getRawPixels();
									if (pixels != nullptr)
									{
										int result = stbi_write_png(cachePath.c_str(), loadedImage.getWidth(), loadedImage.getHeight(), 4, pixels, loadedImage.getWidth() * 4);
										if (result == 0)
										{
											std::cerr << "Failed to save skin to cache: " << cachePath << std::endl;
										}
									}
									else
									{
										std::cerr << "Failed to get pixel data for skin: " << filename << std::endl;
									}
								}
								else
								{
									std::cerr << "Failed to open cache file for skin: " << filename << std::endl;
								}
							}
							else
							{
								std::cerr << "Failed to open skins directory" << std::endl;
							}
						}
						else
						{
							std::cerr << "Failed to open working directory for skin cache" << std::endl;
						}
					}
				}
				catch (const std::exception &e)
				{
					std::cerr << "Exception while saving skin to cache: " << e.what() << std::endl;
				}
				catch (...)
				{
					std::cerr << "Unknown exception while saving skin to cache" << std::endl;
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to download HTTP texture: " << e.what() << std::endl;
	}
}
