#include "launcher_link.h"
#include "config_shared.h"
#include "protocol.h"
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <string.h>

// ─── Module-level statics ─────────────────────────────────────────────────────
// Pointer to the active state struct — set during init so the recv callback
// can update it without passing user data through the ESP-NOW API.
static LauncherLinkState* s_link = nullptr;

static const uint8_t k_launcherMac[6] = LAUNCHER_MAC;

// ─── Internal helpers ─────────────────────────────────────────────────────────

static void fillHeader(PacketHeader& hdr,
                        MessageType type,
                        uint16_t payloadLen,
                        uint16_t& txSeq)
{
    hdr.magic      = PACKET_MAGIC;
    hdr.version    = PROTOCOL_VERSION;
    hdr.msgType    = (uint8_t)type;
    hdr.flags      = 0;
    hdr.seq        = ++txSeq;
    hdr.payloadLen = payloadLen;
    hdr.sessionId  = 0;   // wrist doesn't use sessionId for now
}

static void applyStatusPayload(LauncherLinkState& ls, const StatusPayload& p, uint32_t now) {
    ls.online         = true;
    ls.lastStatusRxMs = now;
    ls.remoteState    = (LauncherSafetyState)p.launcherState;
    ls.lastEvent      = (LauncherEvent)p.lastEvent;
    ls.lastFaultCode  = (FaultCode)p.faultCode;
    ls.continuityOk   = (p.continuityState == (uint8_t)ContinuityState::PRESENT);
    ls.keySwitchOn    = (p.keySwitchOn != 0);
    ls.firePermitted  = (p.canFire != 0);
    ls.batteryPct     = p.batteryPct;
    ls.linkQuality    = p.linkQuality;
    ls.armed          = (ls.remoteState == LauncherSafetyState::ARMED);
}

// ─── ESP-NOW receive callback (called from ISR/radio task context) ────────────
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (!s_link) return;

    // Validate sender is the known launcher
    if (memcmp(mac, k_launcherMac, 6) != 0) {
        Serial.println("[WRIST/link] Recv from unknown MAC — ignored");
        return;
    }

    if (len < (int)sizeof(PacketHeader) + 4) return;

    const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic   != PACKET_MAGIC)     return;
    if (hdr->version != PROTOCOL_VERSION) return;

    // CRC check: covers everything except final 4 bytes
    size_t crcOffset  = (size_t)len - 4;
    uint32_t stored   = *reinterpret_cast<const uint32_t*>(data + crcOffset);
    uint32_t computed = taccom_crc32(data, crcOffset);
    if (stored != computed) {
        Serial.println("[WRIST/link] CRC mismatch — packet dropped");
        return;
    }

    s_link->lastRxSeq = hdr->seq;
    uint32_t now = millis();

    MessageType mt = (MessageType)hdr->msgType;

    if (mt == MessageType::STATUS) {
        if (len != (int)sizeof(StatusPacket)) return;
        const StatusPacket* pkt = reinterpret_cast<const StatusPacket*>(data);
        applyStatusPayload(*s_link, pkt->payload, now);
        Serial.printf("[WRIST/link] STATUS rx — state=%u armed=%d cont=%d key=%d\n",
                      pkt->payload.launcherState,
                      s_link->armed,
                      s_link->continuityOk,
                      s_link->keySwitchOn);
    }
    else if (mt == MessageType::FIRE_ACK) {
        if (len != (int)sizeof(FireAckPacket)) return;
        const FireAckPacket* pkt = reinterpret_cast<const FireAckPacket*>(data);
        s_link->lastAckRxMs    = now;
        s_link->lastAckAccepted = (pkt->payload.accepted != 0);
        s_link->lastAckToken   = pkt->payload.requestToken;
        Serial.printf("[WRIST/link] FIRE_ACK rx — accepted=%d token=%lu\n",
                      pkt->payload.accepted, (unsigned long)pkt->payload.requestToken);
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void launcher_link_init(LauncherLinkState& state) {
    memset(&state, 0, sizeof(state));
    state.remoteState = LauncherSafetyState::BOOTING;
    state.lastEvent   = LauncherEvent::NONE;

    s_link = &state;

    // Register ESP-NOW receive callback
    esp_now_register_recv_cb(onRecv);

    // Add launcher as peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, k_launcherMac, 6);
    peer.channel = 0;    // use current channel
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
        state.peerConfigured = true;
        Serial.printf("[WRIST/link] Launcher peer registered: "
                      "%02X:%02X:%02X:%02X:%02X:%02X\n",
                      k_launcherMac[0], k_launcherMac[1], k_launcherMac[2],
                      k_launcherMac[3], k_launcherMac[4], k_launcherMac[5]);
    } else {
        Serial.println("[WRIST/link] Failed to add launcher peer");
    }
}

