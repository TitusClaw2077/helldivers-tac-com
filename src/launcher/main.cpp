#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "launcher_state.h"
#include "radio_link.h"
#include "igniter_driver.h"
#include "config_shared.h"

// ─── Runtime state ───────────────────────────────────────────────────────────
static LauncherRuntimeState gState;

// Track previous launcher state to detect changes that should push STATUS
static LauncherSafetyState  gPrevState      = LauncherSafetyState::BOOTING;
static bool                 gPrevKeySwitch  = false;
static ContinuityState      gPrevContinuity = ContinuityState::UNKNOWN;

// Periodic status broadcast timer
static uint32_t gLastStatusTxMs = 0;

// Most recent fire request token — held so we can ack with it after ignition
static uint32_t gPendingFireToken = 0;
static bool     gFireAckPending   = false;
static uint32_t gFireStartMs      = 0;

// ─── Ignition complete callback ──────────────────────────────────────────────
static void onIgnitionComplete() {
    launcherState_onIgnitionComplete(gState);
    Serial.println("[LAUNCHER] Ignition pulse complete → FIRED");

    // Send FIRE_ACK now that pulse is done
    if (gFireAckPending) {
        radio_link_sendFireAck(gState,
                               gPendingFireToken,
                               /*accepted=*/true,
                               gFireStartMs);
        gFireAckPending = false;
    }

    // Immediately follow up with a STATUS so the wrist sees FIRED state
    radio_link_sendStatus(gState);
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[LAUNCHER] Boot");

    // ── Ignition GPIO must go LOW first ─────────────────────────────────────
    igniter_init(PIN_IGNITION_GATE);
    igniter_forceOff();
    igniter_onComplete = onIgnitionComplete;

    // ── Interlock switch input ───────────────────────────────────────────────
    pinMode(PIN_ARM_SENSE, INPUT);

    // ── State machine ────────────────────────────────────────────────────────
    launcherState_init(gState);
    gPrevState = gState.state;

    // ── ESP-NOW ──────────────────────────────────────────────────────────────
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("[LAUNCHER] ESP-NOW init failed — halting");
        while (true) delay(1000);
    }

    // Register peer + recv callback
    radio_link_init();

    Serial.println("[LAUNCHER] Ready — state: DISARMED");
}

// ─── Main loop ───────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── 1. Radio tick (reserved for future use) ───────────────────────────────
    radio_link_tick(now);

    // ── 2. Interlock switch read ──────────────────────────────────────────────
    bool keySwitchNow = (digitalRead(PIN_ARM_SENSE) == HIGH);
    if (keySwitchNow != gState.keySwitchOn) {
        launcherState_onInterlockChanged(gState, keySwitchNow);
        Serial.printf("[LAUNCHER] Key switch: %s\n", keySwitchNow ? "ARM" : "SAFE");
    }

    // ── 3. Service ignition pulse ─────────────────────────────────────────────
    igniter_service(now);

    // ── 4. State machine tick ─────────────────────────────────────────────────
    launcherState_tick(gState, now);

    // ── 5. Process pending ARM command ────────────────────────────────────────
    if (radio_link_hasPendingArm()) {
        bool armCmd = radio_link_getPendingArm();
        radio_link_consumePendingArm();

        Serial.printf("[LAUNCHER] Processing ARM cmd: arm=%d\n", armCmd);
        launcherState_onArm(gState, armCmd);

        // Immediately send STATUS so wrist knows the outcome
        radio_link_sendStatus(gState);
    }

    // ── 6. Process pending FIRE command ──────────────────────────────────────
    if (radio_link_hasPendingFire()) {
        PendingFireCmd cmd = radio_link_consumePendingFire();

        Serial.printf("[LAUNCHER] Processing FIRE cmd id=%u token=%lu\n",
                      cmd.stratagemId, (unsigned long)cmd.requestToken);

        // Save token for the ack after ignition completes
        gPendingFireToken = cmd.requestToken;
        gFireAckPending   = true;
        gFireStartMs      = now;

        launcherState_onFireCmd(gState, cmd.requestToken);

        if (gState.state == LauncherSafetyState::FIRING) {
            // State machine accepted the fire command — start ignition pulse
            igniter_startPulse(IGNITION_PULSE_DURATION_MS);
            Serial.println("[LAUNCHER] Ignition pulse started");
        } else {
            // Fire was rejected — send immediate FIRE_ACK with rejected status
            // and a STATUS to update the wrist
            radio_link_sendFireAck(gState,
                                   cmd.requestToken,
                                   /*accepted=*/false,
                                   0);
            radio_link_sendStatus(gState);
            gFireAckPending = false;
        }
    }

    // ── 7. TODO: service continuity ADC ──────────────────────────────────────
    // continuity_tick(now);
    // ContinuityState cs = continuity_getState();
    // if (cs != gPrevContinuity) {
    //     launcherState_onContinuityChanged(gState, cs);
    //     gPrevContinuity = cs;
    // }

    // ── 8. Detect state changes that should push STATUS ───────────────────────
    bool stateChanged = (gState.state     != gPrevState)      ||
                        (gState.keySwitchOn != gPrevKeySwitch) ||
                        (gState.continuity  != gPrevContinuity);

    if (stateChanged) {
        gPrevState      = gState.state;
        gPrevKeySwitch  = gState.keySwitchOn;
        gPrevContinuity = gState.continuity;
        radio_link_sendStatus(gState);
    }

    // ── 9. Periodic STATUS broadcast ─────────────────────────────────────────
    if (now - gLastStatusTxMs >= STATUS_BROADCAST_INTERVAL_MS) {
        gLastStatusTxMs = now;
        radio_link_sendStatus(gState);
    }

    // ── 10. TODO: service status LED / indicators ─────────────────────────────
    // serviceIndicators(now);
}
