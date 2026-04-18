#include "client/renderer/ItemInHandRenderer.h"

#include "client/Minecraft.h"
#include "client/Lighting.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/renderer/Textures.h"
#include "client/renderer/entity/PlayerRenderer.h"

#include "util/Mth.h"

#include "world/level/material/LiquidMaterial.h"

ItemInHandRenderer::ItemInHandRenderer(Minecraft &mc) : mc(mc)
{

}

namespace HeldItemRenderer
{
	void render(Textures &textures, TileRenderer &tileRenderer, ItemInstance &item)
	{
		Tile *tile = item.itemID >= 0 && item.itemID < static_cast<int_t>(Tile::tiles.size()) ? Tile::tiles[item.itemID] : nullptr;
		bool renderedAsBlock = false;
		if (tile != nullptr && TileRenderer::canRender(tile->getRenderShape()))
		{
			glBindTexture(GL_TEXTURE_2D, textures.loadTexture(u"/terrain.png"));
			tileRenderer.renderTile(*tile, item.getAuxValue());
			renderedAsBlock = true;
		}
	
		if (!renderedAsBlock && !item.isEmpty())
		{
			if (item.itemID < 256)
				glBindTexture(GL_TEXTURE_2D, textures.loadTexture(u"/terrain.png"));
			else
				glBindTexture(GL_TEXTURE_2D, textures.loadTexture(u"/gui/items.png"));
	
			Tesselator &t = Tesselator::instance;
			int_t icon = item.getIcon();
			float u1 = (icon % 16 * 16 + 0.0f) / 256.0f;
			float u0 = (icon % 16 * 16 + 15.99f) / 256.0f;
			float v0 = (icon / 16 * 16 + 0.0f) / 256.0f;
			float v1 = (icon / 16 * 16 + 15.99f) / 256.0f;
			float r = 1.0f;
			float xo = 0.0f;
			float yo = 0.3f;
			glEnable(GL_RESCALE_NORMAL);
			glTranslatef(-xo, -yo, 0.0f);
			float s = 1.5f;
			glScalef(s, s, s);
			glRotatef(50.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(335.0f, 0.0f, 0.0f, 1.0f);
			glTranslatef(-0.9375f, -0.0625f, 0.0f);
			float dd = 0.0625f;
	
			t.begin();
			t.normal(0.0f, 0.0f, 1.0f);
			t.vertexUV(0.0, 0.0, 0.0, u0, v1);
			t.vertexUV(r, 0.0, 0.0, u1, v1);
			t.vertexUV(r, 1.0, 0.0, u1, v0);
			t.vertexUV(0.0, 1.0, 0.0, u0, v0);
			t.end();
	
			t.begin();
			t.normal(0.0f, 0.0f, -1.0f);
			t.vertexUV(0.0, 1.0, 0.0f - dd, u0, v0);
			t.vertexUV(r, 1.0, 0.0f - dd, u1, v0);
			t.vertexUV(r, 0.0, 0.0f - dd, u1, v1);
			t.vertexUV(0.0, 0.0, 0.0f - dd, u0, v1);
			t.end();
	
			t.begin();
			t.normal(-1.0f, 0.0f, 0.0f);
			for (int_t i = 0; i < 16; i++)
			{
				float p = i / 16.0f;
				float uu = u0 + (u1 - u0) * p - 0.001953125f;
				float xx = r * p;
				t.vertexUV(xx, 0.0, 0.0f - dd, uu, v1);
				t.vertexUV(xx, 0.0, 0.0, uu, v1);
				t.vertexUV(xx, 1.0, 0.0, uu, v0);
				t.vertexUV(xx, 1.0, 0.0f - dd, uu, v0);
			}
			t.end();
	
			t.begin();
			t.normal(1.0f, 0.0f, 0.0f);
			for (int_t i = 0; i < 16; i++)
			{
				float p = i / 16.0f;
				float uu = u0 + (u1 - u0) * p - 0.001953125f;
				float xx = r * p + 0.0625f;
				t.vertexUV(xx, 1.0, 0.0f - dd, uu, v0);
				t.vertexUV(xx, 1.0, 0.0, uu, v0);
				t.vertexUV(xx, 0.0, 0.0, uu, v1);
				t.vertexUV(xx, 0.0, 0.0f - dd, uu, v1);
			}
			t.end();
	
			t.begin();
			t.normal(0.0f, 1.0f, 0.0f);
			for (int_t i = 0; i < 16; i++)
			{
				float p = i / 16.0f;
				float vv = v1 + (v0 - v1) * p - 0.001953125f;
				float yy = r * p + 0.0625f;
				t.vertexUV(0.0, yy, 0.0, u0, vv);
				t.vertexUV(r, yy, 0.0, u1, vv);
				t.vertexUV(r, yy, 0.0f - dd, u1, vv);
				t.vertexUV(0.0, yy, 0.0f - dd, u0, vv);
			}
			t.end();
	
			t.begin();
			t.normal(0.0f, -1.0f, 0.0f);
			for (int_t i = 0; i < 16; i++)
			{
				float p = i / 16.0f;
				float vv = v1 + (v0 - v1) * p - 0.001953125f;
				float yy = r * p;
				t.vertexUV(r, yy, 0.0, u1, vv);
				t.vertexUV(0.0, yy, 0.0, u0, vv);
				t.vertexUV(0.0, yy, 0.0f - dd, u0, vv);
				t.vertexUV(r, yy, 0.0f - dd, u1, vv);
			}
			t.end();
			glDisable(GL_RESCALE_NORMAL);
		}
	}
}

void ItemInHandRenderer::renderItem(ItemInstance &item)
{
	glPushMatrix();
	HeldItemRenderer::render(mc.textures, tileRenderer, item);
	glPopMatrix();
}

void ItemInHandRenderer::render(float a)
{
	float h = oHeight + (height - oHeight) * a;
	auto &localPlayer = *mc.player;

	glPushMatrix();
	glRotatef(localPlayer.xRotO + (localPlayer.xRot - localPlayer.xRotO) * a, 1.0f, 0.0f, 0.0f);
	glRotatef(localPlayer.yRotO + (localPlayer.yRot - localPlayer.yRotO) * a, 0.0f, 1.0f, 0.0f);
	Lighting::turnOn();
	glPopMatrix();

	float br = mc.level->getBrightness(Mth::floor(localPlayer.x), Mth::floor(localPlayer.y), Mth::floor(localPlayer.z));
	glColor4f(br, br, br, 1.0f);

	ItemInstance item = selectedItem;
	if (!item.isEmpty())
	{
		glPushMatrix();
		float d = 0.8f;
		float swing = localPlayer.getAttackAnim(a);
		float swing1 = Mth::sin(swing * Mth::PI);
		float swing2 = Mth::sin(Mth::sqrt(swing) * Mth::PI);
		glTranslatef(-swing2 * 0.4f, Mth::sin(Mth::sqrt(swing) * Mth::PI * 2.0f) * 0.2f, -swing1 * 0.2f);
		glTranslatef(0.7f * d, -0.65f * d - (1.0f - h) * 0.6f, -0.9f * d);
		glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
		glEnable(GL_RESCALE_NORMAL);
		swing = localPlayer.getAttackAnim(a);
		swing1 = Mth::sin(swing * swing * Mth::PI);
		swing2 = Mth::sin(Mth::sqrt(swing) * Mth::PI);
		glRotatef(-swing1 * 20.0f, 0.0f, 1.0f, 0.0f);
		glRotatef(-swing2 * 20.0f, 0.0f, 0.0f, 1.0f);
		glRotatef(-swing2 * 80.0f, 1.0f, 0.0f, 0.0f);
		float scale = 0.4f;
		glScalef(scale, scale, scale);
		renderItem(item);
		glDisable(GL_RESCALE_NORMAL);
		glPopMatrix();
	}
	else
	{
		glPushMatrix();
		float d = 0.8f;
		float swing = localPlayer.getAttackAnim(a);
		float swing1 = Mth::sin(swing * Mth::PI);
		float swing2 = Mth::sin(Mth::sqrt(swing) * Mth::PI);
		glTranslatef(-swing2 * 0.3f, Mth::sin(Mth::sqrt(swing) * Mth::PI * 2.0f) * 0.4f, -swing1 * 0.4f);
		glTranslatef(0.8f * d, -0.75f * d - (1.0f - h) * 0.6f, -0.9f * d);
		glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
		glEnable(GL_RESCALE_NORMAL);
		swing = localPlayer.getAttackAnim(a);
		float swing3 = Mth::sin(swing * swing * Mth::PI);
		swing2 = Mth::sin(Mth::sqrt(swing) * Mth::PI);
		glRotatef(swing2 * 70.0f, 0.0f, 1.0f, 0.0f);
		glRotatef(-swing3 * 20.0f, 0.0f, 0.0f, 1.0f);
		const jstring fallbackTexture = localPlayer.getTexture();
		glBindTexture(GL_TEXTURE_2D, mc.textures.loadHttpTexture(localPlayer.customTextureUrl, &fallbackTexture));
		glTranslatef(-1.0f, 3.6f, 3.5f);
		glRotatef(120.0f, 0.0f, 0.0f, 1.0f);
		glRotatef(200.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(-135.0f, 0.0f, 1.0f, 0.0f);
		glScalef(1.0f, 1.0f, 1.0f);
		glTranslatef(5.6f, 0.0f, 0.0f);
		auto &playerRenderer = EntityRenderDispatcher::playerRenderer;
		float ss = 1.0f;
		glScalef(ss, ss, ss);
		playerRenderer.renderHand();
		glPopMatrix();
	}

	glDisable(GL_RESCALE_NORMAL);
	Lighting::turnOff();
}

void ItemInHandRenderer::renderScreenEffect(float a)
{
	glDisable(GL_ALPHA_TEST);

	if (mc.player->isOnFire())
	{
		int_t id = mc.textures.loadTexture(u"/terrain.png");
		glBindTexture(GL_TEXTURE_2D, id);
		renderFire(a);
	}

	if (mc.player->isInWall())
	{
		int_t x = Mth::floor(mc.player->x);
		int_t y = Mth::floor(mc.player->y);
		int_t z = Mth::floor(mc.player->z);

		int_t id = mc.textures.loadTexture(u"/terrain.png");
		glBindTexture(GL_TEXTURE_2D, id);
		int_t tile = mc.level->getTile(x, y, z);
		if (Tile::tiles[tile] != nullptr)
			renderTex(a, Tile::tiles[tile]->getTexture(Facing::NORTH));
	}

	if (mc.player->isUnderLiquid(Material::water))
	{
		int_t id = mc.textures.loadTexture(u"/misc/water.png");
		glBindTexture(GL_TEXTURE_2D, id);
		renderWater(a);
	}
	glEnable(GL_ALPHA_TEST);
}

void ItemInHandRenderer::renderTex(float a, int_t tex)
{
	// TODO
}

void ItemInHandRenderer::renderWater(float a)
{
	Tesselator &t = Tesselator::instance;

	float br = mc.player->getBrightness(a);
	glColor4f(br, br, br, 0.5f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix();

	float uo = -mc.player->yRot / 64.0f;
	float vo = mc.player->xRot / 64.0f;

	t.begin();
	t.vertexUV(-1.0, -1.0, -0.5, 4.0 + uo, 4.0 + vo);
	t.vertexUV( 1.0, -1.0, -0.5, 0.0 + uo, 4.0 + vo);
	t.vertexUV( 1.0,  1.0, -0.5, 0.0 + uo, 0.0 + vo);
	t.vertexUV(-1.0,  1.0, -0.5, 4.0 + uo, 0.0 + vo);
	t.end();

	glPopMatrix();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glDisable(GL_BLEND);
}

void ItemInHandRenderer::renderFire(float a)
{
	// TODO
}

void ItemInHandRenderer::tick()
{
	oHeight = height;

	auto &localPlayer = *mc.player;
	ItemInstance *selected = localPlayer.inventory.getSelected();
	bool selectedEmpty = selected == nullptr;
	bool selectedItemEmpty = selectedItem.isEmpty();
	bool matches = lastSlot == localPlayer.inventory.currentItem;

	if (selectedEmpty && selectedItemEmpty)
	{
		matches = true;
	}
	else if (!selectedEmpty && !selectedItemEmpty)
	{
		// Java keeps the selected slot's ItemInstance reference; with value storage we preserve
		// that behavior by treating same-id replacements as the same held item and refreshing the copy.
		matches = matches && selected->itemID == selectedItem.itemID;
		if (selected->itemID == selectedItem.itemID)
		{
			selectedItem = *selected;
			matches = true;
		}
	}
	else
	{
		matches = false;
	}

	float max = 0.4f;
	float tHeight = matches ? 1.0f : 0.0f;
	float dd = tHeight - height;
	if (dd < -max) dd = -max;
	if (dd > max) dd = max;
	height += dd;
	if (height < 0.1f)
	{
		if (selected != nullptr)
			selectedItem = *selected;
		else
			selectedItem = ItemInstance();
		lastSlot = localPlayer.inventory.currentItem;
	}
}

void ItemInHandRenderer::itemPlaced()
{
	height = 0.0f;
}

void ItemInHandRenderer::itemUsed()
{
	height = 0.0f;
}
