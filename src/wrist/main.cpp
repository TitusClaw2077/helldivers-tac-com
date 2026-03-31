#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "stratagem_engine.h"
#include "config_shared.h"

// ─── Runtime state ────────────────────────────────────────────────────────────
static StratagemEngineState gEngine;
static bool gLauncherArmed   = false;
static bool gLauncherOnline  = false;

// ─── ESP-NOW receive callback ─────────────────────────────────────────────────
static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    // TODO: validate MAC, decode STATUS / FIRE_ACK packets
    // TODO: update gLauncherArmed / gLauncherOnline
    // TODO: on ARMED transition → call stratagemEngine_selectRandom(gEngine)
    Serial.printf("[WRIST] ESP-NOW rx %d bytes\n", len);
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[WRIST] Boot");

    // Init stratagem engine
    stratagemEngine_init(gEngine);

    // Init ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("[WRIST] ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    // TODO: init display, LVGL, touch driver
    // TODO: build LVGL screens

    Serial.println("[WRIST] Ready");
}

// ─── Main loop ───────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // Service stratagem engine
    stratagemEngine_tick(gEngine, now);

    // If engine says fire → send FIRE_CMD to launcher
    if (stratagemEngine_readyToFire(gEngine)) {
        Serial.println("[WRIST] Fire command ready — sending to launcher");
        // TODO: build and send FireCmdPacket via ESP-NOW
        stratagemEngine_reset(gEngine);
    }

    // TODO: service LVGL tick
    // TODO: service heartbeat timer
    // TODO: service launcher timeout
    // TODO: service battery monitor
    // TODO: service power manager
}
