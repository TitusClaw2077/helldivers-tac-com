#pragma once
#include <stdint.h>

void igniter_init(uint8_t pin);
void igniter_startPulse(uint32_t durationMs);
void igniter_service(uint32_t now);         // call each loop()
bool igniter_isActive();
void igniter_forceOff();                    // emergency stop — always safe to call

// Callback — set by launcher_state before firing
extern void (*igniter_onComplete)();
