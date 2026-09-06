#include "System.h"

#include <chrono>
#include <atomic>

namespace System
{

static std::atomic<long_t> simulationTimeMillis{-1};

void setSimulationTimeMillis(long_t millis)
{
	simulationTimeMillis.store(millis, std::memory_order_relaxed);
}

long_t currentTimeMillis()
{
	const long_t simulated = simulationTimeMillis.load(std::memory_order_relaxed);
	if (simulated >= 0)
		return simulated;
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

long_t nanoTime()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}