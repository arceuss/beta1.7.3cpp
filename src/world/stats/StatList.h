#pragma once

#include <unordered_map>
#include <vector>

#include "java/Type.h"

class StatBase;

class StatList
{
private:
	static std::unordered_map<int_t, StatBase *> statsById;

public:
	static std::vector<StatBase *> allStats;

	static void registerStat(StatBase &stat);
	static StatBase *getStat(int_t id);
};
