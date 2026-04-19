#include "client/gui/FurnaceScreen.h"

#include "client/Lighting.h"
#include "client/Minecraft.h"
#include "client/locale/Language.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/renderer/entity/ItemRenderer.h"
#include "java/String.h"
#include "world/entity/player/InventoryPlayer.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/level/tile/FurnaceTile.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"

#include "OpenGL.h"
#include "lwjgl/Keyboard.h"

namespace
{
	constexpr int_t SLOT_NONE = -1;
	constexpr int_t SLOT_INPUT = 200;
	constexpr int_t SLOT_FUEL = 201;
	constexpr int_t SLOT_OUTPUT = 202;

	bool isPointInSlot(int_t relX, int_t relY, int_t slotX, int_t slotY)
	{
		return relX >= slotX - 1 && relX < slotX + 17 && relY >= slotY - 1 && relY < slotY + 17;
	}

	jstring getTooltipText(const ItemInstance &stack)
	{
		Language &language = Language::getInstance();

		// Items (ID >= 256): use item description ID with subtype support
		if (stack.itemID >= 256)
		{
			Item *item = stack.getItem();
			if (item != nullptr)
			{
				jstring descId = item->getDescriptionId(stack);
				if (!descId.empty())
				{
					jstring name = language.getElementName(descId);
					if (!name.empty())
						return name;
				}
			}
			return u"Item " + String::toString(stack.itemID);
		}

		// Blocks (ID < 256): use tile description ID
		Tile *tile = (stack.itemID >= 0 && stack.itemID < static_cast<int_t>(Tile::tiles.size())) ? Tile::tiles[stack.itemID] : nullptr;
		if (tile != nullptr && !tile->descriptionId.empty())
		{
			// Slab special case: subtype-dependent name
			if (stack.itemID == 44)
			{
				static const jstring slabNames[] = {u"tile.stoneSlab.stone", u"tile.stoneSlab.sand", u"tile.stoneSlab.wood", u"tile.stoneSlab.cobble"};
				jstring name = language.getElementName(slabNames[stack.itemDamage & 3]);
				if (!name.empty())
					return name;
			}
			else
			{
				jstring name = language.getElementName(tile->descriptionId);
				if (!name.empty())
					return name;
			}
		}

		if (stack.itemID > 0 && stack.itemID < static_cast<int_t>(Tile::tiles.size()) && Tile::tiles[stack.itemID] != nullptr)
			return u"Tile " + String::toString(stack.itemID);
		return u"Item " + String::toString(stack.itemID);
	}
}

FurnaceScreen::FurnaceScreen(Minecraft &minecraft, std::shared_ptr<FurnaceTileEntity> furnace)
	: Screen(minecraft), furnace(std::move(furnace))
{
	passEvents = true;
}

void FurnaceScreen::tick()
{
	if (minecraft.player == nullptr || furnace == nullptr || furnace->level == nullptr)
	{
		minecraft.setScreen(nullptr);
		return;
	}

	int_t tile = furnace->level->getTile(furnace->x, furnace->y, furnace->z);
	if ((tile != Tile::furnace.id && tile != Tile::furnaceLit.id) || !furnace->canUse(*minecraft.player))
		minecraft.setScreen(nullptr);
}

