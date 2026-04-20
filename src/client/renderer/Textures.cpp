#include "client/renderer/Textures.h"
#include "client/renderer/texturefx/TextureFX.h"

#include <cassert>
#include <mutex>
#include <thread>

#include "client/Options.h"
#include "client/skins/TexturePackRepository.h"
#include "java/File.h"

#include "OpenGL.h"
#include "httplib.h"
#include "stb_image.h"

struct HttpTexture
{
	std::mutex mutex;
	BufferedImage loadedImage;
	int_t count = 1;
	int_t id = -1;
	bool hasLoadedImage = false;
	bool isUploaded = false;
};

namespace
{
struct ParsedHttpUrl
{
	std::string host;
	std::string path;
	int port = 80;
	bool valid = false;
};

constexpr const char *BETACRAFT_PROXY_HOST = "betacraft.uk";
constexpr int BETACRAFT_PROXY_PORT = 11705;

bool usesBetacraftTextureProxy(const ParsedHttpUrl &parsed)
{
	if (parsed.host != "s3.amazonaws.com")
	{
		return false;
	}

	return parsed.path.rfind("/MinecraftSkins/", 0) == 0 || parsed.path.rfind("/MinecraftCloaks/", 0) == 0;
}

bool isSkinTextureUrl(const jstring &url)
{
	return url.find(u"MinecraftSkins") != jstring::npos || url.find(u"/skin/") != jstring::npos;
}

std::string makeHttpTextureCacheName(const ParsedHttpUrl &parsed)
{
	std::string name = parsed.host + parsed.path;
	for (char &c : name)
	{
		bool isAlphaNum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		if (!isAlphaNum && c != '.' && c != '-' && c != '_')
		{
			c = '_';
		}
	}
	return name;
}

File *openHttpTextureCacheFile(const ParsedHttpUrl &parsed)
{
	std::unique_ptr<File> workDir(File::openWorkingDirectory(u".mcbetacpp"));
	if (workDir == nullptr)
	{
		return nullptr;
	}

	std::unique_ptr<File> cacheDir(File::open(*workDir, u"http-textures"));
	if (cacheDir == nullptr)
	{
		return nullptr;
	}

	if (!cacheDir->exists() && !cacheDir->mkdirs())
	{
		return nullptr;
	}

	return File::open(*cacheDir, String::fromUTF8(makeHttpTextureCacheName(parsed)));
}

BufferedImage loadHttpTextureFromCache(const ParsedHttpUrl &parsed)
{
	std::unique_ptr<File> cacheFile(openHttpTextureCacheFile(parsed));
	if (cacheFile == nullptr || !cacheFile->exists() || !cacheFile->isFile())
	{
		return {};
	}

	std::unique_ptr<std::istream> is(cacheFile->toStreamIn());
	if (!is)
	{
		return {};
	}

	try
	{
		return BufferedImage::ImageIO_read(*is);
	}
	catch (...)
	{
		return {};
	}
}

bool storeHttpTextureCache(const ParsedHttpUrl &parsed, const std::string &body)
{
	std::unique_ptr<File> cacheFile(openHttpTextureCacheFile(parsed));
	if (cacheFile == nullptr)
	{
		return false;
	}

	std::unique_ptr<std::ostream> os(cacheFile->toStreamOut());
	if (!os)
	{
		return false;
	}

	os->write(body.data(), static_cast<std::streamsize>(body.size()));
	return os->good();
}

ParsedHttpUrl parseHttpUrl(const jstring &url)
{
	const std::string urlUtf8 = String::toUTF8(url);
	const std::string prefix = "http://";
	if (urlUtf8.compare(0, prefix.size(), prefix) != 0)
	{
		return {};
	}

	ParsedHttpUrl parsed;
	std::string hostAndPath = urlUtf8.substr(prefix.size());
	size_t pathPos = hostAndPath.find('/');
	if (pathPos == std::string::npos)
	{
		return {};
	}

	parsed.host = hostAndPath.substr(0, pathPos);
	parsed.path = hostAndPath.substr(pathPos);

	size_t portPos = parsed.host.find(':');
	if (portPos != std::string::npos)
	{
		try
		{
			parsed.port = std::stoi(parsed.host.substr(portPos + 1));
		}
		catch (...)
		{
			return {};
		}

		parsed.host = parsed.host.substr(0, portPos);
	}

	if (parsed.host.empty() || parsed.path.empty())
	{
		return {};
	}

	parsed.valid = true;
	return parsed;
}

BufferedImage readImageFromMemory(const std::string &data)
{
	int w = 0;
	int h = 0;
	int comp = 0;
	stbi_uc *rawData = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(data.data()), static_cast<int>(data.size()), &w, &h, &comp, 0);
	if (rawData == nullptr)
	{
		return {};
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
		return {};
	}

