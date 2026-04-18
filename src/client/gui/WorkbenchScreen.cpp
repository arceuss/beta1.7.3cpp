#include "client/gui/WorkbenchScreen.h"

#include "client/Minecraft.h"
#include "world/level/Level.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/WorkbenchTile.h"

WorkbenchScreen::WorkbenchScreen(Minecraft &minecraft, Level &level, int_t x, int_t y, int_t z)
	: InventoryScreen(minecraft, 3, 3), level(level), x(x), y(y), z(z)
{
}

void WorkbenchScreen::tick()
{
	if (minecraft.player == nullptr)
		return;

	if (level.getTile(x, y, z) != Tile::workBench.id || minecraft.player->distanceToSqr(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5) > 64.0)
		minecraft.setScreen(nullptr);
}

jstring WorkbenchScreen::getBackgroundTexture() const
{
	return u"/gui/crafting.png";
}

int_t WorkbenchScreen::getCraftingGridLeft() const
{
	return 30;
}

int_t WorkbenchScreen::getCraftingGridTop() const
{
	return 17;
}

int_t WorkbenchScreen::getResultSlotX() const
{
	return 124;
}

int_t WorkbenchScreen::getResultSlotY() const
{
	return 35;
}

int_t WorkbenchScreen::getTitleX() const
{
	return 28;
}

int_t WorkbenchScreen::getTitleY() const
{
	return 6;
}

jstring WorkbenchScreen::getTitleText() const
{
	return u"Crafting";
}

bool WorkbenchScreen::shouldRenderPlayerModel() const
{
	return false;
}
