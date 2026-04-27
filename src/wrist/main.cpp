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

static uint32_t gFireToken = 0;
static bool gFireCommandInFlight = false;
static bool gStratagemModeRequested = false;
static bool gFireSequenceStarted = false;
static uint32_t gFireCommandStartedAtMs = 0;

static bool launcherReadyForActivation() {
    return gLink.online &&
           (gLink.armed || gLink.armRequested) &&
           gLink.keySwitchOn &&
           gLink.continuityOk &&
           gLink.lastFaultCode == FaultCode::NONE;
}

static bool launcherReadyForFire() {
    return gLink.online &&
           gLink.remoteState == LauncherSafetyState::ARMED &&
           gLink.armed &&
           gLink.keySwitchOn &&
           gLink.continuityOk &&
           gLink.firePermitted &&
           gLink.lastFaultCode == FaultCode::NONE;
}

static void applyLocalDisarmUiState(const char* reason) {
    gLink.armed = false;
    gLink.armRequested = false;
    gLink.firePermitted = false;
    gLink.remoteState = LauncherSafetyState::DISARMED;
    gLink.lastEvent = LauncherEvent::DISARMED_OK;
    Serial.printf("[WRIST] Local disarm UI state applied: %s\n", reason);
}

static void applyLocalArmUiState(const char* reason) {
    gLink.armed = true;
    gLink.armRequested = true;
    gLink.remoteState = LauncherSafetyState::ARMED;
    gLink.lastEvent = LauncherEvent::ARMED_OK;
    Serial.printf("[WRIST] Local arm UI state applied: %s\n", reason);
}

static void clearStratagemFlow(const char* reason) {
    if (gStratagemModeRequested || gEngine.active.def != nullptr || gFireSequenceStarted) {
        Serial.printf("[WRIST] Clearing stratagem flow: %s\n", reason);
    }

    stratagemEngine_clearActive(gEngine);
    gStratagemModeRequested = false;
    gFireSequenceStarted = false;
    gFireCommandInFlight = false;
    gFireCommandStartedAtMs = 0;
}

static void clearFireLockout(const char* reason) {
    if (!gFireCommandInFlight) return;

    gFireCommandInFlight = false;
    gFireCommandStartedAtMs = 0;
    Serial.printf("[WRIST] Fire lockout cleared: %s\n", reason);
}

static void updateFireLockout(uint32_t now) {
    if (!gFireCommandInFlight) return;

    if (!gLink.online) {
        clearFireLockout("launcher offline");
        return;
    }

    if (gLink.remoteState == LauncherSafetyState::FIRED ||
        gLink.remoteState == LauncherSafetyState::DISARMED ||
        gLink.remoteState == LauncherSafetyState::FAULT) {
        clearFireLockout("terminal launcher state");
        return;
    }

    if (gFireCommandStartedAtMs != 0 &&
        (now - gFireCommandStartedAtMs) >= FIRE_COMMAND_TIMEOUT_MS) {
        clearFireLockout("timeout");
        clearStratagemFlow("fire timeout recovery");
    }
}

static bool sendFireCommand(const char* source, uint32_t matchedAtMs) {
    if (gFireCommandInFlight) {
        Serial.printf("[WRIST] %s fire ignored, command already in flight\n", source);
        return false;
    }

    if (gEngine.active.def == nullptr) {
        Serial.printf("[WRIST] %s fire blocked, no active stratagem selected\n", source);
        return false;
    }

    if (launcherReadyForFire()) {
        gFireToken++;
        uint8_t sid = gEngine.active.def->id;
        uint8_t slen = gEngine.active.def->length;

        Serial.printf("[WRIST] %s sending FIRE_CMD id=%u token=%lu\n",
                      source,
                      sid,
                      (unsigned long)gFireToken);

        launcher_link_sendFireCmd(gLink,
                                  sid,
                                  slen,
                                  gFireToken,
                                  matchedAtMs);
        gFireCommandInFlight = true;
        gFireSequenceStarted = true;
        gFireCommandStartedAtMs = millis();
        return true;
    }

    Serial.printf("[WRIST] %s fire blocked, launcher not ready for fire\n", source);
    return false;
}

static const char* stateName(LauncherSafetyState state) {
    switch (state) {
        case LauncherSafetyState::BOOTING:   return "BOOTING";
        case LauncherSafetyState::DISARMED:  return "DISARMED";
        case LauncherSafetyState::ARMED:     return "ARMED";
        case LauncherSafetyState::FIRING:    return "FIRING";
        case LauncherSafetyState::FIRED:     return "FIRED";
        case LauncherSafetyState::FAULT:     return "FAULT";
        default:                             return "UNKNOWN";
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
        applyLocalArmUiState("serial arm");
    } else if (cmd == "disarm") {
        Serial.println("[WRIST] Serial command: DISARM");
        launcher_link_sendArmSet(gLink, false);
        applyLocalDisarmUiState("serial disarm");
        clearStratagemFlow("serial disarm");
    } else if (cmd == "fire") {
        if (gEngine.inputState == StratagemInputState::CONFIRMING) {
            stratagemEngine_onConfirm(gEngine);
            if (!sendFireCommand("Serial command", gEngine.matchedAtMs)) {
                clearStratagemFlow("serial fire blocked");
            }
        } else {
            Serial.println("[WRIST] Serial fire blocked: stratagem confirm not open");
        }
    } else if (cmd == "status") {
        printLinkStatus();
    } else if (cmd == "help" || cmd == "?") {
        Serial.println("[WRIST] Serial commands: arm, disarm, fire, status, help");
    } else {
        Serial.printf("[WRIST] Unknown serial command: %s\n", cmd.c_str());
        Serial.println("[WRIST] Serial commands: arm, disarm, fire, status, help");
    }
}