	stbi_image_free(rawData);
	return BufferedImage(w, h, std::move(pixels));
}

bool hasAlpha(const unsigned char *pixels, int_t imgWidth, int_t x0, int_t y0, int_t x1, int_t y1)
{
	for (int_t x = x0; x < x1; x++)
	{
		for (int_t y = y0; y < y1; y++)
		{
			if (pixels[(y * imgWidth + x) * 4 + 3] < 128)
			{
				return true;
			}
		}
	}

	return false;
}

void setForceAlpha(unsigned char *pixels, int_t imgWidth, int_t x0, int_t y0, int_t x1, int_t y1)
{
	if (hasAlpha(pixels, imgWidth, x0, y0, x1, y1))
	{
		return;
	}

	for (int_t x = x0; x < x1; x++)
	{
		for (int_t y = y0; y < y1; y++)
		{
			pixels[(y * imgWidth + x) * 4 + 3] = 0;
		}
	}
}

void setNoAlpha(unsigned char *pixels, int_t imgWidth, int_t x0, int_t y0, int_t x1, int_t y1)
{
	for (int_t x = x0; x < x1; x++)
	{
		for (int_t y = y0; y < y1; y++)
		{
			pixels[(y * imgWidth + x) * 4 + 3] = 255;
		}
	}
}

BufferedImage processMobSkin(BufferedImage &in)
{
	if (in.getWidth() == 0 || in.getHeight() == 0)
	{
		return {};
	}

	BufferedImage out(64, 32);
	unsigned char *dstPixels = const_cast<unsigned char *>(out.getRawPixels());
	const unsigned char *srcPixels = in.getRawPixels();
	int_t srcW = in.getWidth();
	int_t srcH = in.getHeight();

	for (int_t y = 0; y < 32; y++)
	{
		int_t srcY = (y * srcH) / 32;
		for (int_t x = 0; x < 64; x++)
		{
			int_t srcX = (x * srcW) / 64;
			int_t srcIdx = (srcY * srcW + srcX) * 4;
			int_t dstIdx = (y * 64 + x) * 4;
			dstPixels[dstIdx + 0] = srcPixels[srcIdx + 0];
			dstPixels[dstIdx + 1] = srcPixels[srcIdx + 1];
			dstPixels[dstIdx + 2] = srcPixels[srcIdx + 2];
			dstPixels[dstIdx + 3] = srcPixels[srcIdx + 3];
		}
	}

	setNoAlpha(dstPixels, 64, 0, 0, 32, 16);
	setForceAlpha(dstPixels, 64, 32, 0, 64, 32);
	setNoAlpha(dstPixels, 64, 0, 16, 64, 32);

	return out;
}

