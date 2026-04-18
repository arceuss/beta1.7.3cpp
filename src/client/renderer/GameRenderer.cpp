#include "client/renderer/GameRenderer.h"

#include "client/Minecraft.h"
#include "client/Lighting.h"
#include "client/gui/ScreenSizeCalculator.h"

#include "client/renderer/culling/FrustumCuller.h"

#include "world/level/chunk/ChunkCache.h"

#include "util/Mth.h"
#include "util/GLU.h"

#include "lwjgl/Display.h"
#include "lwjgl/GLContext.h"

#include "java/System.h"

#include "OpenGL.h"

#include "world/level/material/LiquidMaterial.h"

GameRenderer::GameRenderer(Minecraft &mc) : mc(mc), itemInHandRenderer(ItemInHandRenderer(mc))
{

}

void GameRenderer::tick()
{
	fogBrO = fogBr;
	float brightness = mc.level->getBrightness(Mth::floor(mc.player->x), Mth::floor(mc.player->y), Mth::floor(mc.player->z));
	float dist = (3.0f - mc.options.viewDistance) / 3.0f;
	float fogBrTarget = brightness * (1.0f - dist) + dist;
	fogBr += (fogBrTarget - fogBr) * 0.1f;

	ticks++;

	itemInHandRenderer.tick();

	// TODO tickRain rain
}

void GameRenderer::itemPlaced()
{
	itemInHandRenderer.itemPlaced();
}

void GameRenderer::itemUsed()
{
	itemInHandRenderer.itemUsed();
}

void GameRenderer::pick(float a)
{
	if (mc.player == nullptr)
		return;

	double range = mc.gameMode->getPickRange();
	mc.hitResult = mc.player->pick(range, a);

	double hitRange = range;
	Vec3 *playerPos = mc.player->getPos(a);
	if (mc.hitResult.type != HitResult::Type::NONE)
		hitRange = mc.hitResult.pos->distanceTo(*playerPos);

	if (mc.gameMode->isCreativeMode())
	{
		range = 32.0;
		hitRange = 32.0;
	}
	else
	{
		if (hitRange > 3)
			hitRange = 3;
		range = hitRange;
	}

	Vec3 *look = mc.player->getViewVector(a);
	Vec3 *to = playerPos->add(look->x * range, look->y * range, look->z * range);
	hovered = nullptr;

	float skin = 1.0f;
	const auto &es = mc.level->getEntities(mc.player.get(), *mc.player->bb.expand(look->x * range, look->y * range, look->z * range)->grow(skin, skin, skin));

	double closestDist = 0.0;

	for (auto &entity : es)
	{
		if (entity->isPickable())
		{
			float radius = entity->getPickRadius();
			AABB *bb = entity->bb.grow(radius, radius, radius);
			HitResult hr = bb->clip(*playerPos, *to);
			if (bb->contains(*playerPos))
			{
				if (0.0 < closestDist || closestDist == 0.0)
				{
					hovered = entity;
					closestDist = 0.0;
				}
			}
			else if (hr.type != HitResult::Type::NONE)
			{
				double clipDist = playerPos->distanceTo(*hr.pos);
				if (clipDist < closestDist || closestDist == 0.0)
				{
					hovered = entity;
					closestDist = clipDist;
				}
			}
		}
	}

	if (hovered != nullptr && !mc.gameMode->isCreativeMode())
		mc.hitResult = HitResult(hovered);
}

float GameRenderer::getFov(float a)
{
	auto &player = mc.player;

	float result = 70.0f;
	// TODO water material

	if (player->health <= 0)
	{
		float time = player->deathTime + a;
		result /= (1.0f - 500.0f / (time + 500.0f)) * 2.0f + 1.0f;
	}

	return result;
}

