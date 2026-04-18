#pragma once

#include <vector>

#include "client/skins/DefaultTexturePack.h"

class Minecraft;
class TexturePack;

class TexturePackRepository
{
private:
	DefaultTexturePack defaultTexturePack;
	std::vector<TexturePack *> texturePacks;
	Minecraft &minecraft;

public:
	TexturePack *selected = nullptr;

	TexturePackRepository(Minecraft &minecraft);

	void updateList();
	void updateListAndSelect();
	const std::vector<TexturePack *> &getTexturePacks() const;
	void setSelected(TexturePack *texturePack);
};
