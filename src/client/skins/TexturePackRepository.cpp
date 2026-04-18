#include "client/skins/TexturePackRepository.h"

#include "client/Minecraft.h"

TexturePackRepository::TexturePackRepository(Minecraft &minecraft) : minecraft(minecraft)
{
	updateListAndSelect();
}

void TexturePackRepository::updateList()
{
	texturePacks.clear();
	texturePacks.push_back(&defaultTexturePack);

	if (selected != &defaultTexturePack)
		selected = &defaultTexturePack;
}

void TexturePackRepository::updateListAndSelect()
{
	updateList();
	minecraft.options.skin = selected->name;
	selected->select();
}

const std::vector<TexturePack *> &TexturePackRepository::getTexturePacks() const
{
	return texturePacks;
}

void TexturePackRepository::setSelected(TexturePack *texturePack)
{
	if (texturePack == nullptr)
		texturePack = &defaultTexturePack;
	if (selected == texturePack)
		return;

	if (selected != nullptr)
		selected->deselect();

	selected = texturePack;
	minecraft.options.skin = selected->name;
	selected->select();
}
