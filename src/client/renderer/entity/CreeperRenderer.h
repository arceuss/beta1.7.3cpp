#pragma once

#include "client/renderer/entity/MobRenderer.h"

class CreeperRenderer : public MobRenderer
{
public:
	explicit CreeperRenderer(EntityRenderDispatcher &entityRenderDispatcher);

protected:
	void scale(Mob &mob, float a) override;
	int_t getOverlayColor(Mob &mob, float br, float a) override;
};