void GameRenderer::bobHurt(float a)
{
	auto &player = *mc.player;
	float hurtTime = static_cast<float>(player.hurtTime) - a;

	if (player.health <= 0)
	{
		float deathTime = static_cast<float>(player.deathTime) + a;
		glRotatef(40.0f - 8000.0f / (deathTime + 200.0f), 0.0f, 0.0f, 1.0f);
	}

	if (hurtTime >= 0.0f)
	{
		hurtTime /= static_cast<float>(player.hurtDuration);
		hurtTime = Mth::sin(hurtTime * hurtTime * hurtTime * hurtTime * Mth::PI);
		float yaw = player.hurtDir;
		glRotatef(-yaw, 0.0f, 1.0f, 0.0f);
		glRotatef(-hurtTime * 14.0f, 0.0f, 0.0f, 1.0f);
		glRotatef(yaw, 0.0f, 1.0f, 0.0f);
	}
}

void GameRenderer::bobView(float a)
{
	auto &localPlayer = *mc.player;
	float walkDelta = localPlayer.walkDist - localPlayer.walkDistO;
	float walkDist = -(localPlayer.walkDist + walkDelta * a);
	float bob = localPlayer.oBob + (localPlayer.bob - localPlayer.oBob) * a;
	float tilt = localPlayer.oTilt + (localPlayer.tilt - localPlayer.oTilt) * a;
	glTranslatef(Mth::sin(walkDist * Mth::PI) * bob * 0.5F, -std::abs(Mth::cos(walkDist * Mth::PI) * bob), 0.0F);
	glRotatef(Mth::sin(walkDist * Mth::PI) * bob * 3.0F, 0.0F, 0.0F, 1.0F);
	glRotatef(std::abs(Mth::cos(walkDist * Mth::PI - 0.2F) * bob) * 5.0F, 1.0F, 0.0F, 0.0F);
	glRotatef(tilt, 1.0F, 0.0F, 0.0F);
}

void GameRenderer::moveCameraToPlayer(float a)
{
	auto &player = *mc.player;
	double xOff = player.xOld + (player.x - player.xOld) * a;
	double yOff = player.yOld + (player.y - player.yOld) * a;
	double zOff = player.zOld + (player.z - player.zOld) * a;

	if (mc.options.thirdPersonView)
	{
		double distance = 4.0;
		float yRot = player.yRot;
		float xRot = player.xRot;
		if (lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_F1))
		{
			xRot += 180.0f;
			distance += 2.0f;
		}

		double vx = -Mth::sin(yRot / 180.0f * Mth::PI) * Mth::cos(xRot / 180.0f * Mth::PI) * distance;
		double vz = Mth::cos(yRot / 180.0f * Mth::PI) * Mth::cos(xRot / 180.0f * Mth::PI) * distance;
		double vy = -Mth::sin(xRot / 180.0f * Mth::PI) * distance;

		for (int_t i = 0; i < 8; i++)
		{
			float txo = (i & 1) * 2 - 1;
			float tyo = ((i >> 1) & 1) * 2 - 1;
			float tzo = ((i >> 2) & 1) * 2 - 1;
			txo *= 0.1F;
			tyo *= 0.1F;
			tzo *= 0.1F;
			HitResult hr = mc.level->clip(*Vec3::newTemp(xOff + txo, yOff + tyo, zOff + tzo), *Vec3::newTemp(xOff - vx + txo + tzo, yOff - vy + tyo, zOff - vz + tzo));
			if (hr.type != HitResult::Type::NONE)
			{
				double hitd = hr.pos->distanceTo(*Vec3::newTemp(xOff, yOff, zOff));
				if (hitd < distance) distance = hitd;
			}
		}

		if (lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_F1))
			glRotatef(180.0f, 0.0f, 1.0f, 0.0f);

		glRotatef(player.xRot - xRot, 1.0f, 0.0f, 0.0f);
		glRotatef(player.yRot - yRot, 0.0f, 1.0f, 0.0f);
		glTranslatef(0.0f, 0.0f, -distance);
		glRotatef(yRot - player.yRot, 0.0f, 1.0f, 0.0f);
		glRotatef(xRot - player.xRot, 1.0f, 0.0f, 0.0f);
	}
	else
	{
		glTranslatef(0.0f, 0.0f, -0.1f);
	}

	glRotatef(player.xRotO + (player.xRot - player.xRotO) * a, 1.0f, 0.0f, 0.0f);
	glRotatef(player.yRotO + (player.yRot - player.yRotO) * a + 180.0f, 0.0f, 1.0f, 0.0f);

	glTranslatef(0.0f, player.heightOffset - 1.62f, 0.0f);
}

