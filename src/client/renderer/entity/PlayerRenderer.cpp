#include "client/renderer/entity/PlayerRenderer.h"

#include "client/renderer/Textures.h"
#include "client/renderer/ItemInHandRenderer.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "java/String.h"
#include "world/entity/player/Player.h"
#include "util/Mth.h"

#include <cmath>
#include <iostream>

#include "OpenGL.h"

PlayerRenderer::PlayerRenderer(EntityRenderDispatcher &entityRenderDispatcher) : MobRenderer(entityRenderDispatcher, Util::make_shared<HumanoidModel>(0.0f), 0.5f)
{
	humanoidModel = std::static_pointer_cast<HumanoidModel>(model);

	armorParts1 = Util::make_shared<HumanoidModel>(1.0f);
	armorParts2 = Util::make_shared<HumanoidModel>(0.5f);
}

void PlayerRenderer::render(Entity &entity, double x, double y, double z, float rot, float a)
{
	Player &mob = reinterpret_cast<Player &>(entity);

	humanoidModel->holdingRightHand = mob.inventory.getSelected() != nullptr;
	humanoidModel->sneaking = mob.isSneaking();

	double yp = y - mob.heightOffset;
	if (mob.isSneaking())
		yp -= 0.125;

	MobRenderer::render(mob, x, yp, z, rot, a);

	humanoidModel->holdingRightHand = false;
	humanoidModel->sneaking = false;
}

void PlayerRenderer::scale(Mob &mob, float a)
{
	float s = 0.9375f;
	glScalef(s, s, s);
}

void PlayerRenderer::additionalRendering(Mob &mobBase, float a)
{
	Player &mob = static_cast<Player &>(mobBase);
	Textures *textures = entityRenderDispatcher.textures;
	if (textures == nullptr)
		return;

	if (!mob.customTextureUrl2.empty())
	{
		int_t cloakId = textures->loadHttpTexture(mob.customTextureUrl2);
		static jstring lastLoggedCloakUrl;
		static int_t lastLoggedCloakId = -2;
		if (lastLoggedCloakUrl != mob.customTextureUrl2 || lastLoggedCloakId != cloakId)
		{
			std::cerr << "[Cape] Render check url=" << String::toUTF8(mob.customTextureUrl2) << " textureId=" << cloakId << std::endl;
			lastLoggedCloakUrl = mob.customTextureUrl2;
			lastLoggedCloakId = cloakId;
		}
		if (cloakId >= 0)
		{
			glBindTexture(GL_TEXTURE_2D, cloakId);
			glPushMatrix();
			glTranslatef(0.0f, 0.0f, 2.0f / 16.0f);

			double cloakX = mob.xCloakO + (mob.xCloak - mob.xCloakO) * a - (mob.xo + (mob.x - mob.xo) * a);
			double cloakY = mob.yCloakO + (mob.yCloak - mob.yCloakO) * a - (mob.yo + (mob.y - mob.yo) * a);
			double cloakZ = mob.zCloakO + (mob.zCloak - mob.zCloakO) * a - (mob.zo + (mob.z - mob.zo) * a);
			float bodyRot = mob.yBodyRotO + (mob.yBodyRot - mob.yBodyRotO) * a;
			double sinRot = std::sin(bodyRot * Mth::PI / 180.0f);
			double cosRot = -std::cos(bodyRot * Mth::PI / 180.0f);

			float pitch = static_cast<float>(cloakY * 10.0);
			if (pitch < -6.0f) pitch = -6.0f;
			if (pitch > 32.0f) pitch = 32.0f;
			float sway = static_cast<float>((cloakX * sinRot + cloakZ * cosRot) * 100.0);
			float twist = static_cast<float>((cloakX * cosRot - cloakZ * sinRot) * 100.0);
			if (sway < 0.0f) sway = 0.0f;
			float walkBob = mob.oBob + (mob.bob - mob.oBob) * a;
			pitch += std::sin((mob.walkDistO + (mob.walkDist - mob.walkDistO) * a) * 6.0f) * 32.0f * walkBob;
			if (mob.isSneaking()) pitch += 25.0f;

			glRotatef(6.0f + sway / 2.0f + pitch, 1.0f, 0.0f, 0.0f);
			glRotatef(twist / 2.0f, 0.0f, 0.0f, 1.0f);
			glRotatef(-twist / 2.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
			humanoidModel->cloak.render(1.0f / 16.0f);
			glPopMatrix();
		}
	}

	ItemInstance *item = mob.inventory.getSelected();
	if (item == nullptr)
		return;

	glPushMatrix();
	humanoidModel->arm0.translateTo(1.0f / 16.0f);
	glTranslatef(-1.0f / 16.0f, 7.0f / 16.0f, 1.0f / 16.0f);

	Tile *tile = item->itemID >= 0 && item->itemID < static_cast<int_t>(Tile::tiles.size()) ? Tile::tiles[item->itemID] : nullptr;
	if (tile != nullptr && TileRenderer::canRender(tile->getRenderShape()))
	{
		float s = 0.5f;
		glTranslatef(0.0f, 0.1875f, -0.3125f);
		s *= 0.75f;
		glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
		glScalef(s, -s, s);
	}
	else
	{
		float s = 0.375f;
		glTranslatef(0.25f, 0.1875f, -0.1875f);
		glScalef(s, s, s);
		glRotatef(60.0f, 0.0f, 0.0f, 1.0f);
		glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(20.0f, 0.0f, 0.0f, 1.0f);
	}

	TileRenderer tileRenderer(false, false);
	HeldItemRenderer::render(*textures, tileRenderer, *item);
	glPopMatrix();
}

void PlayerRenderer::renderHand()
{
	humanoidModel->attackTime = 0.0f;
	humanoidModel->setupAnim(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f / 16.0f);
	humanoidModel->arm0.render(1.0f / 16.0f);
}
