#include "client/renderer/entity/WolfRenderer.h"

#include "client/model/WolfModel.h"
#include "world/entity/animal/Wolf.h"

WolfRenderer::WolfRenderer(EntityRenderDispatcher &entityRenderDispatcher)
	: MobRenderer(entityRenderDispatcher, std::make_shared<WolfModel>(), 0.5f)
{
}

float WolfRenderer::getBob(Mob &mobBase, float a)
{
	Wolf &wolf = static_cast<Wolf &>(mobBase);
	if (auto wolfModel = std::dynamic_pointer_cast<WolfModel>(model))
	{
		wolfModel->sitting = wolf.isWolfSitting();
		wolfModel->angry = wolf.isWolfAngry();
		wolfModel->interestedAngle = wolf.getInterestedAngle(a);
	}
	return wolf.getTailRotation();
}
