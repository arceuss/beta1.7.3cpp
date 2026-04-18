#pragma once

#include "client/gui/InventoryScreen.h"

class Level;

class WorkbenchScreen : public InventoryScreen
{
private:
	Level &level;
	int_t x = 0;
	int_t y = 0;
	int_t z = 0;

protected:
	jstring getBackgroundTexture() const override;
	int_t getCraftingGridLeft() const override;
	int_t getCraftingGridTop() const override;
	int_t getResultSlotX() const override;
	int_t getResultSlotY() const override;
	int_t getTitleX() const override;
	int_t getTitleY() const override;
	jstring getTitleText() const override;
	bool shouldRenderPlayerModel() const override;

public:
	WorkbenchScreen(Minecraft &minecraft, Level &level, int_t x, int_t y, int_t z);

	void tick() override;
};