void downloadHttpTexture(const std::shared_ptr<HttpTexture> &texture, const jstring &url)
{
	const std::string urlUtf8 = String::toUTF8(url);
	try
	{
		ParsedHttpUrl parsed = parseHttpUrl(url);
		if (!parsed.valid)
		{
			std::cerr << "[HTTP Texture] Invalid URL: " << urlUtf8 << std::endl;
			return;
		}

		BufferedImage image = loadHttpTextureFromCache(parsed);
		if (image.getWidth() > 0 && image.getHeight() > 0)
		{
			std::cerr << "[HTTP Texture] Cache hit " << urlUtf8 << " size=" << image.getWidth() << "x" << image.getHeight() << std::endl;
		}
		else
		{
			const bool useBetacraftProxy = usesBetacraftTextureProxy(parsed);
			std::cerr << "[HTTP Texture] Downloading " << urlUtf8
				<< " host=" << parsed.host
				<< " port=" << parsed.port
				<< " path=" << parsed.path;
			if (useBetacraftProxy)
			{
				std::cerr << " proxyHost=" << BETACRAFT_PROXY_HOST << " proxyPort=" << BETACRAFT_PROXY_PORT;
			}
			std::cerr << std::endl;

			httplib::Client client(parsed.host.c_str(), parsed.port);
			if (useBetacraftProxy)
			{
				client.set_proxy(BETACRAFT_PROXY_HOST, BETACRAFT_PROXY_PORT);
			}
			client.set_connection_timeout(30);
			client.set_read_timeout(30);
			client.set_follow_location(true);

			auto response = client.Get(parsed.path.c_str());
			if (!response)
			{
				std::cerr << "[HTTP Texture] Request failed: " << urlUtf8 << " (null response)" << std::endl;
				return;
			}

			const std::string location = response->get_header_value("Location");
			std::cerr << "[HTTP Texture] Response " << urlUtf8 << " status=" << response->status
				<< " bytes=" << response->body.size();
			if (!location.empty())
			{
				std::cerr << " location=" << location;
			}
			std::cerr << std::endl;
			if (response->status < 200 || response->status >= 400)
			{
				return;
			}

			image = readImageFromMemory(response->body);
			if (image.getWidth() == 0 || image.getHeight() == 0)
			{
				std::cerr << "[HTTP Texture] Decode failed: " << urlUtf8 << std::endl;
				return;
			}

			if (storeHttpTextureCache(parsed, response->body))
			{
				std::cerr << "[HTTP Texture] Cached " << urlUtf8 << std::endl;
			}
		}

		if (isSkinTextureUrl(url))
		{
			image = processMobSkin(image);
		}

		std::lock_guard<std::mutex> lock(texture->mutex);
		if (image.getWidth() == 0 || image.getHeight() == 0)
		{
			std::cerr << "[HTTP Texture] Decode failed: " << urlUtf8 << std::endl;
			return;
		}

		std::cerr << "[HTTP Texture] Decoded " << urlUtf8 << " size=" << image.getWidth() << "x" << image.getHeight() << std::endl;
		texture->loadedImage = std::move(image);
		texture->hasLoadedImage = true;
	}
	catch (const std::exception &e)
	{
		std::cerr << "[HTTP Texture] Exception for " << urlUtf8 << ": " << e.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "[HTTP Texture] Unknown exception for " << urlUtf8 << std::endl;
	}
}
}

Textures::Textures(TexturePackRepository &skins, Options &options) : skins(skins), options(options)
{

}

int_t Textures::loadTexture(const jstring &resourceName)
{
	auto *skin = skins.selected;

	auto it = idMap.find(resourceName);
	if (it != idMap.end())
	{
		return it->second;
	}

	MemoryTracker::genTextures(ib);
	int_t i = ib[0];

	if (!resourceName.compare(0, 2, u"##"))
	{
		std::unique_ptr<std::istream> is(skin->getResource(resourceName.substr(2)));
		BufferedImage img = readImage(*is);
		img = makeStrip(img);
		loadTexture(img, i);
	}
	else if (!resourceName.compare(0, 7, u"%clamp%"))
	{
		std::unique_ptr<std::istream> is(skin->getResource(resourceName.substr(7)));
		BufferedImage img = readImage(*is);
		clamp = true;
		loadTexture(img, i);
		clamp = false;
	}
	else if (!resourceName.compare(0, 6, u"%blur%"))
	{
		std::unique_ptr<std::istream> is(skin->getResource(resourceName.substr(6)));
		BufferedImage img = readImage(*is);
		blur = true;
		loadTexture(img, i);
		blur = false;
	}
	else
	{
		std::unique_ptr<std::istream> is(skin->getResource(resourceName));
		BufferedImage img = readImage(*is);
		loadTexture(img, i);
	}

	idMap.emplace(resourceName, i);
	return i;
}

BufferedImage Textures::makeStrip(BufferedImage &source)
{
	int_t cols = source.getWidth() / 16;

	BufferedImage out(16, source.getHeight() * cols);

	for (int_t i = 0; i < cols; i++)
	{
		// g.drawImage(source, -i * 16, i * source.getHeight(), null);
		// TODO
	}

	return out;
}

int_t Textures::getTexture(BufferedImage &img)
{
	MemoryTracker::genTextures(Textures::ib);
	loadTexture(img, ib[0]);
	loadedImages.emplace(ib[0], std::move(img));
	return ib[0];
}

