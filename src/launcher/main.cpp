#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "launcher_state.h"
#include "igniter_driver.h"
#include "config_shared.h"

// ─── Runtime state ───────────────────────────────────────────────────────────
static LauncherRuntimeState gState;

// ─── Ignition complete callback ──────────────────────────────────────────────
static void onIgnitionComplete() {
    launcherState_onIgnitionComplete(gState);
    Serial.println("[LAUNCHER] Ignition pulse complete → FIRED");
}

// ─── ESP-NOW receive callback ─────────────────────────────────────────────────
static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    // TODO: validate MAC, decode packet, call launcherState_onArm / launcherState_onFireCmd
    Serial.printf("[LAUNCHER] ESP-NOW rx %d bytes\n", len);
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[LAUNCHER] Boot");

    // Init igniter GPIO — must go LOW before anything else
    igniter_init(PIN_IGNITION_GATE);
    igniter_forceOff();
    igniter_onComplete = onIgnitionComplete;

    // Init interlock sense
    pinMode(PIN_ARM_SENSE, INPUT);

    // Init state
    launcherState_init(gState);

    // Init ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("[LAUNCHER] ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    Serial.println("[LAUNCHER] Ready — state: DISARMED");
}

// ─── Main loop ───────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // Service interlock
    bool keySwitchNow = (digitalRead(PIN_ARM_SENSE) == HIGH);
    if (keySwitchNow != gState.keySwitchOn) {
        launcherState_onInterlockChanged(gState, keySwitchNow);
        Serial.printf("[LAUNCHER] Key switch: %s\n", keySwitchNow ? "ARM" : "SAFE");
    }

    // Service ignition pulse
    igniter_service(now);

    // Service state machine tick
    launcherState_tick(gState, now);

    // TODO: service continuity ADC
    // TODO: service status broadcast to wrist
}
