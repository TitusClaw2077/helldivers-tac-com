#include "diag_ui.h"

#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
#include <string.h>

namespace {
#ifndef CROWPANEL_REV_V22
#define CROWPANEL_REV_V22 1
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

constexpr int TOUCH_RAW_MIN_X = 200;
constexpr int TOUCH_RAW_MAX_X = 3900;
constexpr int TOUCH_RAW_MIN_Y = 200;
constexpr int TOUCH_RAW_MAX_Y = 3900;
constexpr int TOUCH_MIN_Z     = 200;
constexpr int TOUCH_MAX_Z     = 4000;

constexpr uint16_t UI_BG          = 0x0000;
constexpr uint16_t UI_TEXT        = 0xFFFF;
constexpr uint16_t UI_DIM         = 0x8410;
constexpr uint16_t UI_OK          = 0x07E0;
constexpr uint16_t UI_WARN        = 0xFD20;
constexpr uint16_t UI_FAULT       = 0xF800;
constexpr uint16_t UI_ACCENT      = 0x04DF;
constexpr uint16_t UI_PANEL       = 0x1082;
constexpr uint16_t UI_BTN_ARM     = 0x05E0;
constexpr uint16_t UI_BTN_DISARM  = 0xC800;
constexpr uint16_t UI_BTN_ACTION  = 0x03EF;
constexpr uint16_t UI_BTN_FIRE    = 0xF800;
constexpr uint16_t UI_BTN_CANCEL  = 0x8410;
constexpr uint16_t UI_BTN_ARROW   = 0x18C3;
constexpr uint16_t UI_BTN_DISABLED = 0x4208;

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

enum class UiScreen : uint8_t {
    LINK_WAIT = 0,
    DIAGNOSTICS_HOME,
    HOME_DETAILS,
    WAITING_FOR_ARM,
    STRATAGEM_ENTRY,
    STRATAGEM_CONFIRM,
    FIRE_IN_FLIGHT
};

struct UiViewModel {
    UiScreen screen;
    bool launcherReady;
    bool activationAvailable;
    bool confirmAvailable;
    bool launcherFaulted;
    bool stratagemActive;
    bool hasActiveStratagem;
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
uint8_t gLauncherMac[6] = {0};
bool gShowHomeDetails = false;
uint32_t gLastDetailsAgeRefreshMs = 0;
uint32_t gLastLauncherStateChangeMs = 0;

UiScreen gLastScreen = UiScreen::LINK_WAIT;
bool gHasLastFrame = false;
bool gLastOnline = false;
bool gLastArmed = false;
bool gLastArmRequested = false;
bool gLastKey = false;
bool gLastContinuityOk = false;
ContinuityState gLastContinuityState = ContinuityState::UNKNOWN;
bool gLastFirePermitted = false;
LauncherSafetyState gLastState = LauncherSafetyState::BOOTING;
LauncherEvent gLastEvent = LauncherEvent::NONE;
FaultCode gLastFault = FaultCode::NONE;
StratagemInputState gLastInputState = StratagemInputState::IDLE;
bool gLastStratagemModeRequested = false;
bool gLastFireCommandInFlight = false;
int gLastActiveStratagemId = -1;
uint8_t gLastBufferLength = 0;
bool gLastConfirmVisible = false;
bool gLastShowHomeDetails = false;
bool gLastActivationAvailable = false;

const Rect kArmToggleButton  = { 20, 140, 188, 112 };
const Rect kDetailsButton    = { 20, 262, 188, 46 };
const Rect kActivateButton   = { 220, 140, 248, 168 };
const Rect kBackButton       = { 20, 262, 140, 42 };
const Rect kCancelButton     = { 380, 8, 72, 72 };
const Rect kConfirmAbortButton = { 20, 248, 188, 56 };
const Rect kFireButton       = { 220, 248, 240, 56 };
const Rect kArrowUpButton    = { 172, 162, 136, 64 };
const Rect kArrowLeftButton  = { 24, 238, 136, 64 };
const Rect kArrowDownButton  = { 172, 238, 136, 64 };
const Rect kArrowRightButton = { 320, 238, 136, 64 };

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

const char* inputStateName(StratagemInputState state) {
    switch (state) {
        case StratagemInputState::IDLE:       return "IDLE";
        case StratagemInputState::INPUTTING:  return "INPUT";
        case StratagemInputState::MATCHED:    return "MATCHED";
        case StratagemInputState::CONFIRMING: return "CONFIRM";
        case StratagemInputState::FIRING:     return "FIRING";
        default:                              return "?";
    }
}

const char* directionGlyph(Direction d) {
    switch (d) {
        case Direction::UP:    return "^";
        case Direction::DOWN:  return "V";
        case Direction::LEFT:  return "<";
        case Direction::RIGHT: return ">";
        default:               return "?";
    }
}

uint16_t boolColor(bool v) {
    return v ? UI_OK : UI_DIM;
}

uint16_t continuityColor(ContinuityState s) {
    if (s == ContinuityState::PRESENT) return UI_OK;
    if (s == ContinuityState::OPEN) return UI_WARN;
    if (s == ContinuityState::SHORT_FAULT) return UI_FAULT;
    return UI_DIM;
}

uint16_t launcherStateColor(const LauncherLinkState& link) {
    if (!link.online) return UI_DIM;
    switch (link.remoteState) {
        case LauncherSafetyState::ARMED:  return UI_OK;
        case LauncherSafetyState::FAULT:  return UI_FAULT;
        case LauncherSafetyState::FIRING:
        case LauncherSafetyState::FIRED:  return UI_WARN;
        default:                          return UI_TEXT;
    }
}

bool launcherReadyForActivation(const LauncherLinkState& link) {
    return link.online &&
           (link.armed || link.armRequested) &&
           link.keySwitchOn &&
           link.continuityOk &&
           link.lastFaultCode == FaultCode::NONE;
}

UiViewModel buildViewModel(const LauncherLinkState& link,
                          const StratagemEngineState& engine,
                          bool stratagemModeRequested,
                          bool fireCommandInFlight) {
    UiViewModel vm = {};
    vm.launcherReady = launcherReadyForActivation(link);
    vm.activationAvailable = vm.launcherReady && !stratagemModeRequested;
    vm.confirmAvailable = (engine.inputState == StratagemInputState::CONFIRMING);
    vm.launcherFaulted = link.lastFaultCode != FaultCode::NONE || link.remoteState == LauncherSafetyState::FAULT;
    vm.stratagemActive = stratagemModeRequested;
    vm.hasActiveStratagem = engine.active.def != nullptr;

    if (!link.online) {
        gShowHomeDetails = false;
        vm.screen = UiScreen::LINK_WAIT;
    } else if (fireCommandInFlight || link.remoteState == LauncherSafetyState::FIRING) {
        gShowHomeDetails = false;
        vm.screen = UiScreen::FIRE_IN_FLIGHT;
    } else if (stratagemModeRequested) {
        gShowHomeDetails = false;
        if (!link.armed) {
            vm.screen = UiScreen::WAITING_FOR_ARM;
        } else if (engine.inputState == StratagemInputState::MATCHED ||
                   engine.inputState == StratagemInputState::CONFIRMING ||
                   engine.inputState == StratagemInputState::FIRING) {
            vm.screen = UiScreen::STRATAGEM_CONFIRM;
        } else {
            vm.screen = UiScreen::STRATAGEM_ENTRY;
        }
    } else if (gShowHomeDetails) {
        vm.screen = UiScreen::HOME_DETAILS;
    } else {
        vm.screen = UiScreen::DIAGNOSTICS_HOME;
    }

    return vm;
}

void formatStatusAge(const LauncherLinkState& link, uint32_t now, char* out, size_t outSize) {
    if (!link.online || link.lastStatusRxMs == 0 || link.lastStatusRxMs > now) {
        snprintf(out, outSize, "--");
        return;
    }

    unsigned long ageTenths = (unsigned long)((now - link.lastStatusRxMs) / 100UL);
    snprintf(out, outSize, "%lu.%lus", ageTenths / 10UL, ageTenths % 10UL);
}

void drawTitleBar(const char* title, const char* subtitle = nullptr) {
    gDisplay.fillRect(0, 0, SCREEN_W, 42, UI_PANEL);
    gDisplay.setTextColor(UI_TEXT, UI_PANEL);
    gDisplay.setTextFont(2);
    gDisplay.setCursor(16, 10);
    gDisplay.print(title);

    gDisplay.setTextColor(UI_DIM, UI_PANEL);
    gDisplay.setCursor(314, 10);
    gDisplay.print("CAMERA TBD");

    if (subtitle != nullptr) {
        gDisplay.setTextColor(UI_DIM, UI_BG);
        gDisplay.setCursor(18, 48);
        gDisplay.print(subtitle);
    }
}

void drawCompactHeader(const char* title, const char* subtitle = nullptr) {
    gDisplay.setTextColor(UI_TEXT, UI_BG);
    gDisplay.setTextFont(2);
    gDisplay.setCursor(18, 14);
    gDisplay.print(title);

    if (subtitle != nullptr) {
        gDisplay.setTextColor(UI_DIM, UI_BG);
        gDisplay.setCursor(18, 38);
        gDisplay.print(subtitle);
    }
}

void drawLinkOnlineBanner(const LauncherLinkState& link) {
    if (!link.online) return;

    gDisplay.setTextColor(UI_OK, UI_BG);
    gDisplay.setTextDatum(textdatum_t::top_right);
    gDisplay.drawString("LINK ONLINE", SCREEN_W - 18, 14);
    gDisplay.setTextDatum(textdatum_t::top_left);
}

void drawButton(const Rect& r, const char* label, uint16_t fill, uint16_t textColor = UI_TEXT) {
    gDisplay.fillRoundRect(r.x, r.y, r.w, r.h, 10, fill);
    gDisplay.drawRoundRect(r.x, r.y, r.w, r.h, 10, UI_TEXT);
    gDisplay.setTextColor(textColor, fill);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
    gDisplay.setTextDatum(textdatum_t::top_left);
}

void drawStatusRow(int16_t x, int16_t y, const char* label, const char* value, uint16_t color = UI_TEXT) {
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.setCursor(x, y);
    gDisplay.print(label);
    gDisplay.setTextColor(color, UI_BG);
    gDisplay.setCursor(x + 108, y);
    gDisplay.print(value);
}

void drawReadinessPill(int16_t x, int16_t y, const char* label, bool ready, uint16_t onColor, const char* offLabel = "BLOCK") {
    const int w = 104;
    const int h = 28;
    uint16_t fill = ready ? onColor : UI_BTN_DISABLED;
    gDisplay.fillRoundRect(x, y, w, h, 8, fill);
    gDisplay.drawRoundRect(x, y, w, h, 8, UI_TEXT);
    gDisplay.setTextColor(UI_TEXT, fill);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString(ready ? label : offLabel, x + (w / 2), y + (h / 2));
    gDisplay.setTextDatum(textdatum_t::top_left);
}

void drawGateCard(const Rect& r, const char* label, bool ready, const char* onText = "READY", const char* offText = "LOCKED") {
    uint16_t fill = ready ? UI_OK : UI_PANEL;
    uint16_t accent = ready ? UI_OK : UI_BTN_DISABLED;
    gDisplay.fillRoundRect(r.x, r.y, r.w, r.h, 12, fill);
    gDisplay.drawRoundRect(r.x, r.y, r.w, r.h, 12, UI_TEXT);

    gDisplay.setTextColor(ready ? UI_BG : UI_TEXT, fill);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString(label, r.x + (r.w / 2), r.y + 22);
    gDisplay.setTextColor(ready ? UI_BG : UI_DIM, fill);
    gDisplay.drawString(ready ? onText : offText, r.x + (r.w / 2), r.y + 52);
    gDisplay.setTextDatum(textdatum_t::top_left);

    if (!ready) {
        gDisplay.drawRoundRect(r.x + 4, r.y + 4, r.w - 8, r.h - 8, 10, accent);
    }
}

void drawCompactStatusChip(const Rect& r, const char* label, const char* value, uint16_t valueColor = UI_TEXT) {
    gDisplay.fillRoundRect(r.x, r.y, r.w, r.h, 10, UI_PANEL);
    gDisplay.drawRoundRect(r.x, r.y, r.w, r.h, 10, UI_TEXT);
    gDisplay.setTextColor(UI_DIM, UI_PANEL);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString(label, r.x + (r.w / 2), r.y + 14);
    gDisplay.setTextColor(valueColor, UI_PANEL);
    gDisplay.drawString(value, r.x + (r.w / 2), r.y + 36);
    gDisplay.setTextDatum(textdatum_t::top_left);
}

void drawStatusAgeRow(uint32_t now, const LauncherLinkState& link) {
    gDisplay.fillRect(244, 176, 196, 24, UI_BG);
    char ageBuf[24];
    if (gLastLauncherStateChangeMs == 0 || gLastLauncherStateChangeMs > now) {
        snprintf(ageBuf, sizeof(ageBuf), "--");
    } else {
        unsigned long ageTenths = (unsigned long)((now - gLastLauncherStateChangeMs) / 100UL);
        snprintf(ageBuf, sizeof(ageBuf), "%lu.%lus", ageTenths / 10UL, ageTenths % 10UL);
    }
    drawStatusRow(248, 182, "STATUS AGE", ageBuf);
}

void drawHomeScreen(const LauncherLinkState& link, const UiViewModel& vm, uint32_t now) {
    (void)now;
    drawCompactHeader("TACTICAL LINK", "Launcher status and controls");
    drawLinkOnlineBanner(link);

    const int bubbleY = 78;
    const int bubbleW = 106;
    const int bubbleH = 52;
    drawCompactStatusChip({20,  bubbleY, bubbleW, bubbleH}, "KEY",   link.keySwitchOn ? "ARM" : "SAFE", boolColor(link.keySwitchOn));
    drawCompactStatusChip({134, bubbleY, bubbleW, bubbleH}, "ARMED", link.armed ? "YES" : "NO", boolColor(link.armed));
    drawCompactStatusChip({248, bubbleY, bubbleW, bubbleH}, "CONT",  continuityName(link.continuityState), continuityColor(link.continuityState));
    drawCompactStatusChip({362, bubbleY, bubbleW, bubbleH}, "FAULT", faultName(link.lastFaultCode),
                          link.lastFaultCode == FaultCode::NONE ? UI_OK : UI_FAULT);

    drawButton(kArmToggleButton,
               link.armed ? "DISARM LAUNCHER" : "ARM LAUNCHER",
               link.armed ? UI_BTN_DISARM : UI_BTN_ARM);
    drawButton(kDetailsButton, "DETAILS", UI_PANEL);

    gLastActivationAvailable = vm.activationAvailable;

    if (vm.activationAvailable) {
        drawButton(kActivateButton, "ACTIVATE STRATAGEM", UI_BTN_ACTION);
    } else {
        drawButton(kActivateButton, "STRATAGEM LOCKED", UI_BTN_DISABLED);
    }
}

void drawHomeStratagemButton(const UiViewModel& vm) {
    if (vm.activationAvailable) {
        drawButton(kActivateButton, "ACTIVATE STRATAGEM", UI_BTN_ACTION);
    } else {
        drawButton(kActivateButton, "STRATAGEM LOCKED", UI_BTN_DISABLED);
    }
}

void drawHomeDetailsScreen(const LauncherLinkState& link, uint32_t now) {
    drawCompactHeader("SYSTEM DETAILS", "Detailed launcher status");
    drawLinkOnlineBanner(link);

    gDisplay.drawRoundRect(16, 70, 448, 176, 12, UI_ACCENT);
    drawStatusRow(28, 86,  "ONLINE", yesNo(link.online), boolColor(link.online));
    drawStatusRow(28, 110, "STATE", stateName(link.remoteState), launcherStateColor(link));
    drawStatusRow(28, 134, "KEY", yesNo(link.keySwitchOn), boolColor(link.keySwitchOn));
    drawStatusRow(28, 158, "CONT", continuityName(link.continuityState), continuityColor(link.continuityState));
    drawStatusRow(28, 182, "FAULT", faultName(link.lastFaultCode), link.lastFaultCode == FaultCode::NONE ? UI_TEXT : UI_FAULT);

    drawStatusRow(248, 86,  "ARMED", yesNo(link.armed), boolColor(link.armed));
    drawStatusRow(248, 110, "EVENT", eventName(link.lastEvent));
    drawStatusRow(248, 134, "CAN FIRE", yesNo(link.firePermitted), boolColor(link.firePermitted));
    drawStatusRow(248, 158, "LINK Q", link.linkQuality >= 0 ? "OK" : "--");

    drawStatusAgeRow(now, link);

    drawButton(kBackButton, "BACK", UI_PANEL);
}

void drawWaitingForArmScreen(const LauncherLinkState& link,
                             const StratagemEngineState& engine,
                             const UiViewModel&) {
    drawTitleBar("ACTIVATING STRATAGEM", "Waiting for launcher ARM state");

    gDisplay.setTextColor(UI_TEXT, UI_BG);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString("ESTABLISHING LAUNCH WINDOW", SCREEN_W / 2, 108);
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.drawString(link.keySwitchOn ? "Remote ARM requested" : "Key/interlock not ready", SCREEN_W / 2, 138);

    if (engine.active.def != nullptr) {
        gDisplay.setTextColor(UI_ACCENT, UI_BG);
        gDisplay.drawString(engine.active.def->name, SCREEN_W / 2, 176);
    }

    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.drawString(stateName(link.remoteState), SCREEN_W / 2, 208);
    gDisplay.setTextDatum(textdatum_t::top_left);

    drawButton(kCancelButton, "CANCEL", UI_BTN_CANCEL);
}

void drawSequenceBoxes(const StratagemEngineState& engine) {
    if (engine.active.def == nullptr) return;

    const int boxW = 36;
    const int boxH = 36;
    const int gap = 8;
    const int totalW = (engine.active.def->length * boxW) + ((engine.active.def->length - 1) * gap);
    int startX = (SCREEN_W - totalW) / 2;
    int y = 84;

    for (uint8_t i = 0; i < engine.active.def->length; i++) {
        bool complete = i < engine.buffer.length;
        uint16_t fill = complete ? UI_OK : UI_PANEL;
        gDisplay.fillRoundRect(startX + (i * (boxW + gap)), y, boxW, boxH, 8, fill);
        gDisplay.drawRoundRect(startX + (i * (boxW + gap)), y, boxW, boxH, 8, UI_TEXT);
        gDisplay.setTextColor(UI_TEXT, fill);
        gDisplay.setTextDatum(textdatum_t::middle_center);
        gDisplay.drawString(directionGlyph(engine.active.def->sequence[i]),
                            startX + (i * (boxW + gap)) + (boxW / 2),
                            y + (boxH / 2));
    }

    gDisplay.setTextDatum(textdatum_t::top_left);
}

void drawArrowButtons() {
    drawButton(kArrowUpButton,    "UP",    UI_BTN_ARROW);
    drawButton(kArrowLeftButton,  "LEFT",  UI_BTN_ARROW);
    drawButton(kArrowDownButton,  "DOWN",  UI_BTN_ARROW);
    drawButton(kArrowRightButton, "RIGHT", UI_BTN_ARROW);
}

void drawEntryScreen(const LauncherLinkState& link,
                     const StratagemEngineState& engine,
                     const UiViewModel&) {
    (void)link;
    drawCompactHeader("STRATAGEM ENTRY");
    drawButton(kCancelButton, "CANCEL", UI_BTN_CANCEL);

    if (engine.active.def != nullptr) {
        gDisplay.setTextColor(UI_ACCENT, UI_BG);
        gDisplay.setTextDatum(textdatum_t::middle_center);
        gDisplay.drawString(engine.active.def->name, SCREEN_W / 2, 66);
        gDisplay.setTextDatum(textdatum_t::top_left);
    }

    drawSequenceBoxes(engine);
    drawArrowButtons();
}

void drawConfirmScreen(const LauncherLinkState& link,
                       const StratagemEngineState& engine,
                       const UiViewModel& vm) {
    drawCompactHeader("CONFIRM STRATAGEM");

    gDisplay.setTextColor(UI_TEXT, UI_BG);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    if (engine.active.def != nullptr) {
        gDisplay.drawString(engine.active.def->name, SCREEN_W / 2, 74);
    }

    drawSequenceBoxes(engine);

    gDisplay.setTextColor(vm.confirmAvailable ? UI_OK : UI_WARN, UI_BG);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString(vm.confirmAvailable ? "FIRE WINDOW OPEN" : "LOCKING STRATAGEM", SCREEN_W / 2, 154);

    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.drawString(stateName(link.remoteState), SCREEN_W / 2, 180);
    gDisplay.setTextDatum(textdatum_t::top_left);

    drawButton(kConfirmAbortButton, "ABORT STRATAGEM", UI_BTN_CANCEL);
    drawButton(kFireButton,
               vm.confirmAvailable ? "FIRE MISSILE" : "FIRE LOCKED",
               vm.confirmAvailable ? UI_BTN_FIRE : UI_BTN_DISABLED);
}

void drawFireInFlightScreen(const LauncherLinkState& link,
                            const StratagemEngineState& engine) {
    drawCompactHeader("FIRE IN FLIGHT", "Repeat taps locked out");

    gDisplay.setTextColor(UI_FAULT, UI_BG);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString("!! FIRING MISSILE !!", SCREEN_W / 2, 118);
    gDisplay.setTextColor(UI_TEXT, UI_BG);
    if (engine.active.def != nullptr) {
        gDisplay.drawString(engine.active.def->name, SCREEN_W / 2, 154);
    }
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.drawString(stateName(link.remoteState), SCREEN_W / 2, 188);
    gDisplay.drawString(eventName(link.lastEvent), SCREEN_W / 2, 220);
    gDisplay.setTextDatum(textdatum_t::top_left);
}

void drawLinkWaitScreen() {
    drawTitleBar("TACTICAL LINK");
    gDisplay.setTextColor(UI_TEXT, UI_BG);
    gDisplay.setTextDatum(textdatum_t::middle_center);
    gDisplay.drawString("ESTABLISHING TACTICAL LAUNCHER LINK", SCREEN_W / 2, 132);
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.drawString("Waiting for launcher heartbeat", SCREEN_W / 2, 172);
    gDisplay.setTextDatum(textdatum_t::top_left);
}

void drawFrame(const LauncherLinkState& link,
               const StratagemEngineState& engine,
               bool stratagemModeRequested,
               bool fireCommandInFlight,
               uint32_t now) {
    const UiViewModel vm = buildViewModel(link, engine, stratagemModeRequested, fireCommandInFlight);

    gDisplay.startWrite();
    gDisplay.fillScreen(UI_BG);

    switch (vm.screen) {
        case UiScreen::LINK_WAIT:
            drawLinkWaitScreen();
            break;
        case UiScreen::DIAGNOSTICS_HOME:
            drawHomeScreen(link, vm, now);
            break;
        case UiScreen::HOME_DETAILS:
            drawHomeDetailsScreen(link, now);
            break;
        case UiScreen::WAITING_FOR_ARM:
            drawWaitingForArmScreen(link, engine, vm);
            break;
        case UiScreen::STRATAGEM_ENTRY:
            drawEntryScreen(link, engine, vm);
            break;
        case UiScreen::STRATAGEM_CONFIRM:
            drawConfirmScreen(link, engine, vm);
            break;
        case UiScreen::FIRE_IN_FLIGHT:
            drawFireInFlightScreen(link, engine);
            break;
    }

    gDisplay.endWrite();
}

bool mapTouchPoint(const TS_Point& p, int16_t& outX, int16_t& outY) {
    if (p.z < TOUCH_MIN_Z || p.z >= TOUCH_MAX_Z) return false;
    if (p.x == 0 && p.y == 0) return false;
    if (p.x < TOUCH_RAW_MIN_X || p.x > TOUCH_RAW_MAX_X ||
        p.y < TOUCH_RAW_MIN_Y || p.y > TOUCH_RAW_MAX_Y) {
        return false;
    }

    long sx = map(p.x, TOUCH_RAW_MAX_X, TOUCH_RAW_MIN_X, 0, SCREEN_W - 1);
    long sy = map(p.y, TOUCH_RAW_MAX_Y, TOUCH_RAW_MIN_Y, 0, SCREEN_H - 1);
    sx = constrain(sx, 0, SCREEN_W - 1);
    sy = constrain(sy, 0, SCREEN_H - 1);
    outX = (int16_t)sx;
    outY = (int16_t)sy;
    return true;
}

void queueActionForTouch(int16_t x,
                         int16_t y,
                         const LauncherLinkState& link,
                         const StratagemEngineState& engine,
                         bool stratagemModeRequested,
                         bool fireCommandInFlight) {
    const UiViewModel vm = buildViewModel(link, engine, stratagemModeRequested, fireCommandInFlight);

    switch (vm.screen) {
        case UiScreen::DIAGNOSTICS_HOME:
            if (kArmToggleButton.contains(x, y)) {
                gPendingAction = link.armed ? DiagUiAction::DISARM : DiagUiAction::ARM;
            } else if (kDetailsButton.contains(x, y)) {
                gShowHomeDetails = true;
            } else if (vm.activationAvailable && kActivateButton.contains(x, y)) {
                gPendingAction = DiagUiAction::ACTIVATE;
            }
            break;

        case UiScreen::HOME_DETAILS:
            if (kBackButton.contains(x, y)) {
                gShowHomeDetails = false;
            }
            break;

        case UiScreen::WAITING_FOR_ARM:
            if (kCancelButton.contains(x, y)) {
                gPendingAction = DiagUiAction::CANCEL;
            }
            break;

        case UiScreen::STRATAGEM_ENTRY:
            if (kCancelButton.contains(x, y)) {
                gPendingAction = DiagUiAction::CANCEL;
            } else if (kArrowUpButton.contains(x, y)) {
                gPendingAction = DiagUiAction::DIR_UP;
            } else if (kArrowLeftButton.contains(x, y)) {
                gPendingAction = DiagUiAction::DIR_LEFT;
            } else if (kArrowDownButton.contains(x, y)) {
                gPendingAction = DiagUiAction::DIR_DOWN;
            } else if (kArrowRightButton.contains(x, y)) {
                gPendingAction = DiagUiAction::DIR_RIGHT;
            }
            break;

        case UiScreen::STRATAGEM_CONFIRM:
            if (kConfirmAbortButton.contains(x, y)) {
                gPendingAction = DiagUiAction::CANCEL;
            } else if (vm.confirmAvailable && kFireButton.contains(x, y)) {
                gPendingAction = DiagUiAction::FIRE;
            }
            break;

        case UiScreen::FIRE_IN_FLIGHT:
        case UiScreen::LINK_WAIT:
            break;
    }
}

void serviceTouch(const LauncherLinkState& link,
                  const StratagemEngineState& engine,
                  bool stratagemModeRequested,
                  bool fireCommandInFlight) {
    if (!gTouchReady) return;

    bool touching = gTouch.touched();
    if (!touching) {
        gTouchWasDown = false;
        return;
    }

    TS_Point p = gTouch.getPoint();
    int16_t x = 0;
    int16_t y = 0;
    bool valid = mapTouchPoint(p, x, y);
    bool isNewTouch = !gTouchWasDown;
    gTouchWasDown = true;

    if (!valid || !isNewTouch) return;

    queueActionForTouch(x, y, link, engine, stratagemModeRequested, fireCommandInFlight);
}

bool shouldRedraw(const LauncherLinkState& link,
                  const StratagemEngineState& engine,
                  bool stratagemModeRequested,
                  bool fireCommandInFlight) {
    const UiViewModel vm = buildViewModel(link, engine, stratagemModeRequested, fireCommandInFlight);

    if (!gHasLastFrame) return true;
    if (vm.screen != gLastScreen) return true;
    if (link.online != gLastOnline) return true;
    if (link.armed != gLastArmed) return true;
    if (link.armRequested != gLastArmRequested) return true;
    if (link.keySwitchOn != gLastKey) return true;
    if (link.continuityOk != gLastContinuityOk) return true;
    if (link.continuityState != gLastContinuityState) return true;
    if (link.firePermitted != gLastFirePermitted) return true;
    if (link.remoteState != gLastState) return true;
    if (link.lastEvent != gLastEvent) return true;
    if (link.lastFaultCode != gLastFault) return true;
    if (engine.inputState != gLastInputState) return true;
    if (stratagemModeRequested != gLastStratagemModeRequested) return true;
    if (fireCommandInFlight != gLastFireCommandInFlight) return true;
    if (gShowHomeDetails != gLastShowHomeDetails) return true;
    if ((engine.active.def ? engine.active.def->id : -1) != gLastActiveStratagemId) return true;
    if (engine.buffer.length != gLastBufferLength) return true;
    if (engine.confirmVisible != gLastConfirmVisible) return true;
    return false;
}

void rememberFrame(const LauncherLinkState& link,
                   const StratagemEngineState& engine,
                   bool stratagemModeRequested,
                   bool fireCommandInFlight) {
    gHasLastFrame = true;
    gLastScreen = buildViewModel(link, engine, stratagemModeRequested, fireCommandInFlight).screen;
    gLastOnline = link.online;
    gLastArmed = link.armed;
    gLastArmRequested = link.armRequested;
    gLastKey = link.keySwitchOn;
    gLastContinuityOk = link.continuityOk;
    gLastContinuityState = link.continuityState;
    gLastFirePermitted = link.firePermitted;
    gLastState = link.remoteState;
    gLastEvent = link.lastEvent;
    gLastFault = link.lastFaultCode;
    gLastInputState = engine.inputState;
    gLastStratagemModeRequested = stratagemModeRequested;
    gLastFireCommandInFlight = fireCommandInFlight;
    gLastShowHomeDetails = gShowHomeDetails;
    gLastActivationAvailable = buildViewModel(link, engine, stratagemModeRequested, fireCommandInFlight).activationAvailable;
    gLastActiveStratagemId = engine.active.def ? engine.active.def->id : -1;
    gLastBufferLength = engine.buffer.length;
    gLastConfirmVisible = engine.confirmVisible;
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
    gDisplay.println("TACTICAL LINK");
    gDisplay.setTextColor(UI_DIM, UI_BG);
    gDisplay.setCursor(20, 48);
    gDisplay.println("Display init OK");

    gTouch.begin();
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
    gTouch.setRotation(SCREEN_ROTATION);
    gTouchReady = true;

    Serial.println("[WRIST/UI] Display init OK");
    Serial.println("[WRIST/UI] Touch init OK (first-pass calibration)");
}

void diag_ui_tick(const LauncherLinkState& link,
                  const StratagemEngineState& engine,
                  bool stratagemModeRequested,
                  bool fireCommandInFlight,
                  uint32_t now) {
    const UiViewModel vm = buildViewModel(link, engine, stratagemModeRequested, fireCommandInFlight);

    serviceTouch(link, engine, stratagemModeRequested, fireCommandInFlight);

    if (!gHasLastFrame) {
        gLastLauncherStateChangeMs = now;
    } else if (link.online != gLastOnline ||
               link.armed != gLastArmed ||
               link.keySwitchOn != gLastKey ||
               link.continuityState != gLastContinuityState ||
               link.firePermitted != gLastFirePermitted ||
               link.remoteState != gLastState ||
               link.lastEvent != gLastEvent ||
               link.lastFaultCode != gLastFault) {
        gLastLauncherStateChangeMs = now;
    }

    if (shouldRedraw(link, engine, stratagemModeRequested, fireCommandInFlight)) {
        drawFrame(link, engine, stratagemModeRequested, fireCommandInFlight, now);
        rememberFrame(link, engine, stratagemModeRequested, fireCommandInFlight);
        gLastDetailsAgeRefreshMs = now;
    } else if (vm.screen == UiScreen::DIAGNOSTICS_HOME) {
        gDisplay.startWrite();
        drawHomeStratagemButton(vm);
        gDisplay.endWrite();
        gLastActivationAvailable = vm.activationAvailable;
    } else if (vm.screen == UiScreen::HOME_DETAILS &&
               now - gLastDetailsAgeRefreshMs >= 1000) {
        gDisplay.startWrite();
        drawStatusAgeRow(now, link);
        gDisplay.endWrite();
        gLastDetailsAgeRefreshMs = now;
    }
}

DiagUiAction diag_ui_takeAction() {
    DiagUiAction action = gPendingAction;
    gPendingAction = DiagUiAction::NONE;
    return action;
}

void diag_ui_requestFullRedraw() {
    gHasLastFrame = false;
}
