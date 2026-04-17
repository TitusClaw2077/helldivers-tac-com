#include "output_indicator.h"

#include <Arduino.h>

void (*output_indicator_onComplete)() = nullptr;

static uint8_t s_pin = 0;
static bool s_active = false;
static uint32_t s_pulseStartMs = 0;
static uint32_t s_pulseDurMs = 0;

void output_indicator_init(uint8_t pin) {
    s_pin = pin;
    pinMode(s_pin, OUTPUT);
    digitalWrite(s_pin, LOW);
    s_active = false;
}

void output_indicator_startPulse(uint32_t durationMs) {
    if (s_active) return;
    s_pulseDurMs = durationMs;
    s_pulseStartMs = millis();
    s_active = true;
    digitalWrite(s_pin, HIGH);
}

void output_indicator_service(uint32_t now) {
    if (!s_active) return;
    if (now - s_pulseStartMs >= s_pulseDurMs) {
        output_indicator_forceOff();
        if (output_indicator_onComplete) output_indicator_onComplete();
    }
}

bool output_indicator_isActive() {
    return s_active;
}

void output_indicator_forceOff() {
    digitalWrite(s_pin, LOW);
    s_active = false;
}
