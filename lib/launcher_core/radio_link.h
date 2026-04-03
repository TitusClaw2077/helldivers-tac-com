#pragma once
#include "protocol_types.h"
#include "protocol.h"
#include "launcher_state.h"
#include <stdint.h>

// ─── Pending command structs ──────────────────────────────────────────────────

struct PendingArmCmd {
    bool    valid;
    bool    arm;
    uint16_t seq;
    uint32_t token;
    uint32_t receivedAtMs;
};

struct PendingFireCmd {
    bool     valid;
    uint8_t  stratagemId;
    uint8_t  inputLength;
    uint16_t seq;
    uint32_t requestToken;
    uint32_t matchedAtMs;
    uint32_t receivedAtMs;
};

// ─── API ─────────────────────────────────────────────────────────────────────

// Call once in setup()
void radio_link_init();

// Call every loop() — no work right now, reserved for future use
void radio_link_tick(uint32_t now);

// Send current launcher state as a STATUS packet back to wrist
void radio_link_sendStatus(const LauncherRuntimeState& ls);

// Send FIRE_ACK back to wrist — call after fire command is processed
void radio_link_sendFireAck(const LauncherRuntimeState& ls,
                             uint32_t requestToken,
                             bool accepted,
                             uint32_t firedAtMs);

// ─── Pending command accessors ────────────────────────────────────────────────
bool radio_link_hasPendingArm();
bool radio_link_getPendingArm();       // returns arm flag (true=arm, false=disarm)
void radio_link_consumePendingArm();   // clear after processing

bool radio_link_hasPendingFire();
PendingFireCmd radio_link_consumePendingFire();  // returns copy then clears
