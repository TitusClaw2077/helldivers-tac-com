#pragma once

#include <stdint.h>

void output_indicator_init(uint8_t pin);
void output_indicator_startPulse(uint32_t durationMs);
void output_indicator_service(uint32_t now);
bool output_indicator_isActive();
void output_indicator_forceOff();

extern void (*output_indicator_onComplete)();
