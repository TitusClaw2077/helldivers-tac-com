#pragma once
#include <stdint.h>
#include <stddef.h>
#include "protocol_types.h"
#include "config_shared.h"

// ─── Packet header (all packets) ─────────────────────────────────────────────
#pragma pack(push, 1)

struct PacketHeader {
    uint8_t  magic;         // must be PACKET_MAGIC (0xA7)
    uint8_t  version;       // PROTOCOL_VERSION
    uint8_t  msgType;       // MessageType enum value
    uint8_t  flags;         // reserved
    uint16_t seq;           // sender sequence number
    uint16_t payloadLen;    // bytes following header
    uint32_t sessionId;     // random nonce assigned at boot
};

// ─── Payload structs ─────────────────────────────────────────────────────────

struct HeartbeatPayload {
    uint32_t wristUptimeMs;
    uint8_t  wristBatteryPct;
    uint8_t  uiScreen;
    uint8_t  inputState;        // StratagemInputState
    uint8_t  reserved;
};

struct StatusPayload {
    uint8_t  launcherState;     // LauncherSafetyState
    uint8_t  continuityState;   // ContinuityState
    uint8_t  lastEvent;         // LauncherEvent
    uint8_t  faultCode;         // FaultCode
    uint8_t  keySwitchOn;       // 0/1
    uint8_t  canArm;            // 0/1
    uint8_t  canFire;           // 0/1
    uint8_t  batteryPct;
    int8_t   linkQuality;
    uint8_t  reserved[3];
    uint32_t launcherUptimeMs;
};

struct ArmSetPayload {
    uint8_t  arm;               // 1 = arm, 0 = disarm
    uint8_t  requestedByUi;     // always 1
    uint16_t reserved;
    uint32_t requestToken;
};

struct FireCmdPayload {
    uint8_t  stratagemId;
    uint8_t  inputLength;
    uint8_t  reserved[2];
    uint32_t requestToken;
    uint32_t matchedAtMs;
};

struct FireAckPayload {
    uint32_t requestToken;
    uint8_t  accepted;          // 1 = yes
    uint8_t  launcherState;     // post-processing LauncherSafetyState
    uint8_t  lastEvent;
    uint8_t  faultCode;
    uint32_t firedAtMs;
};

// ─── Full packet wrappers ─────────────────────────────────────────────────────

struct HeartbeatPacket {
    PacketHeader    header;
    HeartbeatPayload payload;
    uint32_t        crc32;
};

struct StatusPacket {
    PacketHeader    header;
    StatusPayload   payload;
    uint32_t        crc32;
};

struct ArmSetPacket {
    PacketHeader    header;
    ArmSetPayload   payload;
    uint32_t        crc32;
};

struct FireCmdPacket {
    PacketHeader    header;
    FireCmdPayload  payload;
    uint32_t        crc32;
};

struct FireAckPacket {
    PacketHeader    header;
    FireAckPayload  payload;
    uint32_t        crc32;
};

#pragma pack(pop)

// ─── Validation helpers (implemented in lib/common/crc32.cpp) ─────────────────
uint32_t taccom_crc32(const uint8_t* data, size_t len);
bool     validatePacket(const uint8_t* data, int len, MessageType expectedType);
