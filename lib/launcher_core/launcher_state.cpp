#include "launcher_state.h"
#include <Arduino.h>

void launcherState_init(LauncherRuntimeState& s) {
    s.state             = LauncherSafetyState::DISARMED;
    s.lastEvent         = LauncherEvent::READY;
    s.faultCode         = FaultCode::NONE;
    s.continuity        = ContinuityState::UNKNOWN;
    s.keySwitchOn       = false;
    s.ignitionActive    = false;
    s.batteryPct        = 100;
    s.lastCommandRxMs   = 0;
    s.lastStatusTxMs    = 0;
    s.lastRxSeq         = 0;
    s.txSeq             = 0;
    s.bootSessionId     = esp_random();
}

void launcherState_tick(LauncherRuntimeState& s, uint32_t now) {
    // If we're in FIRED state, return to DISARMED after cooldown
    if (s.state == LauncherSafetyState::FIRED) {
        if (now - s.lastCommandRxMs >= POST_FIRE_COOLDOWN_MS) {
            s.state     = LauncherSafetyState::DISARMED;
            s.lastEvent = LauncherEvent::DISARMED_OK;
        }
    }

    // Hard interlock: if key switch goes off while armed, force disarm
    if (!s.keySwitchOn &&
        (s.state == LauncherSafetyState::ARMED ||
         s.state == LauncherSafetyState::FIRING)) {
        s.state     = LauncherSafetyState::DISARMED;
        s.lastEvent = LauncherEvent::DISARMED_OK;
        s.faultCode = FaultCode::INTERLOCK_OFF;
    }
}

void launcherState_onArm(LauncherRuntimeState& s, bool arm) {
    if (arm) {
        if (!s.keySwitchOn) {
            s.faultCode = FaultCode::INTERLOCK_OFF;
            s.lastEvent = LauncherEvent::INTERLOCK_BLOCKED;
            return;
        }
        if (s.state == LauncherSafetyState::DISARMED) {
            s.state     = LauncherSafetyState::ARMED;
            s.lastEvent = LauncherEvent::ARMED_OK;
            s.faultCode = FaultCode::NONE;
        }
    } else {
        s.state     = LauncherSafetyState::DISARMED;
        s.lastEvent = LauncherEvent::DISARMED_OK;
    }
}

void launcherState_onFireCmd(LauncherRuntimeState& s, uint32_t requestToken) {
    (void)requestToken;

    if (s.state != LauncherSafetyState::ARMED) {
        s.faultCode = FaultCode::FIRE_WHILE_DISARMED;
        s.lastEvent = LauncherEvent::FAULT_GENERIC;
        s.state     = LauncherSafetyState::FAULT;
        return;
    }

    if (!s.keySwitchOn) {
        s.faultCode = FaultCode::INTERLOCK_OFF;
        s.lastEvent = LauncherEvent::INTERLOCK_BLOCKED;
        s.state     = LauncherSafetyState::FAULT;
        return;
    }

    if (s.continuity != ContinuityState::PRESENT) {
        s.faultCode = (s.continuity == ContinuityState::SHORT_FAULT)
                        ? FaultCode::CONTINUITY_SHORT
                        : FaultCode::CONTINUITY_OPEN;
        s.lastEvent = LauncherEvent::CONTINUITY_FAIL;
        s.state     = LauncherSafetyState::FAULT;
        return;
    }

    // All checks passed — enter firing state
    s.state         = LauncherSafetyState::FIRING;
    s.lastEvent     = LauncherEvent::FIRE_SENT;
    s.faultCode     = FaultCode::NONE;
    s.ignitionActive = true;
}

void launcherState_onInterlockChanged(LauncherRuntimeState& s, bool on) {
    s.keySwitchOn = on;
    if (!on && (s.state == LauncherSafetyState::ARMED ||
                s.state == LauncherSafetyState::FIRING)) {
        s.state     = LauncherSafetyState::DISARMED;
        s.lastEvent = LauncherEvent::DISARMED_OK;
        s.faultCode = FaultCode::INTERLOCK_OFF;
    }
}

void launcherState_onContinuityChanged(LauncherRuntimeState& s, ContinuityState cs) {
    s.continuity = cs;
}

void launcherState_onIgnitionComplete(LauncherRuntimeState& s) {
    s.ignitionActive = false;
    s.state          = LauncherSafetyState::FIRED;
    s.lastEvent      = LauncherEvent::FIRED_OK;
    s.lastCommandRxMs = millis(); // reset for cooldown timer
}

bool launcherState_canArm(const LauncherRuntimeState& s) {
    return s.keySwitchOn && s.state == LauncherSafetyState::DISARMED;
}

bool launcherState_canFire(const LauncherRuntimeState& s) {
    return s.state == LauncherSafetyState::ARMED &&
           s.keySwitchOn &&
           s.continuity == ContinuityState::PRESENT;
}
