#pragma once
#include <stdint.h>

// ─── Directional input ───────────────────────────────────────────────────────
enum class Direction : uint8_t {
    UP    = 0,
    DOWN  = 1,
    LEFT  = 2,
    RIGHT = 3
};

// ─── Message types ───────────────────────────────────────────────────────────
enum class MessageType : uint8_t {
    HEARTBEAT   = 1,
    STATUS      = 2,
    ARM_SET     = 3,
    FIRE_CMD    = 4,
    FIRE_ACK    = 5
};

// ─── Launcher safety states ──────────────────────────────────────────────────
enum class LauncherSafetyState : uint8_t {
    BOOTING     = 0,
    DISARMED    = 1,
    ARMED       = 2,
    FIRING      = 3,
    FIRED       = 4,
    FAULT       = 5
};

// ─── Launcher events ─────────────────────────────────────────────────────────
enum class LauncherEvent : uint8_t {
    NONE                = 0,
    READY               = 1,
    ARMED_OK            = 2,
    DISARMED_OK         = 3,
    FIRE_SENT           = 4,
    FIRED_OK            = 5,
    CONTINUITY_FAIL     = 6,
    INTERLOCK_BLOCKED   = 7,
    COMMS_LOST          = 8,
    FAULT_GENERIC       = 9
};

// ─── Fault codes ─────────────────────────────────────────────────────────────
enum class FaultCode : uint8_t {
    NONE                = 0,
    INVALID_MAC         = 1,
    BAD_PACKET          = 2,
    INTERLOCK_OFF       = 3,
    CONTINUITY_OPEN     = 4,
    CONTINUITY_SHORT    = 5,
    FIRE_WHILE_DISARMED = 6,
    FIRE_TIMEOUT        = 7,
    INTERNAL_ERROR      = 8
};

// ─── Continuity states ───────────────────────────────────────────────────────
enum class ContinuityState : uint8_t {
    UNKNOWN     = 0,
    OPEN        = 1,
    PRESENT     = 2,
    SHORT_FAULT = 3
};

// ─── Stratagem input states (wrist) ──────────────────────────────────────────
enum class StratagemInputState : uint8_t {
    IDLE        = 0,
    INPUTTING   = 1,
    MATCHED     = 2,
    CONFIRMING  = 3,
    FIRING      = 4
};

// ─── Wrist power modes ───────────────────────────────────────────────────────
enum class WristPowerMode : uint8_t {
    ACTIVE          = 0,
    DIMMED          = 1,
    IDLE            = 2,
    SLEEP_PENDING   = 3,
    DEEP_SLEEP      = 4
};
