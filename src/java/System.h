#pragma once

#include "Type.h"

namespace System
{

long_t currentTimeMillis();
long_t nanoTime();
// Negative values restore the real clock; only the deterministic stress tool sets this.
void setSimulationTimeMillis(long_t millis);

}
