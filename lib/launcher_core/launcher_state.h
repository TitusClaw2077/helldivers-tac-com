#pragma once
#include "protocol_types.h"
#include "config_shared.h"
#include <stdint.h>

struct LauncherRuntimeState {
    LauncherSafetyState state;
    LauncherEvent       lastEvent;
    FaultCode           faultCode;
    ContinuityState     continuity;
    bool                keySwitchOn;
    bool                ignitionActive;
    uint8_t             batteryPct;
    uint32_t            lastCommandRxMs;
    uint32_t            lastStatusTxMs;
    uint16_t            lastRxSeq;
    uint16_t            txSeq;
    uint32_t            bootSessionId;
};

void    launcherState_init(LauncherRuntimeState& s);
void    launcherState_tick(LauncherRuntimeState& s, uint32_t now);

// Called when commands are received
void    launcherState_onArm(LauncherRuntimeState& s, bool arm);
void    launcherState_onFireCmd(LauncherRuntimeState& s, uint32_t requestToken);

// Called by hardware drivers
void    launcherState_onInterlockChanged(LauncherRuntimeState& s, bool on);
void    launcherState_onContinuityChanged(LauncherRuntimeState& s, ContinuityState cs);
void    launcherState_onIgnitionComplete(LauncherRuntimeState& s);

bool    launcherState_canArm(const LauncherRuntimeState& s);
bool    launcherState_canFire(const LauncherRuntimeState& s);
