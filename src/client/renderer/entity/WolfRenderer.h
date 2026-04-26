#pragma once

#include "client/renderer/entity/MobRenderer.h"

class WolfRenderer : public MobRenderer
{
public:
	WolfRenderer(EntityRenderDispatcher &entityRenderDispatcher);

protected:
	float getBob(Mob &mob, float a) override;
};