void FurnaceScreen::render(int_t xm, int_t ym, float a)
{
	xMouse = static_cast<float>(xm);
	yMouse = static_cast<float>(ym);

	renderBackground();
	renderBg();

	if (minecraft.player == nullptr || furnace == nullptr)
		return;

	int_t xo = getGuiLeft();
	int_t yo = getGuiTop();
	int_t relX = xm - xo;
	int_t relY = ym - yo;

	glPushMatrix();
	glTranslatef(static_cast<float>(xo), static_cast<float>(yo), 0.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_RESCALE_NORMAL);

	glPushMatrix();
	glRotatef(120.0f, 1.0f, 0.0f, 0.0f);
	Lighting::turnOn();
	glPopMatrix();

	int_t hoveredSlot = SLOT_NONE;
	int_t hoveredSlotX = -1;
	int_t hoveredSlotY = -1;

	renderSlot(furnace->getItem(0), 56, 17, a);
	if (isPointInSlot(relX, relY, 56, 17))
	{
		hoveredSlot = SLOT_INPUT;
		hoveredSlotX = 56;
		hoveredSlotY = 17;
	}

	renderSlot(furnace->getItem(1), 56, 53, a);
	if (isPointInSlot(relX, relY, 56, 53))
	{
		hoveredSlot = SLOT_FUEL;
		hoveredSlotX = 56;
		hoveredSlotY = 53;
	}

	renderSlot(furnace->getItem(2), 116, 35, a);
	if (isPointInSlot(relX, relY, 116, 35))
	{
		hoveredSlot = SLOT_OUTPUT;
		hoveredSlotX = 116;
		hoveredSlotY = 35;
	}

	for (int_t row = 0; row < 3; ++row)
	{
		for (int_t col = 0; col < 9; ++col)
		{
			int_t slot = 9 + col + row * 9;
			int_t slotX = getInventorySlotX(slot);
			int_t slotY = getInventorySlotY(slot);
			renderSlot(minecraft.player->inventory.mainInventory[slot], slotX, slotY, a);
			if (isPointInSlot(relX, relY, slotX, slotY))
			{
				hoveredSlot = slot;
				hoveredSlotX = slotX;
				hoveredSlotY = slotY;
			}
		}
	}

	for (int_t slot = 0; slot < 9; ++slot)
	{
		int_t slotX = getInventorySlotX(slot);
		int_t slotY = getInventorySlotY(slot);
		renderSlot(minecraft.player->inventory.mainInventory[slot], slotX, slotY, a);
		if (isPointInSlot(relX, relY, slotX, slotY))
		{
			hoveredSlot = slot;
			hoveredSlotX = slotX;
			hoveredSlotY = slotY;
		}
	}

	if (hoveredSlotX >= 0 && hoveredSlotY >= 0)
	{
		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		fillGradient(hoveredSlotX, hoveredSlotY, hoveredSlotX + 16, hoveredSlotY + 16, 0x80FFFFFF, 0x80FFFFFF);
		glEnable(GL_LIGHTING);
		glEnable(GL_DEPTH_TEST);
	}

	ItemInstance *carried = minecraft.player->inventory.getCarried();
	if (carried != nullptr && !carried->isEmpty())
	{
		static ItemRenderer itemRenderer(EntityRenderDispatcher::instance);
		glTranslatef(0.0f, 0.0f, 32.0f);
		itemRenderer.renderGuiItem(font, minecraft.textures, *carried, relX - 8, relY - 8);
		itemRenderer.renderGuiItemDecorations(font, minecraft.textures, *carried, relX - 8, relY - 8);
	}

	glDisable(GL_RESCALE_NORMAL);
	Lighting::turnOff();
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	renderLabels();
	if (carried == nullptr && hoveredSlot != SLOT_NONE)
	{
		const ItemInstance *hoveredItem = getSlotItem(hoveredSlot);
		if (hoveredItem != nullptr && !hoveredItem->isEmpty())
		{
			jstring tooltip = getTooltipText(*hoveredItem);
			if (!tooltip.empty())
			{
				int_t tooltipX = relX + 12;
				int_t tooltipY = relY - 12;
				int_t tooltipW = font.width(tooltip);
				fillGradient(tooltipX - 3, tooltipY - 3, tooltipX + tooltipW + 3, tooltipY + 11, 0xC0000000, 0xC0000000);
				font.drawShadow(tooltip, tooltipX, tooltipY, 0xFFFFFF);
			}
		}
	}
	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glPopMatrix();
}

bool FurnaceScreen::isPauseScreen()
{
	return false;
}

void FurnaceScreen::keyPressed(char_t eventCharacter, int_t eventKey)
{
	if (eventKey == lwjgl::Keyboard::KEY_ESCAPE || eventKey == minecraft.options.keyInventory.key)
	{
		minecraft.setScreen(nullptr);
		minecraft.grabMouse();
		return;
	}
	Screen::keyPressed(eventCharacter, eventKey);
}