void Textures::loadTexture(BufferedImage &img, int_t id)
{
	glBindTexture(GL_TEXTURE_2D, id);

	if (MIPMAP)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	if (blur)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (clamp)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	int_t w = img.getWidth();
	int_t h = img.getHeight();

	const unsigned char *rawPixels = img.getRawPixels();
	std::unique_ptr<unsigned char[]> newPixels(new unsigned char[w * h * 4]);

	const unsigned char *rawPixels_p = rawPixels;
	unsigned char *newPixels_p = newPixels.get();

	for (int_t i = 0; i < w * h; i++, rawPixels_p += 4, newPixels_p += 4)
	{
		int_t a = rawPixels_p[3];
		int_t r = rawPixels_p[0];
		int_t g = rawPixels_p[1];
		int_t b = rawPixels_p[2];

		if (options.anaglyph3d)
		{
			int_t rr = (r * 30 + g * 59 + b * 11) / 100;
			int_t gg = (r * 30 + g * 70) / 100;
			int_t bb = (r * 30 + b * 70) / 100;

			r = rr;
			g = gg;
			b = bb;
		}

		newPixels_p[0] = r;
		newPixels_p[1] = g;
		newPixels_p[2] = b;
		newPixels_p[3] = a;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, newPixels.get());

	if (MIPMAP)
	{
		std::unique_ptr<int_t[]> mipmapPixels(new int_t[w * h * 4]);
		int_t *inPixels = (int_t*)newPixels.get();

		for (int level = 1; ; level++)
		{
			if (level > 4)
				break;

			int_t ow = w >> (level - 1);

			int_t ww = w >> level;
			int_t hh = h >> level;

			for (int_t x = 0; x < ww; x++)
			{
				for (int_t y = 0; y < hh; y++)
				{
					int_t c0 = inPixels[x * 2 + 0 + (y * 2 + 0) * ow];
					int_t c1 = inPixels[x * 2 + 1 + (y * 2 + 0) * ow];
					int_t c2 = inPixels[x * 2 + 1 + (y * 2 + 1) * ow];
					int_t c3 = inPixels[x * 2 + 0 + (y * 2 + 1) * ow];
					int_t col = crispBlend(crispBlend(c0, c1), crispBlend(c3, c2));
					mipmapPixels[x + y * ww] = col;
				}
			}

			glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, ww, hh, 0, GL_RGBA, GL_UNSIGNED_BYTE, mipmapPixels.get());

			// if (ww == 1 || hh == 1)
			// 	break;
		}
	}
}

void Textures::releaseTexture(int_t id)
{
	loadedImages.erase(id);

	ib[0] = id;
	glDeleteTextures(1, reinterpret_cast<GLuint*>(ib.data()));
}

int_t Textures::loadHttpTexture(const jstring &url, const jstring *backup)
{
	auto it = httpTextures.find(url);
	if (it == httpTextures.end())
	{
		addHttpTexture(url);
		it = httpTextures.find(url);
	}

	if (it != httpTextures.end())
	{
		HttpTexture *texture = it->second.get();
		std::lock_guard<std::mutex> lock(texture->mutex);
		if (texture->hasLoadedImage && !texture->isUploaded)
		{
			if (texture->id < 0)
			{
				texture->id = getTexture(texture->loadedImage);
			}
			else
			{
				loadTexture(texture->loadedImage, texture->id);
			}
			texture->isUploaded = true;
			std::cerr << "[HTTP Texture] Uploaded " << String::toUTF8(url) << " to GL id=" << texture->id << std::endl;
		}

		if (texture->id >= 0)
		{
			return texture->id;
		}
	}

	if (backup != nullptr)
	{
		return loadTexture(*backup);
	}

	return -1;
}

int_t Textures::loadHttpTexture(const jstring &url)
{
	return loadHttpTexture(url, nullptr);
}

HttpTexture *Textures::addHttpTexture(const jstring &url)
{
	auto it = httpTextures.find(url);
	if (it != httpTextures.end())
	{
		it->second->count++;
		std::cerr << "[HTTP Texture] Reusing cached URL " << String::toUTF8(url) << " refCount=" << it->second->count << std::endl;
		return it->second.get();
	}

	std::cerr << "[HTTP Texture] Queueing download for " << String::toUTF8(url) << std::endl;
	auto texture = Util::make_shared<HttpTexture>();
	HttpTexture *ptr = texture.get();
	httpTextures.emplace(url, texture);
	std::thread([texture, url]()
	{
		downloadHttpTexture(texture, url);
	}).detach();
	return ptr;
}

