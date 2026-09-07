#include "client/skins/TexturePack.h"
#include "ClientTarget.h"

void TexturePack::select()
{

}

void TexturePack::deselect()
{

}

void TexturePack::load(Minecraft &minecraft)
{

}

void TexturePack::unload(Minecraft &minecraft)
{

}

void TexturePack::bindTexture(Minecraft &minecraft)
{

}

std::istream *TexturePack::getResource(const jstring &name)
{
	if (ClientTarget::isAlphaPlace() && name == u"/terrain.png")
		return Resource::getResource(u"/terrainap.png");
	return Resource::getResource(name);
}
