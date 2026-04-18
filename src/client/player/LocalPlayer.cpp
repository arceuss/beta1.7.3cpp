#include "client/player/LocalPlayer.h"

#include "client/Minecraft.h"
#include "client/gui/WorkbenchScreen.h"
#include "client/particle/TakeAnimationParticle.h"

LocalPlayer::LocalPlayer(Minecraft &minecraft, Level &level, User *user, int_t dimension) : Player(level), minecraft(minecraft)
{
	this->dimension = dimension;

	const jstring username = u"arceus413";
	name = username;
	customTextureUrl = u"http://s3.amazonaws.com/MinecraftSkins/" + username + u".png";
	customTextureUrl2 = u"http://s3.amazonaws.com/MinecraftCloaks/" + username + u".png";
}


void LocalPlayer::updateAi()
{
	Player::updateAi();
	if (minecraft.screen != nullptr)
	{
		xxa = 0.0f;
		yya = 0.0f;
		jumping = false;
	}
	else
	{
		xxa = input->xa;
		yya = input->ya;
		jumping = input->jumping;
	}
}

void LocalPlayer::aiStep()
{
	oPortalTime = portalTime;

	if (isInsidePortal)
	{
		// TODO
	}

	input->tick(*this);

	if (input->sneaking && ySlideOffset < 0.2f)
		ySlideOffset = 0.2f;

	Player::aiStep();
}

void LocalPlayer::releaseAllKeys()
{
	input->releaseAllKeys();
}

void LocalPlayer::setKey(int_t eventKey, bool eventKeyState)
{
	input->setKey(eventKey, eventKeyState);
}

void LocalPlayer::addAdditionalSaveData(CompoundTag &tag)
{
	Player::addAdditionalSaveData(tag);
}

void LocalPlayer::readAdditionalSaveData(CompoundTag &tag)
{
	Player::readAdditionalSaveData(tag);
}

void LocalPlayer::closeContainer()
{
	Player::closeContainer();
	minecraft.setScreen(nullptr);
}

void LocalPlayer::respawn()
{
	minecraft.respawnPlayer();
}

void LocalPlayer::startCrafting(int_t x, int_t y, int_t z)
{
	minecraft.setScreen(Util::make_shared<WorkbenchScreen>(minecraft, *minecraft.level, x, y, z));
}

void LocalPlayer::take(Entity &entity, int_t count)
{
	(void)count;
	if (minecraft.level == nullptr)
		return;

	std::shared_ptr<Entity> itemEntityPtr = nullptr;
	for (const auto &e : minecraft.level->getAllEntities())
	{
		if (e.get() == &entity)
		{
			itemEntityPtr = e;
			break;
		}
	}

	if (itemEntityPtr == nullptr)
		return;

	std::shared_ptr<Entity> playerPtr = nullptr;
	for (const auto &p : minecraft.level->players)
	{
		if (p.get() == this)
		{
			playerPtr = p;
			break;
		}
	}

	if (playerPtr != nullptr)
		minecraft.particleEngine.add(std::make_unique<TakeAnimationParticle>(*minecraft.level, itemEntityPtr, playerPtr, -0.5f));
}

void LocalPlayer::prepareForTick()
{

}

bool LocalPlayer::isSneaking()
{
	return input->sneaking;
}
