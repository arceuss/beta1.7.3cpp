#include "world/stats/StatList.h"

#include <stdexcept>

#include "java/String.h"
#include "world/stats/StatBase.h"

std::unordered_map<int_t, StatBase *> StatList::statsById;
std::vector<StatBase *> StatList::allStats;

void StatList::registerStat(StatBase &stat)
{
	auto it = statsById.find(stat.statId);
	if (it != statsById.end())
	{
		throw std::runtime_error(
			"Duplicate stat id: \"" + String::toUTF8(it->second->statName) + "\" and \"" +
			String::toUTF8(stat.statName) + "\" at id " + std::to_string(stat.statId));
	}

	allStats.push_back(&stat);
	statsById.emplace(stat.statId, &stat);
}

StatBase *StatList::getStat(int_t id)
{
	auto it = statsById.find(id);
	return it == statsById.end() ? nullptr : it->second;
}
