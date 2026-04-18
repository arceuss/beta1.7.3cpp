#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

#include "java/BufferedImage.h"
#include "java/String.h"
#include "client/renderer/HttpTextureProcessor.h"

class HttpTexture
{
public:
	BufferedImage loadedImage;
	int_t count = 1;
	int_t id = -1;
	std::atomic<bool> isLoaded{false};

	HttpTexture(const jstring &url, HttpTextureProcessor *processor);

private:
	void downloadThread(const jstring &url, HttpTextureProcessor *processor);
};
