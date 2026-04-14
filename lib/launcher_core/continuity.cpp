#include "continuity.h"
#include "config_shared.h"
#include <Arduino.h>

namespace {

// These are intentionally conservative first-pass thresholds.
// We expect to tune them once we have real bench ADC data.
constexpr uint16_t kShortThreshold = 80;
constexpr uint16_t kPresentThreshold = 1800;
constexpr uint8_t  kSamplesPerRead = 8;

ContinuityState s_state = ContinuityState::UNKNOWN;
uint16_t s_rawAverage = 0;
uint32_t s_lastSampleMs = 0;

const char* continuityStateName(ContinuityState state) {
    switch (state) {
        case ContinuityState::UNKNOWN:     return "UNKNOWN";
        case ContinuityState::OPEN:        return "OPEN";
        case ContinuityState::PRESENT:     return "PRESENT";
        case ContinuityState::SHORT_FAULT: return "SHORT_FAULT";
        default:                           return "INVALID";
    }
}

ContinuityState classifyReading(uint16_t rawAverage) {
    if (rawAverage <= kShortThreshold) {
        return ContinuityState::SHORT_FAULT;
    }
    if (rawAverage <= kPresentThreshold) {
        return ContinuityState::PRESENT;
    }
    return ContinuityState::OPEN;
}

uint16_t sampleAverage() {
    uint32_t total = 0;
    for (uint8_t i = 0; i < kSamplesPerRead; ++i) {
        total += analogRead(PIN_CONTINUITY_ADC);
    }
    return static_cast<uint16_t>(total / kSamplesPerRead);
}

} // namespace

void continuity_init() {
    pinMode(PIN_CONTINUITY_ADC, INPUT);
    s_state = ContinuityState::UNKNOWN;
    s_rawAverage = 0;
    s_lastSampleMs = 0;
    Serial.printf("[LAUNCHER/cont] Init ADC pin=%d\n", PIN_CONTINUITY_ADC);
}

void continuity_tick(uint32_t now) {
    if (s_lastSampleMs != 0 && (now - s_lastSampleMs) < CONTINUITY_CHECK_INTERVAL_MS) {
        return;
    }

    s_lastSampleMs = now;
    uint16_t raw = sampleAverage();
    ContinuityState next = classifyReading(raw);

    bool changed = (next != s_state);
    bool firstRead = (s_state == ContinuityState::UNKNOWN && s_rawAverage == 0);
    s_rawAverage = raw;

    if (changed || firstRead) {
        Serial.printf("[LAUNCHER/cont] raw=%u state=%s\n",
                      static_cast<unsigned>(s_rawAverage),
                      continuityStateName(next));
    }

    s_state = next;
}

ContinuityState continuity_getState() {
    return s_state;
}

ContinuityDebugInfo continuity_getDebugInfo() {
    return ContinuityDebugInfo {
        s_rawAverage,
        s_lastSampleMs,
    };
}