void launcher_link_tick(LauncherLinkState& state, uint32_t now) {
    // Heartbeat timer
    if (now - state.lastHeartbeatSentMs >= HEARTBEAT_INTERVAL_MS) {
        state.lastHeartbeatSentMs = now;

        HeartbeatPacket pkt = {};
        fillHeader(pkt.header, MessageType::HEARTBEAT,
                   sizeof(HeartbeatPayload), state.txSeq);
        pkt.payload.wristUptimeMs    = now;
        pkt.payload.wristBatteryPct  = 100;  // placeholder — wire up BatteryMonitor later
        pkt.payload.uiScreen         = 0;
        pkt.payload.inputState       = 0;
        pkt.crc32 = taccom_crc32(reinterpret_cast<const uint8_t*>(&pkt),
                                  sizeof(pkt) - sizeof(uint32_t));

        esp_err_t err = esp_now_send(k_launcherMac,
                                     reinterpret_cast<const uint8_t*>(&pkt),
                                     sizeof(pkt));
        if (err != ESP_OK) {
            Serial.printf("[WRIST/link] Heartbeat send failed: %d\n", err);
        }
    }

    // Launcher timeout check
    if (state.online &&
        state.lastStatusRxMs != 0 &&
        (now - state.lastStatusRxMs) > LAUNCHER_TIMEOUT_MS)
    {
        state.online     = false;
        state.armed      = false;
        state.firePermitted = false;
        state.lastEvent  = LauncherEvent::COMMS_LOST;
        Serial.println("[WRIST/link] Launcher OFFLINE — comms timeout");
    }
}

void launcher_link_sendArmSet(LauncherLinkState& state, bool arm) {
    ArmSetPacket pkt = {};
    fillHeader(pkt.header, MessageType::ARM_SET,
               sizeof(ArmSetPayload), state.txSeq);
    pkt.payload.arm            = arm ? 1 : 0;
    pkt.payload.requestedByUi  = 1;
    pkt.payload.requestToken   = (uint32_t)millis();
    pkt.crc32 = taccom_crc32(reinterpret_cast<const uint8_t*>(&pkt),
                              sizeof(pkt) - sizeof(uint32_t));

    esp_err_t err = esp_now_send(k_launcherMac,
                                 reinterpret_cast<const uint8_t*>(&pkt),
                                 sizeof(pkt));
    Serial.printf("[WRIST/link] ARM_SET sent arm=%d err=%d\n", arm, err);
}

void launcher_link_sendFireCmd(LauncherLinkState& state,
                               uint8_t stratagemId,
                               uint8_t inputLength,
                               uint32_t requestToken,
                               uint32_t matchedAtMs)
{
    FireCmdPacket pkt = {};
    fillHeader(pkt.header, MessageType::FIRE_CMD,
               sizeof(FireCmdPayload), state.txSeq);
    pkt.payload.stratagemId  = stratagemId;
    pkt.payload.inputLength  = inputLength;
    pkt.payload.requestToken = requestToken;
    pkt.payload.matchedAtMs  = matchedAtMs;
    pkt.crc32 = taccom_crc32(reinterpret_cast<const uint8_t*>(&pkt),
                              sizeof(pkt) - sizeof(uint32_t));

    esp_err_t err = esp_now_send(k_launcherMac,
                                 reinterpret_cast<const uint8_t*>(&pkt),
                                 sizeof(pkt));
    Serial.printf("[WRIST/link] FIRE_CMD sent id=%u token=%lu err=%d\n",
                  stratagemId, (unsigned long)requestToken, err);
}
