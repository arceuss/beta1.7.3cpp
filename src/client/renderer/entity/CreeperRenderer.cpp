#include "client/renderer/entity/CreeperRenderer.h"

#include "OpenGL.h"
#include "client/model/CreeperModel.h"
#include "world/entity/monster/Creeper.h"
#include "util/Mth.h"

CreeperRenderer::CreeperRenderer(EntityRenderDispatcher &entityRenderDispatcher)
	: MobRenderer(entityRenderDispatcher, std::make_shared<CreeperModel>(), 0.5f)
{
}

void CreeperRenderer::scale(Mob &mobBase, float a)
{
	Creeper &creeper = static_cast<Creeper &>(mobBase);
	float flash = creeper.getSwelling(a);
	float scaleWobble = 1.0f + Mth::sin(flash * 100.0f) * flash * 0.01f;
	if (flash < 0.0f) flash = 0.0f;
	if (flash > 1.0f) flash = 1.0f;
	flash *= flash;
	flash *= flash;
	float xz = (1.0f + flash * 0.4f) * scaleWobble;
	float y = (1.0f + flash * 0.1f) / scaleWobble;
	glScalef(xz, y, xz);
}

int_t CreeperRenderer::getOverlayColor(Mob &mobBase, float br, float a)
{
	(void)br;
	Creeper &creeper = static_cast<Creeper &>(mobBase);
	float flash = creeper.getSwelling(a);
	if (static_cast<int_t>(flash * 10.0f) % 2 == 0)
		return 0;
	int_t alpha = static_cast<int_t>(flash * 0.2f * 255.0f);
	if (alpha < 0) alpha = 0;
	if (alpha > 255) alpha = 255;
	return (alpha << 24) | 0xFFFFFF;
}
