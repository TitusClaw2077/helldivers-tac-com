#include "radio_link.h"
#include "config_shared.h"
#include "protocol.h"
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <string.h>

// ─── Module-level statics ─────────────────────────────────────────────────────
static const uint8_t k_wristMac[6] = WRIST_MAC;

// Outgoing sequence counter for STATUS / FIRE_ACK
static uint16_t s_txSeq = 0;

// Pending commands — set in receive callback, consumed in loop()
// Simple flag-based queue (one pending of each type at a time).
// If a second command arrives before the first is consumed, it overwrites.
// At 2-second heartbeat rate this is safe in normal operation.
static volatile PendingArmCmd  s_pendingArm  = {};
static volatile PendingFireCmd s_pendingFire = {};

// ─── Internal helpers ─────────────────────────────────────────────────────────

static void fillHeader(PacketHeader& hdr,
                        MessageType type,
                        uint16_t payloadLen)
{
    hdr.magic      = PACKET_MAGIC;
    hdr.version    = PROTOCOL_VERSION;
    hdr.msgType    = (uint8_t)type;
    hdr.flags      = 0;
    hdr.seq        = ++s_txSeq;
    hdr.payloadLen = payloadLen;
    hdr.sessionId  = 0;
}

// ─── ESP-NOW receive callback ─────────────────────────────────────────────────
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    // Validate sender MAC
    if (memcmp(mac, k_wristMac, 6) != 0) {
        Serial.println("[LAUNCHER/radio] Recv from unknown MAC — rejected");
        return;
    }

    // Minimum size check (header + CRC)
    if (len < (int)sizeof(PacketHeader) + 4) return;

    const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(data);

    // Magic and version
    if (hdr->magic   != PACKET_MAGIC)     return;
    if (hdr->version != PROTOCOL_VERSION) return;

    // CRC check
    size_t crcOffset  = (size_t)len - 4;
    uint32_t stored   = *reinterpret_cast<const uint32_t*>(data + crcOffset);
    uint32_t computed = taccom_crc32(data, crcOffset);
    if (stored != computed) {
        Serial.println("[LAUNCHER/radio] CRC mismatch — packet dropped");
        return;
    }

    uint32_t now = millis();
    MessageType mt = (MessageType)hdr->msgType;

    if (mt == MessageType::HEARTBEAT) {
        if (len != (int)sizeof(HeartbeatPacket)) return;
        // Heartbeat is handled entirely by the loop (triggers periodic status send).
        // We don't need to store it; the status broadcast is triggered by a timer
        // in main.cpp after any receive activity.  Just log it.
        Serial.printf("[LAUNCHER/radio] HEARTBEAT rx seq=%u\n", hdr->seq);

        // Signal that a heartbeat was received by setting a flag in the ARM pending
        // struct with a special sentinel — or just nudge lastCommandRxMs indirectly.
        // The simplest approach: use pendingArm.receivedAtMs as a "last hb" timestamp.
        // However we don't want to clobber a real arm command.  Use a separate field.
        // For now we rely on main.cpp calling radio_link_sendStatus() periodically;
        // the heartbeat just wakes the wrist's timeout clock.
        (void)now;
    }
    else if (mt == MessageType::ARM_SET) {
        if (len != (int)sizeof(ArmSetPacket)) return;
        const ArmSetPacket* pkt = reinterpret_cast<const ArmSetPacket*>(data);
        // Overwrite any unprocessed arm command
        s_pendingArm.valid        = true;
        s_pendingArm.arm          = (pkt->payload.arm != 0);
        s_pendingArm.seq          = hdr->seq;
        s_pendingArm.token        = pkt->payload.requestToken;
        s_pendingArm.receivedAtMs = now;
        Serial.printf("[LAUNCHER/radio] ARM_SET rx arm=%d seq=%u\n",
                      pkt->payload.arm, hdr->seq);
    }
    else if (mt == MessageType::FIRE_CMD) {
        if (len != (int)sizeof(FireCmdPacket)) return;
        const FireCmdPacket* pkt = reinterpret_cast<const FireCmdPacket*>(data);
        s_pendingFire.valid        = true;
        s_pendingFire.stratagemId  = pkt->payload.stratagemId;
        s_pendingFire.inputLength  = pkt->payload.inputLength;
        s_pendingFire.seq          = hdr->seq;
        s_pendingFire.requestToken = pkt->payload.requestToken;
        s_pendingFire.matchedAtMs  = pkt->payload.matchedAtMs;
        s_pendingFire.receivedAtMs = now;
        Serial.printf("[LAUNCHER/radio] FIRE_CMD rx id=%u token=%lu seq=%u\n",
                      pkt->payload.stratagemId,
                      (unsigned long)pkt->payload.requestToken,
                      hdr->seq);
    }
    else {
        Serial.printf("[LAUNCHER/radio] Unknown msgType=%u — ignored\n", hdr->msgType);
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void radio_link_init() {
    // Register ESP-NOW receive callback
    esp_now_register_recv_cb(onRecv);

    // Add wrist as peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, k_wristMac, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
        Serial.printf("[LAUNCHER/radio] Wrist peer registered: "
                      "%02X:%02X:%02X:%02X:%02X:%02X\n",
                      k_wristMac[0], k_wristMac[1], k_wristMac[2],
                      k_wristMac[3], k_wristMac[4], k_wristMac[5]);
    } else {
        Serial.println("[LAUNCHER/radio] Failed to add wrist peer");
    }

    s_txSeq = 0;
    memset((void*)&s_pendingArm,  0, sizeof(s_pendingArm));
    memset((void*)&s_pendingFire, 0, sizeof(s_pendingFire));
}

