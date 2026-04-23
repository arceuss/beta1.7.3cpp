#include "client/player/LocalPlayer.h"

#include "client/Minecraft.h"
#include "client/gui/ChestScreen.h"
#include "client/gui/DispenserScreen.h"
#include "client/gui/FurnaceScreen.h"
#include "client/gui/WorkbenchScreen.h"
#include "client/particle/TakeAnimationParticle.h"
#include "client/spc/SPCCommand.h"
#include "client/locale/Language.h"
#include "world/entity/item/EntityMinecart.h"
#include "world/level/tile/entity/DispenserTileEntity.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/level/tile/entity/SignTileEntity.h"
#include "client/gui/EditSignScreen.h"

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
	if (minecraft.screen != nullptr || sleeping)
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

void LocalPlayer::handleInsidePortal()
{
	if (changingDimensionDelay > 0)
	{
		changingDimensionDelay = 10;
		return;
	}

	isInsidePortal = true;
}

void LocalPlayer::aiStep()
{
	oPortalTime = portalTime;

	if (isInsidePortal)
	{
		if (!level.isOnline && riding != nullptr)
			ride(nullptr);

		if (minecraft.screen != nullptr)
			minecraft.setScreen(nullptr);

		if (portalTime == 0.0f)
			minecraft.soundEngine.playUI(u"portal.trigger", 1.0f, random.nextFloat() * 0.4f + 0.8f);

		portalTime += 0.0125f;
		if (portalTime >= 1.0f)
		{
			portalTime = 1.0f;
			if (!level.isOnline)
			{
				changingDimensionDelay = 10;
				minecraft.soundEngine.playUI(u"portal.travel", 1.0f, random.nextFloat() * 0.4f + 0.8f);
				minecraft.toggleDimension();
			}
		}

		isInsidePortal = false;
	}
	else
	{
		if (portalTime > 0.0f)
			portalTime -= 0.05f;

		if (portalTime < 0.0f)
			portalTime = 0.0f;
	}

	if (changingDimensionDelay > 0)
		--changingDimensionDelay;

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

void LocalPlayer::startChest(std::shared_ptr<ChestTileEntity> chest)
{
	minecraft.setScreen(Util::make_shared<ChestScreen>(minecraft, chest));
}

void LocalPlayer::startChest(std::shared_ptr<CompoundContainer> chest)
{
	minecraft.setScreen(Util::make_shared<ChestScreen>(minecraft, chest));
}

void LocalPlayer::startChest(std::shared_ptr<EntityMinecart> chest)
	{
		minecraft.setScreen(Util::make_shared<ChestScreen>(minecraft, chest));
	}

void LocalPlayer::startFurnace(std::shared_ptr<FurnaceTileEntity> furnace)
{
	minecraft.setScreen(Util::make_shared<FurnaceScreen>(minecraft, furnace));
}

void LocalPlayer::startDispenser(std::shared_ptr<DispenserTileEntity> dispenser)
{
	minecraft.setScreen(Util::make_shared<DispenserScreen>(minecraft, dispenser));
}
void LocalPlayer::openTextEdit(std::shared_ptr<SignTileEntity> sign)
{
	minecraft.setScreen(Util::make_shared<EditSignScreen>(minecraft, sign));
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

void LocalPlayer::displayClientMessage(const jstring &message)
{
	Language &language = Language::getInstance();
	jstring translated = language.getElement(message);
	if (!translated.empty())
		SPCCommand::addMessage(translated);
	else
		SPCCommand::addMessage(message);
}