void FurnaceScreen::mouseClicked(int_t x, int_t y, int_t buttonNum)
{
	if (minecraft.player == nullptr || furnace == nullptr || (buttonNum != 0 && buttonNum != 1))
		return;

	int_t xo = getGuiLeft();
	int_t yo = getGuiTop();
	bool outside = x < xo || y < yo || x >= xo + imageWidth || y >= yo + imageHeight;
	InventoryPlayer &inventory = minecraft.player->inventory;
	ItemInstance *carried = inventory.getCarried();

	if (outside)
	{
		if (carried == nullptr)
			return;

		if (buttonNum == 0)
		{
			minecraft.player->drop(*carried);
			inventory.setCarriedNull();
		}
		else
		{
			ItemInstance dropped = carried->remove(1);
			minecraft.player->drop(dropped);
			if (carried->isEmpty())
				inventory.setCarriedNull();
		}
		return;
	}

	int_t slot = getSlotAt(x, y);
	if (slot == SLOT_NONE)
		return;
	if (slot == SLOT_OUTPUT)
	{
		handleOutputSlotClick(buttonNum);
		return;
	}
	if (slot == SLOT_INPUT)
	{
		handleRegularSlotClick(furnace->getItem(0), buttonNum);
		return;
	}
	if (slot == SLOT_FUEL)
	{
		handleRegularSlotClick(furnace->getItem(1), buttonNum);
		return;
	}
	if (slot >= 0 && slot < 36)
		handleRegularSlotClick(minecraft.player->inventory.mainInventory[slot], buttonNum);
}

int_t FurnaceScreen::getGuiLeft() const
{
	return (width - imageWidth) / 2;
}

int_t FurnaceScreen::getGuiTop() const
{
	return (height - imageHeight) / 2;
}

int_t FurnaceScreen::getInventorySlotX(int_t slot) const
{
	if (slot >= 0 && slot < 9)
		return 8 + slot * 18;
	return 8 + ((slot - 9) % 9) * 18;
}

int_t FurnaceScreen::getInventorySlotY(int_t slot) const
{
	if (slot >= 0 && slot < 9)
		return 142;
	return 84 + ((slot - 9) / 9) * 18;
}

int_t FurnaceScreen::getSlotAt(int_t x, int_t y) const
{
	int_t relX = x - getGuiLeft();
	int_t relY = y - getGuiTop();
	if (isPointInSlot(relX, relY, 56, 17))
		return SLOT_INPUT;
	if (isPointInSlot(relX, relY, 56, 53))
		return SLOT_FUEL;
	if (isPointInSlot(relX, relY, 116, 35))
		return SLOT_OUTPUT;
	for (int_t slot = 0; slot < 36; ++slot)
	{
		if (isPointInSlot(relX, relY, getInventorySlotX(slot), getInventorySlotY(slot)))
			return slot;
	}
	return SLOT_NONE;
}

void FurnaceScreen::renderBg()
{
	int_t tex = minecraft.textures.loadTexture(u"/gui/furnace.png");
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	minecraft.textures.bind(tex);

	int_t xo = getGuiLeft();
	int_t yo = getGuiTop();
	blit(xo, yo, 0, 0, imageWidth, imageHeight);
	if (furnace->isBurning())
	{
		int_t burn = furnace->getBurnTimeRemainingScaled(12);
		blit(xo + 56, yo + 36 + 12 - burn, 176, 12 - burn, 14, burn + 2);
	}
	int_t cook = furnace->getCookProgressScaled(24);
	blit(xo + 79, yo + 34, 176, 14, cook + 1, 16);
}

void FurnaceScreen::renderLabels()
{
	font.draw(u"Furnace", 60, 6, 0x00404040);
	font.draw(u"Inventory", 8, imageHeight - 96 + 2, 0x00404040);
}

void FurnaceScreen::renderSlot(ItemInstance &stack, int_t x, int_t y, float a)
{
	if (stack.isEmpty())
		return;

	static ItemRenderer itemRenderer(EntityRenderDispatcher::instance);
	float pop = static_cast<float>(stack.popTime) - a;
	if (pop > 0.0f)
	{
		glPushMatrix();
		float scale = 1.0f + pop / 5.0f;
		glTranslatef(static_cast<float>(x + 8), static_cast<float>(y + 12), 0.0f);
		glScalef(1.0f / scale, (scale + 1.0f) / 2.0f, 1.0f);
		glTranslatef(static_cast<float>(-(x + 8)), static_cast<float>(-(y + 12)), 0.0f);
	}

	itemRenderer.renderGuiItem(font, minecraft.textures, stack, x, y);
	if (pop > 0.0f)
		glPopMatrix();
	itemRenderer.renderGuiItemDecorations(font, minecraft.textures, stack, x, y);
}

