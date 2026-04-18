#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <iostream>

#include "java/Type.h"
#include "java/String.h"

#include "client/MemoryTracker.h"
#include "java/BufferedImage.h"

#include "util/Memory.h"

class TexturePackRepository;
class Options;
class TextureFX;
struct HttpTexture;

class Textures
{
private:
	std::vector<std::unique_ptr<TextureFX>> textureList;

	std::unordered_map<jstring, int_t> idMap;
	std::unordered_map<jstring, std::shared_ptr<HttpTexture>> httpTextures;
public:
	static constexpr bool MIPMAP = false;

private:
	std::unordered_map<int_t, BufferedImage> loadedImages;

	std::vector<int_t> ib = MemoryTracker::createIntBuffer(1);
	std::vector<byte_t> pixels = MemoryTracker::createByteBuffer(0x100000);

	TexturePackRepository &skins;
	Options &options;

	bool clamp = false;
	bool blur = false;

public:
	Textures(TexturePackRepository &skins, Options &options);

	int_t loadTexture(const jstring &resourceName);

private:
	BufferedImage makeStrip(BufferedImage &source);
	HttpTexture *addHttpTexture(const jstring &url);
public:
	int_t getTexture(BufferedImage &img);
	void loadTexture(BufferedImage &img, int_t id);
	void releaseTexture(int_t id);
	int_t loadHttpTexture(const jstring &url, const jstring *backup);
	int_t loadHttpTexture(const jstring &url);
	void removeHttpTexture(const jstring &url);

	void registerTextureFX(std::unique_ptr<TextureFX> fx);
	void tick();

private:
	int_t smoothBlend(int_t c0, int_t c1);
	int_t crispBlend(int_t c0, int_t c1);

public:
	void reloadAll();

private:
	BufferedImage readImage(std::istream &in);

public:
	void bind(int_t id);
};
