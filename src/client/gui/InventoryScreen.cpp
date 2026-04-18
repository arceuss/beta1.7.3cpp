#include "client/gui/InventoryScreen.h"

#include <cmath>

#include "client/Lighting.h"
#include "client/Minecraft.h"
#include "client/locale/Language.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/renderer/entity/ItemRenderer.h"
#include "java/String.h"
#include "world/entity/player/InventoryPlayer.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include "world/item/crafting/CraftingContainer.h"
#include "world/item/crafting/Recipes.h"
#include "world/level/tile/Tile.h"

#include "OpenGL.h"
#include "lwjgl/Keyboard.h"

namespace
{
	constexpr int_t SLOT_NONE = -1;
	constexpr int_t SLOT_CRAFTING_BASE = 100;
	constexpr int_t SLOT_RESULT = 200;

	bool isPointInSlot(int_t relX, int_t relY, int_t slotX, int_t slotY)
	{
		return relX >= slotX - 1 && relX < slotX + 17 && relY >= slotY - 1 && relY < slotY + 17;
	}

	class GridCraftingContainer : public CraftingContainer
	{
	private:
		const std::vector<ItemInstance> &slots;
		int_t width = 0;
		int_t height = 0;

	public:
		GridCraftingContainer(const std::vector<ItemInstance> &slots, int_t width, int_t height)
			: slots(slots), width(width), height(height)
		{
		}

		ItemInstance getItem(int_t x, int_t y) const override
		{
			if (x < 0 || y < 0 || x >= width || y >= height)
				return ItemInstance();
			return slots[x + y * width];
		}
	};

	jstring getTooltipText(const ItemInstance &stack)
	{
		Language &language = Language::getInstance();
		Item *item = stack.getItem();
		if (item != nullptr && !item->getDescriptionId().empty())
		{
			jstring itemName = language.getElementName(item->getDescriptionId());
			if (!itemName.empty())
				return itemName;
		}

		switch (stack.itemID)
		{
		case 1: return language.getElementName(u"tile.stone");
		case 2: return language.getElementName(u"tile.grass");
		case 3: return language.getElementName(u"tile.dirt");
		case 4: return language.getElementName(u"tile.stonebrick");
		case 5: return language.getElementName(u"tile.wood");
		case 7: return language.getElementName(u"tile.bedrock");
		case 8:
		case 9: return language.getElementName(u"tile.water");
		case 10:
		case 11: return language.getElementName(u"tile.lava");
		case 12: return language.getElementName(u"tile.sand");
		case 13: return language.getElementName(u"tile.gravel");
		case 14: return language.getElementName(u"tile.oreGold");
		case 15: return language.getElementName(u"tile.oreIron");
		case 16: return language.getElementName(u"tile.oreCoal");
		case 17: return language.getElementName(u"tile.log");
		case 18: return language.getElementName(u"tile.leaves");
		case 21: return language.getElementName(u"tile.oreLapis");
		case 24: return language.getElementName(u"tile.sandStone");
		case 31: return u"Tall Grass";
		case 32: return u"Dead Bush";
		case 37: return language.getElementName(u"tile.flower");
		case 38: return language.getElementName(u"tile.rose");
		case 39:
		case 40: return language.getElementName(u"tile.mushroom");
		case 48: return language.getElementName(u"tile.stoneMoss");
		case 49: return language.getElementName(u"tile.obsidian");
		case 56: return language.getElementName(u"tile.oreDiamond");
		case 58: return language.getElementName(u"tile.workbench");
		case 73: return language.getElementName(u"tile.oreRedstone");
		case 78: return language.getElementName(u"tile.snow");
		case 79: return language.getElementName(u"tile.ice");
		case 81: return language.getElementName(u"tile.cactus");
		case 82: return language.getElementName(u"tile.clay");
		case 83: return language.getElementName(u"tile.reeds");
		case 86: return language.getElementName(u"tile.pumpkin");
		default: break;
		}

		if (stack.itemID > 0 && stack.itemID < static_cast<int_t>(Tile::tiles.size()) && Tile::tiles[stack.itemID] != nullptr)
			return u"Tile " + String::toString(stack.itemID);
		return u"Item " + String::toString(stack.itemID);
	}
}