void GameRenderer::setupCamera(float a, int_t eye)
{
	renderDistance = 256 >> mc.options.viewDistance;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float eyeDist = 0.07f;
	if (mc.options.anaglyph3d)
		glTranslatef(-(eye * 2 - 1) * eyeDist, 0.0f, 0.0f);

	if (zoom != 1.0)
	{
		glTranslatef(zoom_x, -zoom_y, 0.0f);
		glScaled(zoom, zoom, 1.0);
		gluPerspective(getFov(a), static_cast<float>(mc.width) / static_cast<float>(mc.height), 0.05f, renderDistance);
	}
	else
	{
		gluPerspective(getFov(a), static_cast<float>(mc.width) / static_cast<float>(mc.height), 0.05f, renderDistance);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if (mc.options.anaglyph3d)
		glTranslatef((eye * 2 - 1) * 0.1f, 0.0f, 0.0f);

	bobHurt(a);
	if (mc.options.bobView)
		bobView(a);

	// TODO portal effect

	moveCameraToPlayer(a);
}

void GameRenderer::renderItemInHand(float a, int_t eye)
{
	glLoadIdentity();
	if (mc.options.anaglyph3d)
		glTranslatef((eye * 2 - 1) * 0.1f, 0.0f, 0.0f);

	glPushMatrix();

	bobHurt(a);
	if (mc.options.bobView)
		bobView(a);
	if (!mc.options.thirdPersonView && !lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_F1))
		itemInHandRenderer.render(a);

	glPopMatrix();

	if (!mc.options.thirdPersonView)
	{
		itemInHandRenderer.renderScreenEffect(a);
		bobHurt(a);
	}
	if (mc.options.bobView)
		bobView(a);
}

