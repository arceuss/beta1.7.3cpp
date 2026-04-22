#include "client/renderer/entity/EntityRenderDispatcher.h"

#include "client/renderer/entity/FallingTileRenderer.h"
#include "client/renderer/entity/ItemRenderer.h"
#include "client/renderer/entity/MinecartRenderer.h"
#include "client/renderer/entity/PlayerRenderer.h"
#include "client/renderer/entity/TNTPrimedRenderer.h"
#include "world/entity/item/EntityItem.h"
#include "world/entity/item/FallingTile.h"
#include "world/entity/item/EntityMinecart.h"
#include "world/entity/PrimedTNT.h"
#include "OpenGL.h"


FallingTileRenderer &EntityRenderDispatcher::getFallingTileRenderer()
{
	static FallingTileRenderer renderer(instance);
	return renderer;
}


ItemRenderer &EntityRenderDispatcher::getItemRenderer()
{
	static ItemRenderer renderer(instance);
	return renderer;
}

MinecartRenderer &EntityRenderDispatcher::getMinecartRenderer()
{
	static MinecartRenderer renderer(instance);
	return renderer;
}

TNTPrimedRenderer &EntityRenderDispatcher::getTNTPrimedRenderer()
{
	static TNTPrimedRenderer renderer(instance);
	return renderer;
}


EntityRenderDispatcher EntityRenderDispatcher::instance;

double EntityRenderDispatcher::xOff = 0.0;
double EntityRenderDispatcher::yOff = 0.0;
double EntityRenderDispatcher::zOff = 0.0;

PlayerRenderer EntityRenderDispatcher::playerRenderer = PlayerRenderer(EntityRenderDispatcher::instance);

void EntityRenderDispatcher::prepare(std::shared_ptr<Level> level, Textures &textures, Font &font, std::shared_ptr<Player> player, Options &options, float a)
{
	this->level = level;
	this->textures = &textures;
	this->options = &options;
	this->player = player;
	this->font = &font;

	playerRotY = player->yRotO + (player->yRot - player->yRotO) * a;
	playerRotX = player->xRotO + (player->xRot - player->xRotO) * a;
	xPlayer = player->xOld + (player->x - player->xOld) * a;
	yPlayer = player->yOld + (player->y - player->yOld) * a;
	zPlayer = player->zOld + (player->z - player->zOld) * a;
}

void EntityRenderDispatcher::render(Entity &entity, float a)
{
	double x = entity.xOld + (entity.x - entity.xOld) * a;
	double y = entity.yOld + (entity.y - entity.yOld) * a;
	double z = entity.zOld + (entity.z - entity.zOld) * a;
	float rot = entity.yRotO + (entity.yRot - entity.yRotO) * a;

	float brightness = entity.getBrightness(a);
	glColor3f(brightness, brightness, brightness);
	render(entity, x - xOff, y - yOff, z - zOff, rot, a);
}

void EntityRenderDispatcher::render(Entity &entity, double x, double y, double z, float rot, float a)
{
	if (dynamic_cast<FallingTile *>(&entity) != nullptr)
	{
		FallingTileRenderer &fallingTileRenderer = getFallingTileRenderer();
		fallingTileRenderer.render(entity, x, y, z, rot, a);
		fallingTileRenderer.postRender(entity, x, y, z, rot, a);
		return;
	}

	if (dynamic_cast<EntityItem *>(&entity) != nullptr)
	{
		ItemRenderer &itemRenderer = getItemRenderer();
		itemRenderer.render(entity, x, y, z, rot, a);
		itemRenderer.postRender(entity, x, y, z, rot, a);
		return;
	}
	if (dynamic_cast<EntityMinecart *>(&entity) != nullptr)
	{
		MinecartRenderer &minecartRenderer = getMinecartRenderer();
		minecartRenderer.render(entity, x, y, z, rot, a);
		minecartRenderer.postRender(entity, x, y, z, rot, a);
		return;
	}

	if (dynamic_cast<PrimedTNT *>(&entity) != nullptr)
	{
		TNTPrimedRenderer &tntRenderer = getTNTPrimedRenderer();
		tntRenderer.render(entity, x, y, z, rot, a);
		tntRenderer.postRender(entity, x, y, z, rot, a);
		return;
	}

	playerRenderer.render(entity, x, y, z, rot, a);
	playerRenderer.postRender(entity, x, y, z, rot, a);
}

void EntityRenderDispatcher::setLevel(std::shared_ptr<Level> level)
{
	this->level = level;
}

double EntityRenderDispatcher::distanceToSqr(double x, double y, double z)
{
	double dx = x - xPlayer;
	double dy = y - yPlayer;
	double dz = z - zPlayer;
	return dx * dx + dy * dy + dz * dz;
}

Font &EntityRenderDispatcher::getFont()
{
	return *font;
}
