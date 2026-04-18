#pragma once

#include "client/renderer/TileRenderer.h"
#include "world/item/ItemInstance.h"

#include "java/Type.h"

class Minecraft;
class Textures;

namespace HeldItemRenderer
{
	void render(Textures &textures, TileRenderer &tileRenderer, ItemInstance &item);
}
class ItemInHandRenderer
{
private:
	Minecraft &mc;
	ItemInstance selectedItem;

	float height = 0.0f;
	float oHeight = 0.0f;
	TileRenderer tileRenderer;

	int_t lastSlot = -1;

	void renderItem(ItemInstance &item);

public:
	ItemInHandRenderer(Minecraft &mc);

	void render(float a);
	void renderScreenEffect(float a);
	void renderTex(float a, int_t tex);
	void renderWater(float a);
	void renderFire(float a);

	void tick();
	void itemPlaced();
	void itemUsed();
};