void GameRenderer::render(float a)
{
	if (!lwjgl::Display::isActive())
	{
		if (System::currentTimeMillis() - lastActiveTime > 500)
			mc.pauseGame();
	}
	else
	{
		lastActiveTime = System::currentTimeMillis();
	}

	// Mouse movement
	if (mc.mouseGrabbed)
	{
		mc.mouseHandler.poll();
		float sens = mc.options.mouseSensitivity * 0.6f + 0.2f;
		float sens2 = sens * sens * sens * 8.0f;
		float dx = mc.mouseHandler.xd * sens2;
		float dy = mc.mouseHandler.yd * sens2;

		mc.player->turn(dx, dy * (mc.options.invertYMouse ? -1 : 1));
	}

	if (mc.noRender)
		return;

	ScreenSizeCalculator ssc(mc.options, mc.width, mc.height);
	int_t w = ssc.getWidth();
	int_t h = ssc.getHeight();

	int_t xm = lwjgl::Mouse::getX() * w / mc.width;
	int_t ym = h - lwjgl::Mouse::getY() * h / mc.height - 1;

	if (mc.level != nullptr)
	{
		renderLevel(a);
		if (!lwjgl::Keyboard::isKeyDown(lwjgl::Keyboard::KEY_F1))
			mc.gui.render(a, mc.screen != nullptr, xm, ym);
	}
	else
	{
		glViewport(0, 0, mc.width, mc.height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		setupGuiScreen();
	}

	auto screen = mc.screen; // Keep the current screen alive if render() replaces it.
	if (screen != nullptr)
	{
		glClear(GL_DEPTH_BUFFER_BIT);
		screen->render(xm, ym, a);
	}
}

void GameRenderer::renderLevel(float a)
{
	pick(a);

	auto &player = mc.player;
	auto &levelRenderer = mc.levelRenderer;
	
	double xOff = player->xOld + (player->x - player->xOld) * a;
	double yOff = player->yOld + (player->y - player->yOld) * a;
	double zOff = player->zOld + (player->z - player->zOld) * a;

	auto chunkSource = mc.level->getChunkSource();
	if (chunkSource->isChunkCache())
	{
		ChunkCache &chunkCache = static_cast<ChunkCache &>(*chunkSource);
		int_t x = Mth::floor(static_cast<float>(static_cast<int_t>(player->x))) >> 4;
		int_t z = Mth::floor(static_cast<float>(static_cast<int_t>(player->z))) >> 4;
		chunkCache.centerOn(x, z);
	}

	for (int_t eye = 0; eye < 2; eye++)
	{
		if (mc.options.anaglyph3d)
		{
			if (eye == 0)
				glColorMask(false, true, true, true);
			else
				glColorMask(true, false, false, true);
		}

		glViewport(0, 0, mc.width, mc.height);
		setupClearColor(a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		setupCamera(a, eye);
		Frustum::getFrustum();

		if (mc.options.viewDistance < 2)
		{
			setupFog(-1);
			levelRenderer.renderSky(a);
		}

		glEnable(GL_FOG);
		setupFog(1);

		FrustumCuller culler;
		culler.prepare(xOff, yOff, zOff);

		mc.levelRenderer.cull(culler, a);
		mc.levelRenderer.updateDirtyChunks(*player, Minecraft::FLYBY_MODE);
		
		setupFog(0);

		glEnable(GL_FOG);
		glBindTexture(GL_TEXTURE_2D, mc.textures.loadTexture(u"/terrain.png"));

		if (mc.options.ambientOcclusion)
			glShadeModel(GL_SMOOTH);

		Lighting::turnOff();
		levelRenderer.render(*player, 0, a);
		glShadeModel(GL_FLAT);

		Lighting::turnOn();
		levelRenderer.renderEntities(*player->getPos(a), culler, a);
		mc.particleEngine.renderLit(*player, a);
		Lighting::turnOff();

		setupFog(0);

		mc.particleEngine.render(*player, a);

		// TODO
		// water material
		if (mc.hitResult.type != HitResult::Type::NONE && false) // && player->isUnderLiquid()
		{
			glDisable(GL_ALPHA_TEST);
			// TODO inventory selected
			levelRenderer.renderHit(*player, mc.hitResult, 0, nullptr, a);
			levelRenderer.renderHitOutline(*player, mc.hitResult, 0, nullptr, a);
			glEnable(GL_ALPHA_TEST);
		}

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		setupFog(0);
		glEnable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glBindTexture(GL_TEXTURE_2D, mc.textures.loadTexture(u"/terrain.png"));

		if (mc.options.fancyGraphics)
		{
			if (mc.options.ambientOcclusion)
				glShadeModel(GL_SMOOTH);

			glColorMask(false, false, false, false);
			int_t count = levelRenderer.render(*player, 1, a);
			glColorMask(true, true, true, true);
		
			if (mc.options.anaglyph3d)
			{
				if (eye == 0)
					glColorMask(false, true, true, true);
				else
					glColorMask(true, false, false, true);
			}
		
			if (count > 0)
				levelRenderer.renderSameAsLast(1, a);

			glShadeModel(GL_FLAT);
		}
		else
		{
			if (mc.options.ambientOcclusion)
				glShadeModel(GL_SMOOTH);

			levelRenderer.render(*player, 1, a);

			glShadeModel(GL_FLAT);
		}

		glDepthMask(true);
		glEnable(GL_CULL_FACE);
		glDisable(GL_BLEND);

		// TODO
		// water material
		if (zoom == 1.0 && mc.hitResult.type != HitResult::Type::NONE) // && !player->isUnderLiquid())
		{
			glDisable(GL_ALPHA_TEST);
			levelRenderer.renderHit(*player, mc.hitResult, 0, nullptr, a);
			levelRenderer.renderHitOutline(*player, mc.hitResult, 0, nullptr, a);
			glEnable(GL_ALPHA_TEST);
		}

		glDisable(GL_FOG);
		if (hovered != nullptr)
		{

		}

		setupFog(0);
		glEnable(GL_FOG);
		levelRenderer.renderClouds(a);
		glDisable(GL_FOG);

		if (!Minecraft::FLYBY_MODE)
		{
			setupFog(1);
			if (zoom == 1.0)
			{
				glClear(GL_DEPTH_BUFFER_BIT);
				renderItemInHand(a, eye);
			}
		}

		if (!mc.options.anaglyph3d)
			return;
	}

	glColorMask(true, true, true, true);
}

void GameRenderer::setupGuiScreen()
{
	ScreenSizeCalculator ssc(mc.options, mc.width, mc.height);
	int_t w = ssc.getWidth();
	int_t h = ssc.getHeight();

	glClear(GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, w, h, 0.0, 1000.0, 3000.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -2000.0f);
}

void GameRenderer::setupClearColor(float a)
{
	auto &level = mc.level;
	auto &player = mc.player;

	float dist = 1.0f / (4 - mc.options.viewDistance);
	dist = 1.0f - std::pow(dist, 0.25);

	Vec3 *skyColor = level->getSkyColor(*mc.player, a);
	float sr = skyColor->x;
	float sg = skyColor->y;
	float sb = skyColor->z;

	Vec3 *fogColor = level->getFogColor(a);
	fr = fogColor->x;
	fg = fogColor->y;
	fb = fogColor->z;

	fr += (sr - fr) * dist;
	fg += (sg - fg) * dist;
	fb += (sb - fb) * dist;

	// Underwater/lava clear color override
	if (mc.player->isUnderLiquid(Material::water))
	{
		fr = 0.02f;
		fg = 0.02f;
		fb = 0.2f;
	}
	else if (mc.player->isUnderLiquid(Material::lava))
	{
		fr = 0.6f;
		fg = 0.1f;
		fb = 0.0f;
	}
	float fogBrNow = fogBrO + (fogBr - fogBrO) * a;
	fr *= fogBrNow;
	fg *= fogBrNow;
	fb *= fogBrNow;

	if (mc.options.anaglyph3d)
	{
		float frr = (fr * 30.0f + fg * 59.0f + fb * 11.0f) / 100.0f;
		float fgg = (fr * 30.0f + fg * 70.0f) / 100.0f;
		float fbb = (fr * 30.0f + fb * 70.0f) / 100.0f;
		fr = frr;
		fg = fgg;
		fb = fbb;
	}

	glClearColor(fr, fg, fb, 0.0f);
}

void GameRenderer::setupFog(int_t mode)
{
	auto &player = mc.player;

	float fog[4] = {fr, fg, fb, 1.0f};
	glFogfv(GL_FOG_COLOR, fog);
	glNormal3f(0.0f, -1.0f, 0.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	// Underwater/lava fog
	if (mc.player->isUnderLiquid(Material::water))
	{
		glFogi(GL_FOG_MODE, GL_EXP);
		glFogf(GL_FOG_DENSITY, 0.1f);
	}
	else if (mc.player->isUnderLiquid(Material::lava))
	{
		glFogi(GL_FOG_MODE, GL_EXP);
		glFogf(GL_FOG_DENSITY, 2.0f);
	}
	else
	{
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogf(GL_FOG_START, renderDistance * 0.25f);
		glFogf(GL_FOG_END, renderDistance);

		if (mode < 0)
		{
			glFogf(GL_FOG_START, 0.0f);
		glFogf(GL_FOG_END, renderDistance * 0.8f);
		}

		if (lwjgl::GLContext::getCapabilities()["GL_NV_fog_distance"])
			glFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_RADIAL_NV);

		if (mc.level->dimension->foggy)
			glFogf(GL_FOG_START, 0.0f);
	}

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT);
}

void GameRenderer::updateAllChunks()
{
	mc.levelRenderer.updateDirtyChunks(*mc.player, true);
}
