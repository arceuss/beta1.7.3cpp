#pragma once

#include <map>
#include <vector>
#include "java/String.h"
#include "java/Type.h"

class Minecraft;

class SPCCommand
{
public:
	struct ChatMessage
	{
		jstring text;
		int_t tickCreated;
	};

	static std::vector<ChatMessage> messages;

	static void execute(Minecraft &mc, const jstring &input);
	static void addMessage(const jstring &text);
	static void sendError(const jstring &text);

	static constexpr int_t MESSAGE_DISPLAY_TICKS = 200;
	static int_t guiTickCount;

	// SPC state
	static bool flying;
	static bool noclip;
	static bool damageEnabled;
	static bool fallDamage;
	static bool fireDamage;
	static bool waterDamage;
	static bool instantMine;
	static bool instantKill;
	static bool freezeMobs;
	static bool infiniteItems;
	static bool keepItems;
	static bool blockDrops;
	static bool itemDamage;
	static bool mobDamage;
	static bool waterMovement;
	static bool outputEnabled;
	static bool longerLegs;
	static double speed;
	static double gravity;
	static double reachDistance;
	static double superPunch;
	static float flySpeed;
	static bool monstersSpawn;
	static bool animalSpawn;

	// Waypoints
	struct Waypoint { double x, y, z; };
	static std::map<jstring, Waypoint> waypoints;

	// Return position
	static double prevX, prevY, prevZ;
	static bool hasPrevPos;

	// Last command for /repeat
	static jstring lastCommand;

private:
	static void setCurrentPos(Minecraft &mc);
};