void Textures::removeHttpTexture(const jstring &url)
{
	auto it = httpTextures.find(url);
	if (it == httpTextures.end())
	{
		return;
	}

	HttpTexture *texture = it->second.get();
	texture->count--;
	std::cerr << "[HTTP Texture] Release URL " << String::toUTF8(url) << " refCount=" << texture->count << std::endl;
	if (texture->count == 0)
	{
		if (texture->id >= 0)
		{
			releaseTexture(texture->id);
		}
		httpTextures.erase(it);
	}
}

void Textures::registerTextureFX(std::unique_ptr<TextureFX> fx)
{
	fx->onTick();
	textureList.push_back(std::move(fx));
}

void Textures::tick()
{
	for (auto &fx : textureList)
	{
		fx->anaglyphEnabled = options.anaglyph3d;
		fx->onTick();

		const jstring atlas = fx->tileImage == 1 ? u"/gui/items.png" : u"/terrain.png";
		glBindTexture(GL_TEXTURE_2D, loadTexture(atlas));
		for (int_t tx = 0; tx < fx->tileSize; ++tx)
		{
			for (int_t ty = 0; ty < fx->tileSize; ++ty)
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0,
					(fx->iconIndex % 16) * 16 + tx * 16,
					(fx->iconIndex / 16) * 16 + ty * 16,
					16, 16, GL_RGBA, GL_UNSIGNED_BYTE, fx->imageData);
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, loadTexture(u"/terrain.png"));
}

int_t Textures::smoothBlend(int_t c0, int_t c1)
{
	int_t a0 = (c0 & 0xFF000000) >> 24 & 0xFF;
	int_t a1 = (c1 & 0xFF000000) >> 24 & 0xFF;
	return (((a0 + a1) >> 1) << 24) + (((c0 & 0xFEFEFE) + (c1 & 0xFEFEFE)) >> 1);
}

int_t Textures::crispBlend(int_t c0, int_t c1)
{
	int_t a0 = ((c0 & 0xFF000000) >> 24) & 0xFF;
	int_t a1 = ((c1 & 0xFF000000) >> 24) & 0xFF;

	int_t a = 255;
	if (a0 + a1 == 0)
	{
		a0 = 1;
		a1 = 1;
		a = 0;
	}

	int_t r0 = ((c0 >> 16) & 0xFF) * a0;
	int_t g0 = ((c0 >> 8) & 0xFF) * a0;
	int_t b0 = (c0 & 0xFF) * a0;

	int_t r1 = ((c1 >> 16) & 0xFF) * a1;
	int_t g1 = ((c1 >> 8) & 0xFF) * a1;
	int_t b1 = (c1 & 0xFF) * a1;

	int_t r = (r0 + r1) / (a0 + a1);
	int_t g = (g0 + g1) / (a0 + a1);
	int_t b = (b0 + b1) / (a0 + a1);

	return (a << 24) | (r << 16) | (g << 8) | b;
}

void Textures::reloadAll()
{
	TexturePack *skin = skins.selected;

	// Reload buffered textures
	for (auto &entry : loadedImages)
	{
		loadTexture(entry.second, entry.first);
	}

	// Reload resource textures
	for (auto &entry : idMap)
	{
		const jstring &name = entry.first;

		BufferedImage image;

		if (!name.compare(0, 2, u"##"))
		{
			std::unique_ptr<std::istream> is(skin->getResource(name.substr(2)));
			image = readImage(*is);
			image = makeStrip(image);
		}
		else if (!name.compare(0, 7, u"%clamp%"))
		{
			std::unique_ptr<std::istream> is(skin->getResource(name.substr(7)));
			clamp = true;
			image = readImage(*is);
		}
		else if (!name.compare(0, 6, u"%blur%"))
		{
			std::unique_ptr<std::istream> is(skin->getResource(name.substr(6)));
			blur = true;
			image = readImage(*is);
		}
		else
		{
			std::unique_ptr<std::istream> is(skin->getResource(name));
			image = readImage(*is);
		}

		int_t id = entry.second;
		loadTexture(image, id);
		blur = false;
		clamp = false;
	}
}

BufferedImage Textures::readImage(std::istream &in)
{
	return BufferedImage::ImageIO_read(in);
}

void Textures::bind(int_t id)
{
	if (id < 0) return;
	glBindTexture(GL_TEXTURE_2D, id);
}