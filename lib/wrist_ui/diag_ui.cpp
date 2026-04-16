#include "diag_ui.h"

#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
#include "protocol_types.h"

namespace {
#ifndef CROWPANEL_REV_V22
#define CROWPANEL_REV_V22 0
#endif

constexpr int TFT_MOSI = 13;
#if CROWPANEL_REV_V22
constexpr int TFT_MISO = 33;
constexpr int TOUCH_CS = 12;
#else
constexpr int TFT_MISO = 12;
constexpr int TOUCH_CS = 33;
#endif
constexpr int TFT_SCLK = 14;
constexpr int TFT_CS   = 15;
constexpr int TFT_DC   = 2;
constexpr int TFT_BL   = 27;

constexpr uint16_t SCREEN_W = 480;
constexpr uint16_t SCREEN_H = 320;
constexpr uint8_t SCREEN_ROTATION = 1;

// These are intentionally coarse first-pass calibration values for the CrowPanel
// resistive touch stack. Buttons are large so bench bring-up can work even if
// the raw range varies a little by unit.
constexpr int TOUCH_RAW_MIN_X = 200;
constexpr int TOUCH_RAW_MAX_X = 3900;
constexpr int TOUCH_RAW_MIN_Y = 200;
constexpr int TOUCH_RAW_MAX_Y = 3900;
constexpr int TOUCH_MIN_Z     = 200;
constexpr int TOUCH_MAX_Z     = 4000;

constexpr uint16_t UI_BG      = 0x0000;
constexpr uint16_t UI_TEXT    = 0xFFFF;
constexpr uint16_t UI_DIM     = 0x8410;
constexpr uint16_t UI_OK      = 0x07E0;
constexpr uint16_t UI_WARN    = 0xFD20;
constexpr uint16_t UI_FAULT   = 0xF800;
constexpr uint16_t UI_BTN_ARM = 0x05E0;
constexpr uint16_t UI_BTN_DIS = 0xC800;

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

class CrowPanelDisplay : public lgfx::LGFX_Device {
public:
    CrowPanelDisplay() {
        {
            auto cfg = _bus.config();
            cfg.spi_host = VSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = TFT_MISO;
            cfg.pin_dc = TFT_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs = TFT_CS;
            cfg.pin_rst = -1;
            cfg.pin_busy = -1;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl = TFT_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }

private:
    lgfx::Bus_SPI _bus;
    lgfx::Panel_ILI9488 _panel;
    lgfx::Light_PWM _light;
};

CrowPanelDisplay gDisplay;
XPT2046_Touchscreen gTouch(TOUCH_CS);
DiagUiAction gPendingAction = DiagUiAction::NONE;
bool gTouchReady = false;
bool gTouchWasDown = false;
uint32_t gLastTouchLogMs = 0;
int16_t gLastTouchX = -1;
int16_t gLastTouchY = -1;
uint32_t gLastDrawMs = 0;
uint32_t gLastAgeDrawMs = 0;
uint16_t gLastRxSeq = 0xFFFF;
uint32_t gLastStatusRxMs = 0xFFFFFFFF;
bool gLastOnline = false;
bool gLastArmed = false;
bool gLastKey = false;
bool gLastContinuityOk = false;
LauncherSafetyState gLastState = LauncherSafetyState::BOOTING;
LauncherEvent gLastEvent = LauncherEvent::NONE;
FaultCode gLastFault = FaultCode::NONE;
uint8_t gLauncherMac[6] = {0};

const Rect kArmButton    = { 20, 248, 210, 56 };
const Rect kDisarmButton = { 250, 248, 210, 56 };
const Rect kArmTouchZone    = { 0, 220, SCREEN_W / 2, SCREEN_H - 220 };
const Rect kDisarmTouchZone = { SCREEN_W / 2, 220, SCREEN_W / 2, SCREEN_H - 220 };

const char* yesNo(bool v) {
    return v ? "YES" : "NO";
}

const char* stateName(LauncherSafetyState state) {
    switch (state) {
        case LauncherSafetyState::BOOTING:  return "BOOTING";
        case LauncherSafetyState::DISARMED: return "DISARMED";
        case LauncherSafetyState::ARMED:    return "ARMED";
        case LauncherSafetyState::FIRING:   return "FIRING";
        case LauncherSafetyState::FIRED:    return "FIRED";
        case LauncherSafetyState::FAULT:    return "FAULT";
        default:                            return "UNKNOWN";
    }
}

const char* continuityName(ContinuityState state) {
    switch (state) {
        case ContinuityState::UNKNOWN:     return "UNKNOWN";
        case ContinuityState::OPEN:        return "OPEN";
        case ContinuityState::PRESENT:     return "PRESENT";
        case ContinuityState::SHORT_FAULT: return "SHORT";
        default:                           return "?";
    }
}

const char* eventName(LauncherEvent event) {
    switch (event) {
        case LauncherEvent::NONE:               return "NONE";
        case LauncherEvent::READY:              return "READY";
        case LauncherEvent::ARMED_OK:           return "ARMED_OK";
        case LauncherEvent::DISARMED_OK:        return "DISARMED_OK";
        case LauncherEvent::FIRE_SENT:          return "FIRE_SENT";
        case LauncherEvent::FIRED_OK:           return "FIRED_OK";
        case LauncherEvent::CONTINUITY_FAIL:    return "CONT_FAIL";
        case LauncherEvent::INTERLOCK_BLOCKED:  return "INTERLOCK";
        case LauncherEvent::COMMS_LOST:         return "COMMS_LOST";
        case LauncherEvent::FAULT_GENERIC:      return "FAULT";
        default:                                return "?";
    }
}

const char* faultName(FaultCode fault) {
    switch (fault) {
        case FaultCode::NONE:                return "NONE";
        case FaultCode::INVALID_MAC:         return "INVALID_MAC";
        case FaultCode::BAD_PACKET:          return "BAD_PACKET";
        case FaultCode::INTERLOCK_OFF:       return "INTERLOCK_OFF";
        case FaultCode::CONTINUITY_OPEN:     return "CONT_OPEN";
        case FaultCode::CONTINUITY_SHORT:    return "CONT_SHORT";
        case FaultCode::FIRE_WHILE_DISARMED: return "FIRE_DISARMED";
        case FaultCode::FIRE_TIMEOUT:        return "FIRE_TIMEOUT";
        case FaultCode::INTERNAL_ERROR:      return "INTERNAL";
        default:                             return "?";
    }
}

uint16_t stateColor(const LauncherLinkState& link) {
    if (!link.online) return UI_DIM;
    switch (link.remoteState) {
        case LauncherSafetyState::ARMED:    return UI_OK;
        case LauncherSafetyState::FAULT:    return UI_FAULT;
        case LauncherSafetyState::FIRING:
        case LauncherSafetyState::FIRED:    return UI_WARN;
        default:                            return UI_TEXT;
    }
}

void drawLabelValue(int16_t y, const char* label, const char* value, uint16_t valueColor = UI_TEXT) {
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.setCursor(20, y);
    gDisplay.print(label);
    gDisplay.setTextColor(valueColor, UI_BG);
    gDisplay.setCursor(170, y);
    gDisplay.print(value);
}

void drawButton(const Rect& r, const char* label, uint16_t fill) {
    gDisplay.fillRoundRect(r.x, r.y, r.w, r.h, 8, fill);
    gDisplay.drawRoundRect(r.x, r.y, r.w, r.h, 8, UI_TEXT);
    gDisplay.setTextColor(UI_TEXT, fill);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
    gDisplay.setTextDatum(textdatum_t::top_left);
}

void formatStatusAge(const LauncherLinkState& link, uint32_t now, char* out, size_t outSize) {
    if (!link.online || link.lastStatusRxMs == 0 || link.lastStatusRxMs > now) {
        snprintf(out, outSize, "--");
        return;
    }

    unsigned long ageSeconds = (unsigned long)((now - link.lastStatusRxMs) / 1000UL);
    snprintf(out, outSize, "%lus", ageSeconds);
}

void drawStatusAgeRow(const LauncherLinkState& link, uint32_t now) {
    char ageBuf[24];
    formatStatusAge(link, now, ageBuf, sizeof(ageBuf));

    gDisplay.startWrite();
    gDisplay.fillRect(160, 232, 120, 16, UI_BG);
    drawLabelValue(232, "STATUS AGE", ageBuf);
    gDisplay.endWrite();
}

void drawFrame(const LauncherLinkState& link, uint32_t now) {
    gDisplay.startWrite();
    gDisplay.fillScreen(UI_BG);
    gDisplay.setTextFont(2);
    gDisplay.setTextColor(UI_TEXT, UI_BG);
    gDisplay.setCursor(20, 14);
    gDisplay.print("TACTICAL LINK DIAGNOSTICS");

    char macBuf[24];
    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             gLauncherMac[0], gLauncherMac[1], gLauncherMac[2],
             gLauncherMac[3], gLauncherMac[4], gLauncherMac[5]);
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.setCursor(20, 34);
    gDisplay.printf("Launcher: %s", macBuf);

