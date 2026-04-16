#pragma once
#include "protocol_types.h"
#include "protocol.h"
#include <stdint.h>

// ─── Launcher link state (wrist side) ────────────────────────────────────────
// Reflects the last known state of the remote launcher unit.
struct LauncherLinkState {
    bool    peerConfigured;
    bool    online;
    bool    armed;
    bool    continuityOk;
    ContinuityState continuityState;
    bool    keySwitchOn;
    bool    firePermitted;
    uint8_t batteryPct;
    int8_t  linkQuality;        // from STATUS.linkQuality field

    uint32_t lastHeartbeatSentMs;
    uint32_t lastStatusRxMs;
    uint32_t lastAckRxMs;

    LauncherSafetyState remoteState;
    LauncherEvent       lastEvent;
    FaultCode           lastFaultCode;

    uint16_t txSeq;             // outgoing sequence counter
    uint16_t lastRxSeq;         // last received sequence

    // Last FIRE_ACK fields
    bool     lastAckAccepted;
    uint32_t lastAckToken;
};

// ─── API ─────────────────────────────────────────────────────────────────────

// Call once in setup() — registers peer, registers recv callback
void launcher_link_init(LauncherLinkState& state);

// Call every loop() — drives heartbeat timer and timeout detection
void launcher_link_tick(LauncherLinkState& state, uint32_t now);

// Send an ARM or DISARM command
void launcher_link_sendArmSet(LauncherLinkState& state, bool arm);

// Send a FIRE_CMD
void launcher_link_sendFireCmd(LauncherLinkState& state,
                               uint8_t stratagemId,
                               uint8_t inputLength,
                               uint32_t requestToken,
                               uint32_t matchedAtMs);