void radio_link_tick(uint32_t now) {
    // Reserved for future queued-send or retry logic
    (void)now;
}

void radio_link_sendStatus(const LauncherRuntimeState& ls) {
    StatusPacket pkt = {};
    fillHeader(pkt.header, MessageType::STATUS, sizeof(StatusPayload));

    pkt.payload.launcherState    = (uint8_t)ls.state;
    pkt.payload.continuityState  = (uint8_t)ls.continuity;
    pkt.payload.lastEvent        = (uint8_t)ls.lastEvent;
    pkt.payload.faultCode        = (uint8_t)ls.faultCode;
    pkt.payload.keySwitchOn      = ls.keySwitchOn ? 1 : 0;
    pkt.payload.canArm           = launcherState_canArm(ls) ? 1 : 0;
    pkt.payload.canFire          = launcherState_canFire(ls) ? 1 : 0;
    pkt.payload.batteryPct       = ls.batteryPct;
    pkt.payload.linkQuality      = 0;   // placeholder — no RSSI available on ESP-NOW sender side
    pkt.payload.launcherUptimeMs = millis();
    pkt.crc32 = taccom_crc32(reinterpret_cast<const uint8_t*>(&pkt),
                              sizeof(pkt) - sizeof(uint32_t));

    esp_err_t err = esp_now_send(k_wristMac,
                                 reinterpret_cast<const uint8_t*>(&pkt),
                                 sizeof(pkt));
    if (err != ESP_OK) {
        Serial.printf("[LAUNCHER/radio] STATUS send failed: %d\n", err);
    }
}

void radio_link_sendFireAck(const LauncherRuntimeState& ls,
                             uint32_t requestToken,
                             bool accepted,
                             uint32_t firedAtMs)
{
    FireAckPacket pkt = {};
    fillHeader(pkt.header, MessageType::FIRE_ACK, sizeof(FireAckPayload));

    pkt.payload.requestToken  = requestToken;
    pkt.payload.accepted      = accepted ? 1 : 0;
    pkt.payload.launcherState = (uint8_t)ls.state;
    pkt.payload.lastEvent     = (uint8_t)ls.lastEvent;
    pkt.payload.faultCode     = (uint8_t)ls.faultCode;
    pkt.payload.firedAtMs     = firedAtMs;
    pkt.crc32 = taccom_crc32(reinterpret_cast<const uint8_t*>(&pkt),
                              sizeof(pkt) - sizeof(uint32_t));

    esp_err_t err = esp_now_send(k_wristMac,
                                 reinterpret_cast<const uint8_t*>(&pkt),
                                 sizeof(pkt));
    Serial.printf("[LAUNCHER/radio] FIRE_ACK sent accepted=%d token=%lu err=%d\n",
                  accepted, (unsigned long)requestToken, err);
}

// ─── Pending command accessors ────────────────────────────────────────────────

bool radio_link_hasPendingArm() {
    return s_pendingArm.valid;
}

bool radio_link_getPendingArm() {
    return s_pendingArm.arm;
}

void radio_link_consumePendingArm() {
    s_pendingArm.valid = false;
}

bool radio_link_hasPendingFire() {
    return s_pendingFire.valid;
}

PendingFireCmd radio_link_consumePendingFire() {
    PendingFireCmd copy;
    memcpy(&copy, (void*)&s_pendingFire, sizeof(copy));
    s_pendingFire.valid = false;
    return copy;
}
