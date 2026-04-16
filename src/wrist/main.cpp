#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "stratagem_engine.h"
#include "launcher_link.h"
#include "config_shared.h"
#include "diag_ui.h"

// ─── Runtime state ────────────────────────────────────────────────────────────
static StratagemEngineState gEngine;
static LauncherLinkState    gLink;

// Track armed state across ticks so we can detect the DISARMED→ARMED edge
static bool gWasArmed = false;

// Request token counter for fire commands
static uint32_t gFireToken = 0;

static const char* stateName(LauncherSafetyState state) {
    switch (state) {
        case LauncherSafetyState::BOOTING:   return "BOOTING";
        case LauncherSafetyState::DISARMED: return "DISARMED";
        case LauncherSafetyState::ARMED:    return "ARMED";
        case LauncherSafetyState::FIRING:   return "FIRING";
        case LauncherSafetyState::FIRED:    return "FIRED";
        case LauncherSafetyState::FAULT:    return "FAULT";
        default:                            return "UNKNOWN";
    }
}

static void printLinkStatus() {
    Serial.printf("[WRIST] Link status: online=%d state=%s armed=%d key=%d cont=%d fire_ok=%d last_event=%u fault=%u\n",
                  gLink.online,
                  stateName(gLink.remoteState),
                  gLink.armed,
                  gLink.keySwitchOn,
                  gLink.continuityOk,
                  gLink.firePermitted,
                  (unsigned)gLink.lastEvent,
                  (unsigned)gLink.lastFaultCode);
}

static void handleSerialConsole() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.length() == 0) return;

    if (cmd == "arm") {
        Serial.println("[WRIST] Serial command: ARM");
        launcher_link_sendArmSet(gLink, true);
    } else if (cmd == "disarm") {
        Serial.println("[WRIST] Serial command: DISARM");
        launcher_link_sendArmSet(gLink, false);
    } else if (cmd == "status") {
        printLinkStatus();
    } else if (cmd == "help" || cmd == "?") {
        Serial.println("[WRIST] Serial commands: arm, disarm, status, help");
    } else {
        Serial.printf("[WRIST] Unknown serial command: %s\n", cmd.c_str());
        Serial.println("[WRIST] Serial commands: arm, disarm, status, help");
    }
}

static void printMac(const char* label, const uint8_t* mac) {
    Serial.printf("[WRIST] %s %02X:%02X:%02X:%02X:%02X:%02X\n",
                  label,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

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
    uint8_t staMac[6] = {0};
    WiFi.macAddress(staMac);
    printMac("Actual STA MAC:", staMac);

    const uint8_t expectedLauncherMac[6] = LAUNCHER_MAC;
    printMac("Configured launcher peer:", expectedLauncherMac);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[WRIST] ESP-NOW init failed — halting");
        while (true) delay(1000);
    }

    // Init launcher link (registers peer + recv callback)
    launcher_link_init(gLink);

    // Init minimum diagnostics UI for bench bring-up
    diag_ui_init(expectedLauncherMac);

    // TODO: init battery monitor ADC

    Serial.println("[WRIST] Ready");
    Serial.println("[WRIST] Serial commands: arm, disarm, status, help");
}

// ─── Main loop ───────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── 1. Service ESP-NOW comms ──────────────────────────────────────────────
    launcher_link_tick(gLink, now);

    // ── 1b. Minimum diagnostics UI + action handling ────────────────────────
    diag_ui_tick(gLink, now);

    DiagUiAction uiAction = diag_ui_takeAction();
    if (uiAction == DiagUiAction::ARM) {
        Serial.println("[WRIST] UI action: ARM");
        launcher_link_sendArmSet(gLink, true);
    } else if (uiAction == DiagUiAction::DISARM) {
        Serial.println("[WRIST] UI action: DISARM");
        launcher_link_sendArmSet(gLink, false);
    }

    // ── 1c. Temporary serial debug console for bench testing ─────────────────
    handleSerialConsole();

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

    // ── 5. TODO: service battery monitor ─────────────────────────────────────
    // batteryMonitor_tick(now);

    // ── 6. TODO: service power manager ───────────────────────────────────────
    // powerManager_tick(now);
}
