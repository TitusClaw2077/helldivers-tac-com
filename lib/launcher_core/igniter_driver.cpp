#include "igniter_driver.h"
#include <Arduino.h>

void (*igniter_onComplete)() = nullptr;

static uint8_t  _pin           = 0;
static bool     _active        = false;
static uint32_t _pulseStartMs  = 0;
static uint32_t _pulseDurMs    = IGNITION_PULSE_DURATION_MS;

void igniter_init(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    _active = false;
}

void igniter_startPulse(uint32_t durationMs) {
    if (_active) return;            // already firing — ignore duplicate
    _pulseDurMs   = durationMs;
    _pulseStartMs = millis();
    _active       = true;
    digitalWrite(_pin, HIGH);
}

void igniter_service(uint32_t now) {
    if (!_active) return;
    if (now - _pulseStartMs >= _pulseDurMs) {
        igniter_forceOff();
        if (igniter_onComplete) igniter_onComplete();
    }
}

bool igniter_isActive() {
    return _active;
}

void igniter_forceOff() {
    digitalWrite(_pin, LOW);
    _active = false;
}