    drawLabelValue(64, "ONLINE", yesNo(link.online), link.online ? UI_OK : UI_FAULT);
    drawLabelValue(88, "STATE", stateName(link.remoteState), stateColor(link));
    drawLabelValue(112, "KEY", yesNo(link.keySwitchOn), link.keySwitchOn ? UI_OK : UI_TEXT);
    drawLabelValue(136, "ARMED", yesNo(link.armed), link.armed ? UI_OK : UI_TEXT);
    drawLabelValue(160, "CONT", continuityName(link.continuityState), link.continuityState == ContinuityState::PRESENT ? UI_OK : (link.continuityState == ContinuityState::OPEN ? UI_WARN : UI_TEXT));
    drawLabelValue(184, "EVENT", eventName(link.lastEvent));
    drawLabelValue(208, "FAULT", faultName(link.lastFaultCode), link.lastFaultCode == FaultCode::NONE ? UI_TEXT : UI_FAULT);

    char ageBuf[24];
    formatStatusAge(link, now, ageBuf, sizeof(ageBuf));
    drawLabelValue(232, "STATUS AGE", ageBuf);

    drawButton(kArmButton, "ARM", UI_BTN_ARM);
    drawButton(kDisarmButton, "DISARM", UI_BTN_DIS);
    gDisplay.endWrite();
}

bool mapTouchPoint(const TS_Point& p, int16_t& outX, int16_t& outY) {
    if (p.z < TOUCH_MIN_Z) return false;
    if (p.z >= TOUCH_MAX_Z) return false;
    if (p.x == 0 && p.y == 0) return false;

    if (p.x < TOUCH_RAW_MIN_X || p.x > TOUCH_RAW_MAX_X ||
        p.y < TOUCH_RAW_MIN_Y || p.y > TOUCH_RAW_MAX_Y) {
        return false;
    }

    // For this landscape layout, raw Y usually maps best to screen X and raw X
    // maps inversely to screen Y on ILI9488 + XPT2046 boards.
    long sx = map(p.y, TOUCH_RAW_MIN_Y, TOUCH_RAW_MAX_Y, 0, SCREEN_W - 1);
    long sy = map(p.x, TOUCH_RAW_MAX_X, TOUCH_RAW_MIN_X, 0, SCREEN_H - 1);

    sx = constrain(sx, 0, SCREEN_W - 1);
    sy = constrain(sy, 0, SCREEN_H - 1);
    outX = (int16_t)sx;
    outY = (int16_t)sy;
    return true;
}

void serviceTouch() {
    if (!gTouchReady) return;

    bool touching = gTouch.touched();
    if (!touching) {
        if (gTouchWasDown) {
            Serial.println("[WRIST/UI] touch released");
        }
        gTouchWasDown = false;
        gLastTouchX = -1;
        gLastTouchY = -1;
        return;
    }

    TS_Point p = gTouch.getPoint();
    int16_t x = 0;
    int16_t y = 0;
    bool valid = mapTouchPoint(p, x, y);
    bool isNewTouch = !gTouchWasDown;
    bool moved = (x != gLastTouchX) || (y != gLastTouchY);
    bool logDue = (millis() - gLastTouchLogMs) >= 150;

    if (isNewTouch || moved || logDue) {
        Serial.printf("[WRIST/UI] touch raw=(%d,%d,%d) mapped=(%d,%d) valid=%d\n", p.x, p.y, p.z, x, y, valid);
        gLastTouchLogMs = millis();
    }
    gTouchWasDown = true;
    gLastTouchX = x;
    gLastTouchY = y;

    if (!valid) return;

    if (!isNewTouch) return;

    if (kArmTouchZone.contains(x, y)) {
        gPendingAction = DiagUiAction::ARM;
        Serial.println("[WRIST/UI] ARM zone pressed");
    } else if (kDisarmTouchZone.contains(x, y)) {
        gPendingAction = DiagUiAction::DISARM;
        Serial.println("[WRIST/UI] DISARM zone pressed");
    } else {
        Serial.println("[WRIST/UI] touch missed action zones");
    }
}

bool shouldRedraw(const LauncherLinkState& link, uint32_t now) {
    if (link.lastRxSeq != gLastRxSeq) return true;
    if (link.lastStatusRxMs != gLastStatusRxMs) return true;
    if (link.online != gLastOnline) return true;
    if (link.armed != gLastArmed) return true;
    if (link.keySwitchOn != gLastKey) return true;
    if (link.continuityOk != gLastContinuityOk) return true;
    if (link.remoteState != gLastState) return true;
    if (link.lastEvent != gLastEvent) return true;
    if (link.lastFaultCode != gLastFault) return true;
    return false;
}

void rememberState(const LauncherLinkState& link, uint32_t now) {
    gLastDrawMs = now;
    gLastRxSeq = link.lastRxSeq;
    gLastStatusRxMs = link.lastStatusRxMs;
    gLastOnline = link.online;
    gLastArmed = link.armed;
    gLastKey = link.keySwitchOn;
    gLastContinuityOk = link.continuityOk;
    gLastState = link.remoteState;
    gLastEvent = link.lastEvent;
    gLastFault = link.lastFaultCode;
}
} // namespace

