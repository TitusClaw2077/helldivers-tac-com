#pragma once
#include "protocol_types.h"
#include <stdint.h>

struct ContinuityDebugInfo {
    uint16_t rawAverage;
    uint32_t lastSampleMs;
};

// Call once during launcher setup.
void continuity_init();

// Sample/classify continuity state at the configured interval.
void continuity_tick(uint32_t now);

// Latest classified continuity state.
ContinuityState continuity_getState();

// Latest averaged ADC reading and sample timestamp for debug/status prints.
ContinuityDebugInfo continuity_getDebugInfo();