InventoryScreen::InventoryScreen(Minecraft &minecraft, int_t craftingWidth, int_t craftingHeight)
	: Screen(minecraft), craftingSlots(craftingWidth * craftingHeight), craftingWidth(craftingWidth), craftingHeight(craftingHeight)
{
	passEvents = true;
	updateCraftingResult();
}

void InventoryScreen::render(int_t xm, int_t ym, float a)
{
	xMouse = static_cast<float>(xm);
	yMouse = static_cast<float>(ym);

	renderBackground();

	int_t xo = getGuiLeft();
	int_t yo = getGuiTop();
	renderBg(a);

	if (minecraft.player == nullptr)
		return;

	glPushMatrix();
	glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
	Lighting::turnOn();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(static_cast<float>(xo), static_cast<float>(yo), 0.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_RESCALE_NORMAL);

	int_t hoveredSlot = SLOT_NONE;
	int_t hoveredSlotX = -1;
	int_t hoveredSlotY = -1;
	int_t relX = xm - xo;
	int_t relY = ym - yo;

	for (int_t slot = 0; slot < static_cast<int_t>(craftingSlots.size()); ++slot)
	{
		int_t slotX = getCraftingSlotX(slot);
		int_t slotY = getCraftingSlotY(slot);
		renderSlot(craftingSlots[slot], slotX, slotY, a);
		if (isPointInSlot(relX, relY, slotX, slotY))
		{
			hoveredSlot = SLOT_CRAFTING_BASE + slot;
			hoveredSlotX = slotX;
			hoveredSlotY = slotY;
		}
	}

	int_t resultSlotX = getResultSlotX();
	int_t resultSlotY = getResultSlotY();
	renderSlot(craftingResult, resultSlotX, resultSlotY, a);
	if (isPointInSlot(relX, relY, resultSlotX, resultSlotY))
	{
		hoveredSlot = SLOT_RESULT;
		hoveredSlotX = resultSlotX;
		hoveredSlotY = resultSlotY;
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

bool InventoryScreen::isPauseScreen()
{
	return false;
}

void InventoryScreen::removed()
{
	dropCraftingContents();
	Screen::removed();
}

void InventoryScreen::keyPressed(char_t eventCharacter, int_t eventKey)
{
	if (eventKey == lwjgl::Keyboard::KEY_ESCAPE || eventKey == minecraft.options.keyInventory.key)
	{
		minecraft.setScreen(nullptr);
		minecraft.grabMouse();
		return;
	}

	Screen::keyPressed(eventCharacter, eventKey);
}

void InventoryScreen::mouseClicked(int_t x, int_t y, int_t buttonNum)
{
	if (minecraft.player == nullptr || (buttonNum != 0 && buttonNum != 1))
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
	if (slot != SLOT_NONE)
		handleSlotClick(slot, buttonNum);
}

int_t InventoryScreen::getGuiLeft() const
{
	return (width - imageWidth) / 2;
}

int_t InventoryScreen::getGuiTop() const
{
	return (height - imageHeight) / 2;
}

int_t InventoryScreen::getInventorySlotX(int_t slot) const
{
	if (slot >= 0 && slot < 9)
		return 8 + slot * 18;
	return 8 + ((slot - 9) % 9) * 18;
}

int_t InventoryScreen::getInventorySlotY(int_t slot) const
{
	if (slot >= 0 && slot < 9)
		return 142;
	return 84 + ((slot - 9) / 9) * 18;
}

int_t InventoryScreen::getCraftingSlotX(int_t slot) const
{
	return getCraftingGridLeft() + (slot % craftingWidth) * 18;
}

int_t InventoryScreen::getCraftingSlotY(int_t slot) const
{
	return getCraftingGridTop() + (slot / craftingWidth) * 18;
}

int_t InventoryScreen::getSlotAt(int_t x, int_t y) const
{
	int_t relX = x - getGuiLeft();
	int_t relY = y - getGuiTop();

	if (isPointInSlot(relX, relY, getResultSlotX(), getResultSlotY()))
		return SLOT_RESULT;

	for (int_t slot = 0; slot < static_cast<int_t>(craftingSlots.size()); ++slot)
	{
		if (isPointInSlot(relX, relY, getCraftingSlotX(slot), getCraftingSlotY(slot)))
			return SLOT_CRAFTING_BASE + slot;
	}

	for (int_t slot = 0; slot < 36; ++slot)
	{
		if (isPointInSlot(relX, relY, getInventorySlotX(slot), getInventorySlotY(slot)))
			return slot;
	}
	return SLOT_NONE;
}

void InventoryScreen::updateCraftingResult()
{
	GridCraftingContainer container(craftingSlots, craftingWidth, craftingHeight);
	craftingResult = Recipes::getInstance().getItemFor(container);
}

void InventoryScreen::consumeCraftingIngredients()
{
	for (ItemInstance &stack : craftingSlots)
	{
		if (stack.isEmpty())
			continue;
		--stack.stackSize;
		if (stack.isEmpty())
			stack = ItemInstance();
	}
}

void InventoryScreen::dropCraftingContents()
{
	if (minecraft.player == nullptr)
		return;

	for (ItemInstance &stack : craftingSlots)
	{
		if (stack.isEmpty())
			continue;
		minecraft.player->drop(stack);
		stack = ItemInstance();
	}
	craftingResult = ItemInstance();
}

void InventoryScreen::renderBg(float a)
{
	(void)a;
	int_t tex = minecraft.textures.loadTexture(getBackgroundTexture());
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	minecraft.textures.bind(tex);

	int_t xo = getGuiLeft();
	int_t yo = getGuiTop();
	blit(xo, yo, 0, 0, imageWidth, imageHeight);

	if (minecraft.player == nullptr || !shouldRenderPlayerModel())
		return;

	glEnable(GL_RESCALE_NORMAL);
	glEnable(GL_LIGHTING);
	renderPlayerModel(xo + 51, yo + 75, 30);
	glDisable(GL_RESCALE_NORMAL);
}

void InventoryScreen::renderLabels()
{
	jstring title = getTitleText();
	if (!title.empty())
		font.draw(title, getTitleX(), getTitleY(), 0x00404040);
}

void InventoryScreen::renderPlayerModel(int_t x, int_t y, int_t scale)
{
	float oldBodyRot = minecraft.player->yBodyRot;
	float oldYRot = minecraft.player->yRot;
	float oldXRot = minecraft.player->xRot;

	glPushMatrix();
	glTranslatef(static_cast<float>(x), static_cast<float>(y), 50.0f);
	glScalef(static_cast<float>(-scale), static_cast<float>(scale), static_cast<float>(scale));
	glRotatef(180.0f, 0.0f, 0.0f, 1.0f);

	float xd = static_cast<float>(x) - xMouse;
	float yd = static_cast<float>(y - 50) - yMouse;

	glRotatef(135.0f, 0.0f, 1.0f, 0.0f);
	Lighting::turnOn();
	glRotatef(-135.0f, 0.0f, 1.0f, 0.0f);
	glRotatef(-static_cast<float>(std::atan(yd / 40.0f)) * 20.0f, 1.0f, 0.0f, 0.0f);

	minecraft.player->yBodyRot = static_cast<float>(std::atan(xd / 40.0f)) * 20.0f;
	minecraft.player->yRot = static_cast<float>(std::atan(xd / 40.0f)) * 40.0f;
	minecraft.player->xRot = -static_cast<float>(std::atan(yd / 40.0f)) * 20.0f;

	glTranslatef(0.0f, minecraft.player->heightOffset, 0.0f);
	EntityRenderDispatcher::instance.render(*minecraft.player, 0.0, 0.0, 0.0, 0.0f, 1.0f);

	minecraft.player->yBodyRot = oldBodyRot;
	minecraft.player->yRot = oldYRot;
	minecraft.player->xRot = oldXRot;

	glPopMatrix();
	Lighting::turnOff();
}

void InventoryScreen::renderSlot(ItemInstance &stack, int_t x, int_t y, float a)
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

const ItemInstance *InventoryScreen::getSlotItem(int_t slot) const
{
	if (slot == SLOT_RESULT)
		return craftingResult.isEmpty() ? nullptr : &craftingResult;
	if (slot >= SLOT_CRAFTING_BASE)
	{
		int_t craftingSlot = slot - SLOT_CRAFTING_BASE;
		if (craftingSlot >= 0 && craftingSlot < static_cast<int_t>(craftingSlots.size()) && !craftingSlots[craftingSlot].isEmpty())
			return &craftingSlots[craftingSlot];
		return nullptr;
	}
	if (slot >= 0 && slot < 36)
	{
		const ItemInstance &stack = minecraft.player->inventory.mainInventory[slot];
		return stack.isEmpty() ? nullptr : &stack;
	}
	return nullptr;
}

void InventoryScreen::handleRegularSlotClick(ItemInstance &slotStack, int_t buttonNum)
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
		return;
	}

	if (carried == nullptr)
	{
		int_t toTake = buttonNum == 0 ? slotStack.stackSize : (slotStack.stackSize + 1) / 2;
		inventory.setCarried(ItemInstance(slotStack.itemID, toTake, slotStack.itemDamage));
		slotStack.stackSize -= toTake;
		if (slotStack.isEmpty())
			slotStack = ItemInstance();
		return;
	}

	bool canMerge = slotStack.sameItem(*carried) && slotStack.isStackable() && carried->isStackable();
	if (!canMerge)
	{
		ItemInstance swapped = slotStack;
		slotStack = *carried;
		inventory.setCarried(swapped);
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
}

void InventoryScreen::handleSlotClick(int_t slot, int_t buttonNum)
{
	InventoryPlayer &inventory = minecraft.player->inventory;
	if (slot == SLOT_RESULT)
	{
		if (craftingResult.isEmpty())
			return;

		ItemInstance *carried = inventory.getCarried();
		if (carried == nullptr)
			inventory.setCarried(craftingResult);
		else
		{
			bool canMerge = carried->sameItem(craftingResult) && carried->isStackable() && craftingResult.isStackable();
			if (!canMerge)
				return;

			int_t space = carried->getMaxStackSize() - carried->stackSize;
			if (space < craftingResult.stackSize)
				return;
			carried->stackSize += craftingResult.stackSize;
		}

		consumeCraftingIngredients();
		updateCraftingResult();
		return;
	}

	if (slot >= SLOT_CRAFTING_BASE)
	{
		handleRegularSlotClick(craftingSlots[slot - SLOT_CRAFTING_BASE], buttonNum);
		updateCraftingResult();
		return;
	}

	if (slot >= 0 && slot < 36)
		handleRegularSlotClick(minecraft.player->inventory.mainInventory[slot], buttonNum);
}

jstring InventoryScreen::getBackgroundTexture() const
{
	return u"/gui/inventory.png";
}

int_t InventoryScreen::getCraftingGridLeft() const
{
	return 88;
}

int_t InventoryScreen::getCraftingGridTop() const
{
	return 26;
}

int_t InventoryScreen::getResultSlotX() const
{
	return 144;
}

int_t InventoryScreen::getResultSlotY() const
{
	return 36;
}

int_t InventoryScreen::getTitleX() const
{
	return 86;
}

int_t InventoryScreen::getTitleY() const
{
	return 16;
}

jstring InventoryScreen::getTitleText() const
{
	return u"Crafting";
}

bool InventoryScreen::shouldRenderPlayerModel() const
{
	return true;
}