void diag_ui_init(const uint8_t launcherMac[6]) {
    memcpy(gLauncherMac, launcherMac, sizeof(gLauncherMac));

#if CROWPANEL_REV_V22
    Serial.println("[WRIST/UI] CrowPanel pin profile: v2.2");
#else
    Serial.println("[WRIST/UI] CrowPanel pin profile: pre-v2.2");
#endif

    gDisplay.init();
    gDisplay.setRotation(SCREEN_ROTATION);
    gDisplay.setBrightness(255);
    gDisplay.fillScreen(UI_BG);
    gDisplay.setTextFont(2);
    gDisplay.setTextColor(UI_TEXT, UI_BG);
    gDisplay.setCursor(20, 20);
    gDisplay.println("TACTICAL LINK DIAGNOSTICS");
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.setCursor(20, 48);
    gDisplay.println("Display init OK");

    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
    gTouch.begin();
    gTouch.setRotation(SCREEN_ROTATION);
    gTouchReady = true;
    Serial.println("[WRIST/UI] Display init OK");
    Serial.println("[WRIST/UI] Touch init OK (first-pass calibration)");
}

void diag_ui_tick(const LauncherLinkState& link, uint32_t now) {
    serviceTouch();

    if (shouldRedraw(link, now)) {
        drawFrame(link, now);
        rememberState(link, now);
        gLastAgeDrawMs = now;
    } else if (now - gLastAgeDrawMs >= 1000) {
        drawStatusAgeRow(link, now);
        gLastAgeDrawMs = now;
    }
}

DiagUiAction diag_ui_takeAction() {
    DiagUiAction action = gPendingAction;
    gPendingAction = DiagUiAction::NONE;
    return action;
}
