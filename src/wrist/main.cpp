#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "stratagem_engine.h"
#include "launcher_link.h"
#include "config_shared.h"

// ─── Runtime state ────────────────────────────────────────────────────────────
static StratagemEngineState gEngine;
static LauncherLinkState    gLink;

// Track armed state across ticks so we can detect the DISARMED→ARMED edge
static bool gWasArmed = false;

// Request token counter for fire commands
static uint32_t gFireToken = 0;

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[WRIST] Boot");

    // Seed RNG from hardware entropy
    randomSeed(esp_random());

    // Init stratagem engine
    stratagemEngine_init(gEngine);

    // Init ESP-NOW (WiFi must be STA before esp_now_init)
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("[WRIST] ESP-NOW init failed — halting");
        while (true) delay(1000);
    }

    // Init launcher link (registers peer + recv callback)
    launcher_link_init(gLink);

    // TODO: init display driver
    // TODO: init touch driver
    // TODO: init LVGL and build screens
    // TODO: init battery monitor ADC

    Serial.println("[WRIST] Ready");
}

// ─── Main loop ───────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── 1. Service ESP-NOW comms ──────────────────────────────────────────────
    launcher_link_tick(gLink, now);

    // ── 2. Detect DISARMED→ARMED transition ──────────────────────────────────
    bool currentlyArmed = gLink.online && gLink.armed;

    if (currentlyArmed && !gWasArmed) {
        // Launcher just transitioned to ARMED — assign a random stratagem
        Serial.println("[WRIST] Launcher ARMED — selecting stratagem");
        stratagemEngine_selectRandom(gEngine);
    }

    if (!currentlyArmed && gWasArmed) {
        // Launcher went offline or disarmed — reset input engine
        Serial.println("[WRIST] Launcher disarmed/offline — resetting engine");
        stratagemEngine_reset(gEngine);
    }

    gWasArmed = currentlyArmed;

    // ── 3. Service stratagem engine ───────────────────────────────────────────
    stratagemEngine_tick(gEngine, now);

    // ── 4. Check fire-ready condition ─────────────────────────────────────────
    if (stratagemEngine_readyToFire(gEngine)) {
        // Final safety gate: launcher must still be armed and online
        if (gLink.online && gLink.armed && gLink.continuityOk) {
            gFireToken++;
            uint8_t sid = gEngine.active.def ? gEngine.active.def->id : 0;
            uint8_t slen = gEngine.active.def ? gEngine.active.def->length : 0;

            Serial.printf("[WRIST] Sending FIRE_CMD id=%u token=%lu\n",
                          sid, (unsigned long)gFireToken);

            launcher_link_sendFireCmd(gLink,
                                      sid,
                                      slen,
                                      gFireToken,
                                      gEngine.matchedAtMs);
        } else {
            Serial.println("[WRIST] Fire blocked — launcher not ready");
        }

        // Always reset engine after fire attempt regardless of outcome
        stratagemEngine_reset(gEngine);
    }

    // ── 5. TODO: service LVGL tick ────────────────────────────────────────────
    // lv_timer_handler();

    // ── 6. TODO: service touch input ─────────────────────────────────────────
    // serviceTouchInput(now);

    // ── 7. TODO: service battery monitor ─────────────────────────────────────
    // batteryMonitor_tick(now);

    // ── 8. TODO: service power manager ───────────────────────────────────────
    // powerManager_tick(now);
}