static void printMac(const char* label, const uint8_t* mac) {
    Serial.printf("[WRIST] %s %02X:%02X:%02X:%02X:%02X:%02X\n",
                  label,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void handleUiAction(DiagUiAction uiAction, uint32_t now) {
    switch (uiAction) {
        case DiagUiAction::NONE:
            break;

        case DiagUiAction::ARM:
            Serial.println("[WRIST] UI action: ARM");
            launcher_link_sendArmSet(gLink, true);
            applyLocalArmUiState("ui arm");
            break;

        case DiagUiAction::DISARM:
            Serial.println("[WRIST] UI action: DISARM");
            launcher_link_sendArmSet(gLink, false);
            applyLocalDisarmUiState("ui disarm");
            clearStratagemFlow("ui disarm");
            break;

        case DiagUiAction::ACTIVATE:
            Serial.println("[WRIST] UI action: ACTIVATE STRATAGEM");
            if (!launcherReadyForActivation()) {
                Serial.println("[WRIST] Activate blocked, launcher not ready");
                break;
            }
            stratagemEngine_selectRandom(gEngine);
            gStratagemModeRequested = (gEngine.active.def != nullptr);
            if (!gStratagemModeRequested) {
                Serial.println("[WRIST] Activate blocked, launch pool empty");
                break;
            }
            break;

        case DiagUiAction::CANCEL:
            Serial.println("[WRIST] UI action: CANCEL STRATAGEM");
            launcher_link_sendArmSet(gLink, false);
            applyLocalDisarmUiState("ui cancel");
            clearStratagemFlow("ui cancel");
            break;

        case DiagUiAction::DIR_UP:
            Serial.println("[WRIST] UI action: UP");
            stratagemEngine_onDirection(gEngine, Direction::UP, now);
            break;

        case DiagUiAction::DIR_DOWN:
            Serial.println("[WRIST] UI action: DOWN");
            stratagemEngine_onDirection(gEngine, Direction::DOWN, now);
            break;

        case DiagUiAction::DIR_LEFT:
            Serial.println("[WRIST] UI action: LEFT");
            stratagemEngine_onDirection(gEngine, Direction::LEFT, now);
            break;

        case DiagUiAction::DIR_RIGHT:
            Serial.println("[WRIST] UI action: RIGHT");
            stratagemEngine_onDirection(gEngine, Direction::RIGHT, now);
            break;

        case DiagUiAction::FIRE:
            Serial.println("[WRIST] UI action: FIRE MISSILE");
            if (gEngine.inputState == StratagemInputState::CONFIRMING) {
                stratagemEngine_onConfirm(gEngine);
                if (!sendFireCommand("UI action", gEngine.matchedAtMs)) {
                    clearStratagemFlow("ui fire blocked");
                }
            }
            break;
    }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[WRIST] Boot");

    randomSeed(esp_random());

    stratagemEngine_init(gEngine);

    WiFi.mode(WIFI_STA);
    uint8_t staMac[6] = {0};
    WiFi.macAddress(staMac);
    printMac("Actual STA MAC:", staMac);

    const uint8_t expectedLauncherMac[6] = LAUNCHER_MAC;
    printMac("Configured launcher peer:", expectedLauncherMac);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[WRIST] ESP-NOW init failed, halting");
        while (true) delay(1000);
    }

    launcher_link_init(gLink);
    diag_ui_init(expectedLauncherMac);

    Serial.println("[WRIST] Ready");
    Serial.println("[WRIST] Serial commands: arm, disarm, fire, status, help");
}

// ─── Main loop ───────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    launcher_link_tick(gLink, now);
    updateFireLockout(now);

    if (gStratagemModeRequested) {
        if (!launcherReadyForActivation()) {
            clearStratagemFlow("launcher no longer ready");
        } else if (gEngine.inputState != StratagemInputState::IDLE && !gLink.armed) {
            clearStratagemFlow("unexpected arm loss");
        } else if ((gEngine.inputState == StratagemInputState::CONFIRMING ||
                    gEngine.inputState == StratagemInputState::FIRING) && !gLink.firePermitted) {
            clearStratagemFlow("launcher no longer fire-permitted");
        }
    }

    if (gFireSequenceStarted && !gFireCommandInFlight &&
        (!gLink.online ||
         gLink.remoteState == LauncherSafetyState::FIRED ||
         gLink.remoteState == LauncherSafetyState::DISARMED ||
         gLink.remoteState == LauncherSafetyState::FAULT)) {
        clearStratagemFlow("fire sequence completed");
    }

    diag_ui_tick(gLink,
                 gEngine,
                 gStratagemModeRequested,
                 gFireCommandInFlight,
                 now);

    DiagUiAction uiAction = diag_ui_takeAction();
    handleUiAction(uiAction, now);

    handleSerialConsole();

    stratagemEngine_tick(gEngine, now);
}