const ItemInstance *FurnaceScreen::getSlotItem(int_t slot) const
{
	if (furnace == nullptr)
		return nullptr;
	if (slot == SLOT_INPUT)
		return furnace->getItem(0).isEmpty() ? nullptr : &furnace->getItem(0);
	if (slot == SLOT_FUEL)
		return furnace->getItem(1).isEmpty() ? nullptr : &furnace->getItem(1);
	if (slot == SLOT_OUTPUT)
		return furnace->getItem(2).isEmpty() ? nullptr : &furnace->getItem(2);
	if (slot >= 0 && slot < 36 && minecraft.player != nullptr)
	{
		const ItemInstance &stack = minecraft.player->inventory.mainInventory[slot];
		return stack.isEmpty() ? nullptr : &stack;
	}
	return nullptr;
}

void FurnaceScreen::handleRegularSlotClick(ItemInstance &slotStack, int_t buttonNum)
{
	InventoryPlayer &inventory = minecraft.player->inventory;
	ItemInstance *carried = inventory.getCarried();

	if (slotStack.isEmpty())
	{
		if (carried == nullptr)
			return;
		int_t toPlace = buttonNum == 0 ? carried->stackSize : 1;
		if (toPlace > carried->getMaxStackSize())
			toPlace = carried->getMaxStackSize();
		if (toPlace <= 0)
			return;
		slotStack = ItemInstance(carried->itemID, toPlace, carried->itemDamage);
		carried->stackSize -= toPlace;
		if (carried->isEmpty())
			inventory.setCarriedNull();
		if (furnace != nullptr)
			furnace->setChanged();
		return;
	}

	if (carried == nullptr)
	{
		int_t toTake = buttonNum == 0 ? slotStack.stackSize : (slotStack.stackSize + 1) / 2;
		inventory.setCarried(ItemInstance(slotStack.itemID, toTake, slotStack.itemDamage));
		slotStack.stackSize -= toTake;
		if (slotStack.isEmpty())
			slotStack = ItemInstance();
		if (furnace != nullptr)
			furnace->setChanged();
		return;
	}

	bool canMerge = slotStack.sameItem(*carried) && slotStack.isStackable() && carried->isStackable();
	if (!canMerge)
	{
		ItemInstance swapped = slotStack;
		slotStack = *carried;
		inventory.setCarried(swapped);
		if (furnace != nullptr)
			furnace->setChanged();
		return;
	}

	int_t space = slotStack.getMaxStackSize() - slotStack.stackSize;
	if (space <= 0)
		return;
	int_t toMove = buttonNum == 0 ? carried->stackSize : 1;
	if (toMove > space)
		toMove = space;
	if (toMove <= 0)
		return;

	slotStack.stackSize += toMove;
	carried->stackSize -= toMove;
	if (carried->isEmpty())
		inventory.setCarriedNull();
	if (furnace != nullptr)
		furnace->setChanged();
}

void FurnaceScreen::handleOutputSlotClick(int_t buttonNum)
{
	InventoryPlayer &inventory = minecraft.player->inventory;
	ItemInstance &slotStack = furnace->getItem(2);
	if (slotStack.isEmpty())
		return;
	ItemInstance *carried = inventory.getCarried();
	if (carried == nullptr)
	{
		int_t toTake = buttonNum == 0 ? slotStack.stackSize : (slotStack.stackSize + 1) / 2;
		inventory.setCarried(ItemInstance(slotStack.itemID, toTake, slotStack.itemDamage));
		slotStack.stackSize -= toTake;
		if (slotStack.isEmpty())
			slotStack = ItemInstance();
		furnace->setChanged();
		return;
	}

	bool canMerge = slotStack.sameItem(*carried) && slotStack.isStackable() && carried->isStackable();
	if (!canMerge)
		return;
	int_t space = carried->getMaxStackSize() - carried->stackSize;
	if (space <= 0)
		return;
	int_t toMove = buttonNum == 0 ? slotStack.stackSize : 1;
	if (toMove > space)
		toMove = space;
	if (toMove <= 0)
		return;
	carried->stackSize += toMove;
	slotStack.stackSize -= toMove;
	if (slotStack.isEmpty())
		slotStack = ItemInstance();
	furnace->setChanged();
}
