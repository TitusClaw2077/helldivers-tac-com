#include "lvgl_ui.h"

#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
#include <lvgl/lvgl.h>

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

constexpr lv_coord_t DRAW_BUFFER_LINES = 20;

constexpr uint16_t COLOR_BG0        = 0x10A2;
constexpr uint16_t COLOR_BG1        = 0x0000;
constexpr uint16_t COLOR_PANEL      = 0x18C3;
constexpr uint16_t COLOR_PANEL_ALT  = 0x2124;
constexpr uint16_t COLOR_TEXT       = 0xFF9B;
constexpr uint16_t COLOR_DIM        = 0x8C71;
constexpr uint16_t COLOR_AMBER      = 0xFD20;
constexpr uint16_t COLOR_AMBER_DIM  = 0xB400;
constexpr uint16_t COLOR_GREEN      = 0x45C8;
constexpr uint16_t COLOR_RED        = 0xD8A7;
constexpr uint16_t COLOR_CYAN       = 0x2D7F;
constexpr uint16_t COLOR_WHITE      = 0xFFFF;

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

enum class UiScreen : uint8_t {
    LINK_WAIT = 0,
    HOME,
    WAITING_FOR_ARM,
    ENTRY,
    CONFIRM,
    FIRING
};

struct UiRenderModel {
    UiScreen screen = UiScreen::LINK_WAIT;
    bool online = false;
    bool armed = false;
    bool armRequested = false;
    bool keySwitchOn = false;
    bool continuityOk = false;
    bool firePermitted = false;
    bool activationAvailable = false;
    bool confirmAvailable = false;
    bool inputEnabled = false;
    LauncherSafetyState remoteState = LauncherSafetyState::BOOTING;
    ContinuityState continuityState = ContinuityState::UNKNOWN;
    FaultCode lastFaultCode = FaultCode::NONE;
    LauncherEvent lastEvent = LauncherEvent::NONE;
    StratagemInputState inputState = StratagemInputState::IDLE;
    uint8_t batteryPct = 0;
    int8_t linkQuality = 0;
    uint8_t inputLength = 0;
    uint8_t activeLength = 0;
    char title[48] = "TACTICAL COMMUNICATIONS";
    char subtitle[64] = "SUPER EARTH ORDNANCE LINK";
    char statusLine[96] = "LINK OFFLINE";
    char activeName[48] = "NO STRATAGEM";
    char expectedSequence[48] = "--";
    char enteredSequence[48] = "--";
    char footerLine[96] = "Awaiting launcher heartbeat";
};

CrowPanelDisplay gDisplay;
XPT2046_Touchscreen gTouch(TOUCH_CS);
bool gTouchReady = false;

lv_disp_draw_buf_t gDrawBuf;
lv_color_t gDrawBufMem[SCREEN_W * DRAW_BUFFER_LINES];
lv_disp_drv_t gDispDrv;
lv_indev_drv_t gIndevDrv;
lv_obj_t* gScreen = nullptr;
lv_obj_t* gStatusBar = nullptr;
lv_obj_t* gBadgeLink = nullptr;
lv_obj_t* gBadgeInterlock = nullptr;
lv_obj_t* gBadgeContinuity = nullptr;
lv_obj_t* gBadgeState = nullptr;
lv_obj_t* gTitleLabel = nullptr;
lv_obj_t* gSubtitleLabel = nullptr;
lv_obj_t* gHeroPanel = nullptr;
lv_obj_t* gStatusLabel = nullptr;
lv_obj_t* gActiveNameLabel = nullptr;
lv_obj_t* gSequenceExpectedLabel = nullptr;
lv_obj_t* gSequenceEnteredLabel = nullptr;
lv_obj_t* gFooterLabel = nullptr;
lv_obj_t* gHomeActions = nullptr;
lv_obj_t* gEntryPad = nullptr;
lv_obj_t* gConfirmActions = nullptr;
lv_obj_t* gLaunchOverlay = nullptr;
lv_obj_t* gArmButton = nullptr;
lv_obj_t* gActivateButton = nullptr;
lv_obj_t* gCancelButton = nullptr;
lv_obj_t* gFireButton = nullptr;
lv_obj_t* gArrowUpButton = nullptr;
lv_obj_t* gArrowLeftButton = nullptr;
lv_obj_t* gArrowDownButton = nullptr;
lv_obj_t* gArrowRightButton = nullptr;

lv_style_t gStyleScreen;
lv_style_t gStylePanel;
lv_style_t gStyleBadge;
lv_style_t gStyleTextMuted;
lv_style_t gStyleButton;
lv_style_t gStyleButtonDanger;
lv_style_t gStyleButtonPrimary;
lv_style_t gStyleButtonDisabled;
lv_style_t gStyleArrowButton;
lv_style_t gStyleLaunchOverlay;

uint32_t gLastLvTickMs = 0;
LvglUiAction gPendingAction = LvglUiAction::NONE;
UiRenderModel gLastModel = {};

static const char* stateName(LauncherSafetyState state) {
    switch (state) {
        case LauncherSafetyState::BOOTING:  return "BOOT";
        case LauncherSafetyState::DISARMED: return "SAFE";
        case LauncherSafetyState::ARMED:    return "ARMED";
        case LauncherSafetyState::FIRING:   return "FIRING";
        case LauncherSafetyState::FIRED:    return "FIRED";
        case LauncherSafetyState::FAULT:    return "FAULT";
        default:                            return "?";
    }
}

static const char* continuityName(ContinuityState state) {
    switch (state) {
        case ContinuityState::UNKNOWN:     return "UNKNOWN";
        case ContinuityState::OPEN:        return "OPEN";
        case ContinuityState::PRESENT:     return "PRESENT";
        case ContinuityState::SHORT_FAULT: return "SHORT";
        default:                           return "?";
    }
}

static bool launcherReadyForActivation(const LauncherLinkState& link) {
    return link.online &&
           (link.armed || link.armRequested) &&
           link.keySwitchOn &&
           link.continuityOk &&
           link.lastFaultCode == FaultCode::NONE;
}

static void setPendingAction(LvglUiAction action) {
    if (gPendingAction == LvglUiAction::NONE) {
        gPendingAction = action;
    }
}

static bool mapTouchPoint(const TS_Point& p, int16_t& outX, int16_t& outY) {
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
    outX = static_cast<int16_t>(sx);
    outY = static_cast<int16_t>(sy);
    return true;
}

static lv_color_t c565(uint16_t v) {
    lv_color_t c;
    c.full = v;
    return c;
}

static void setBadgeStyle(lv_obj_t* obj, uint16_t bg, uint16_t text = COLOR_TEXT) {
    lv_obj_set_style_bg_color(obj, c565(bg), 0);
    lv_obj_set_style_border_color(obj, c565(text), 0);
    lv_obj_set_style_text_color(obj, c565(text), 0);
}

static void setButtonEnabled(lv_obj_t* button, bool enabled) {
    if (enabled) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void actionButtonEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    auto action = static_cast<LvglUiAction>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (action == LvglUiAction::ARM) {
        setPendingAction(gLastModel.armed ? LvglUiAction::DISARM : LvglUiAction::ARM);
    } else {
        setPendingAction(action);
    }
}

static void displayFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* colorP) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    gDisplay.startWrite();
    gDisplay.pushImage(area->x1, area->y1, w, h, reinterpret_cast<const uint16_t*>(&colorP->full));
    gDisplay.endWrite();

    lv_disp_flush_ready(disp);
}

static void touchRead(lv_indev_drv_t*, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_RELEASED;
    if (!gTouchReady || !gTouch.touched()) return;

    TS_Point p = gTouch.getPoint();
    int16_t x = 0;
    int16_t y = 0;
    if (!mapTouchPoint(p, x, y)) return;

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
}

static void appendDirectionString(char* out, size_t outSize, Direction dir) {
    const char* arrow = directionToArrow(dir);
    if (strlen(out) > 0) {
        strlcat(out, " ", outSize);
    }
    strlcat(out, arrow, outSize);
}

static UiRenderModel buildModel(const LauncherLinkState& link,
                                const StratagemEngineState& engine,
                                bool stratagemModeRequested,
                                bool fireCommandInFlight) {
    UiRenderModel model = {};
    model.online = link.online;
    model.armed = link.armed;
    model.armRequested = link.armRequested;
    model.keySwitchOn = link.keySwitchOn;
    model.continuityOk = link.continuityOk;
    model.firePermitted = link.firePermitted;
    model.activationAvailable = launcherReadyForActivation(link) && !stratagemModeRequested;
    model.confirmAvailable = engine.inputState == StratagemInputState::CONFIRMING;
    model.inputEnabled = stratagemModeRequested && link.armed;
    model.remoteState = link.remoteState;
    model.continuityState = link.continuityState;
    model.lastFaultCode = link.lastFaultCode;
    model.lastEvent = link.lastEvent;
    model.inputState = engine.inputState;
    model.batteryPct = link.batteryPct;
    model.linkQuality = link.linkQuality;
    model.inputLength = engine.buffer.length;

    if (!link.online) {
        model.screen = UiScreen::LINK_WAIT;
        strlcpy(model.statusLine, "TACTICAL LINK OFFLINE", sizeof(model.statusLine));
        strlcpy(model.footerLine, "Waiting for launcher heartbeat", sizeof(model.footerLine));
    } else if (fireCommandInFlight || link.remoteState == LauncherSafetyState::FIRING) {
        model.screen = UiScreen::FIRING;
        strlcpy(model.statusLine, "LAUNCHING", sizeof(model.statusLine));
        strlcpy(model.footerLine, "Command accepted, maintaining lockout", sizeof(model.footerLine));
    } else if (stratagemModeRequested) {
        if (!link.armed) {
            model.screen = UiScreen::WAITING_FOR_ARM;
            strlcpy(model.statusLine, "WAITING FOR ARM", sizeof(model.statusLine));
            strlcpy(model.footerLine, "Physical interlock and remote arm required", sizeof(model.footerLine));
        } else if (engine.inputState == StratagemInputState::MATCHED ||
                   engine.inputState == StratagemInputState::CONFIRMING ||
                   engine.inputState == StratagemInputState::FIRING) {
            model.screen = UiScreen::CONFIRM;
            strlcpy(model.statusLine, "STRATAGEM ACCEPTED", sizeof(model.statusLine));
            strlcpy(model.footerLine, model.confirmAvailable ? "Confirm launch when ready" : "Awaiting fire window", sizeof(model.footerLine));
        } else {
            model.screen = UiScreen::ENTRY;
            strlcpy(model.statusLine, "ENTER STRATAGEM", sizeof(model.statusLine));
            strlcpy(model.footerLine, "Directional input armed", sizeof(model.footerLine));
        }
    } else {
        model.screen = UiScreen::HOME;
        strlcpy(model.statusLine, model.activationAvailable ? "READY FOR STRATAGEM" : "SYSTEM CHECK REQUIRED", sizeof(model.statusLine));
        strlcpy(model.footerLine, "Launcher baseline locked, LVGL shell active", sizeof(model.footerLine));
    }

    if (engine.active.def != nullptr) {
        strlcpy(model.activeName, engine.active.def->name, sizeof(model.activeName));
        model.activeLength = engine.active.def->length;
        model.expectedSequence[0] = '\0';
        for (uint8_t i = 0; i < engine.active.def->length; ++i) {
            appendDirectionString(model.expectedSequence, sizeof(model.expectedSequence), engine.active.def->sequence[i]);
        }
    }

    if (engine.buffer.length > 0) {
        model.enteredSequence[0] = '\0';
        for (uint8_t i = 0; i < engine.buffer.length; ++i) {
            appendDirectionString(model.enteredSequence, sizeof(model.enteredSequence), engine.buffer.values[i]);
        }
    }

    if (model.enteredSequence[0] == '\0') {
        strlcpy(model.enteredSequence, "--", sizeof(model.enteredSequence));
    }

    return model;
}

static lv_obj_t* createBadge(lv_obj_t* parent, const char* text, uint16_t bg) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_add_style(label, &gStyleBadge, 0);
    lv_label_set_text(label, text);
    setBadgeStyle(label, bg, COLOR_TEXT);
    return label;
}

static lv_obj_t* createActionButton(lv_obj_t* parent,
                                    const char* text,
                                    const lv_style_t* style,
                                    LvglUiAction action,
                                    lv_coord_t w,
                                    lv_coord_t h) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, const_cast<lv_style_t*>(style), 0);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_event_cb(btn, actionButtonEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(action)));
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static void initStyles() {
    lv_style_init(&gStyleScreen);
    lv_style_set_bg_color(&gStyleScreen, c565(COLOR_BG0));
    lv_style_set_bg_grad_color(&gStyleScreen, c565(COLOR_BG1));
    lv_style_set_bg_grad_dir(&gStyleScreen, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&gStyleScreen, c565(COLOR_TEXT));
    lv_style_set_pad_all(&gStyleScreen, 0);

    lv_style_init(&gStylePanel);
    lv_style_set_bg_color(&gStylePanel, c565(COLOR_PANEL));
    lv_style_set_border_color(&gStylePanel, c565(COLOR_AMBER_DIM));
    lv_style_set_border_width(&gStylePanel, 2);
    lv_style_set_radius(&gStylePanel, 8);
    lv_style_set_pad_all(&gStylePanel, 12);
    lv_style_set_text_color(&gStylePanel, c565(COLOR_TEXT));

    lv_style_init(&gStyleBadge);
    lv_style_set_pad_hor(&gStyleBadge, 10);
    lv_style_set_pad_ver(&gStyleBadge, 6);
    lv_style_set_radius(&gStyleBadge, 6);
    lv_style_set_border_width(&gStyleBadge, 1);
    lv_style_set_text_font(&gStyleBadge, &lv_font_montserrat_14);

    lv_style_init(&gStyleTextMuted);
    lv_style_set_text_color(&gStyleTextMuted, c565(COLOR_DIM));

    lv_style_init(&gStyleButton);
    lv_style_set_bg_color(&gStyleButton, c565(COLOR_PANEL_ALT));
    lv_style_set_border_color(&gStyleButton, c565(COLOR_AMBER_DIM));
    lv_style_set_border_width(&gStyleButton, 2);
    lv_style_set_radius(&gStyleButton, 8);
    lv_style_set_text_color(&gStyleButton, c565(COLOR_TEXT));
    lv_style_set_text_font(&gStyleButton, &lv_font_montserrat_18);
    lv_style_set_pad_all(&gStyleButton, 14);

    lv_style_init(&gStyleButtonPrimary);
    lv_style_set_bg_color(&gStyleButtonPrimary, c565(COLOR_AMBER));
    lv_style_set_border_color(&gStyleButtonPrimary, c565(COLOR_WHITE));
    lv_style_set_border_width(&gStyleButtonPrimary, 2);
    lv_style_set_radius(&gStyleButtonPrimary, 8);
    lv_style_set_text_color(&gStyleButtonPrimary, c565(COLOR_BG1));
    lv_style_set_text_font(&gStyleButtonPrimary, &lv_font_montserrat_18);
    lv_style_set_pad_all(&gStyleButtonPrimary, 14);

    lv_style_init(&gStyleButtonDanger);
    lv_style_set_bg_color(&gStyleButtonDanger, c565(COLOR_RED));
    lv_style_set_border_color(&gStyleButtonDanger, c565(COLOR_WHITE));
    lv_style_set_border_width(&gStyleButtonDanger, 2);
    lv_style_set_radius(&gStyleButtonDanger, 8);
    lv_style_set_text_color(&gStyleButtonDanger, c565(COLOR_WHITE));
    lv_style_set_text_font(&gStyleButtonDanger, &lv_font_montserrat_18);
    lv_style_set_pad_all(&gStyleButtonDanger, 14);

    lv_style_init(&gStyleButtonDisabled);
    lv_style_set_bg_color(&gStyleButtonDisabled, c565(COLOR_PANEL_ALT));
    lv_style_set_border_color(&gStyleButtonDisabled, c565(COLOR_DIM));
    lv_style_set_border_width(&gStyleButtonDisabled, 2);
    lv_style_set_text_color(&gStyleButtonDisabled, c565(COLOR_DIM));

    lv_style_init(&gStyleArrowButton);
    lv_style_set_bg_color(&gStyleArrowButton, c565(COLOR_PANEL_ALT));
    lv_style_set_border_color(&gStyleArrowButton, c565(COLOR_CYAN));
    lv_style_set_border_width(&gStyleArrowButton, 2);
    lv_style_set_radius(&gStyleArrowButton, 8);
    lv_style_set_text_color(&gStyleArrowButton, c565(COLOR_TEXT));
    lv_style_set_text_font(&gStyleArrowButton, &lv_font_montserrat_22);
    lv_style_set_pad_all(&gStyleArrowButton, 14);

    lv_style_init(&gStyleLaunchOverlay);
    lv_style_set_bg_color(&gStyleLaunchOverlay, c565(COLOR_RED));
    lv_style_set_bg_opa(&gStyleLaunchOverlay, LV_OPA_80);
    lv_style_set_border_color(&gStyleLaunchOverlay, c565(COLOR_AMBER));
    lv_style_set_border_width(&gStyleLaunchOverlay, 3);
    lv_style_set_radius(&gStyleLaunchOverlay, 10);
    lv_style_set_text_color(&gStyleLaunchOverlay, c565(COLOR_WHITE));
}

static void buildUi() {
    gScreen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(gScreen);
    lv_obj_add_style(gScreen, &gStyleScreen, 0);
    lv_obj_set_size(gScreen, SCREEN_W, SCREEN_H);

    gStatusBar = lv_obj_create(gScreen);
    lv_obj_remove_style_all(gStatusBar);
    lv_obj_set_size(gStatusBar, SCREEN_W - 24, 36);
    lv_obj_set_pos(gStatusBar, 12, 10);
    lv_obj_set_flex_flow(gStatusBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gStatusBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(gStatusBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gStatusBar, 0, 0);
    lv_obj_set_style_pad_gap(gStatusBar, 8, 0);

    gBadgeLink = createBadge(gStatusBar, "LINK", COLOR_PANEL_ALT);
    gBadgeInterlock = createBadge(gStatusBar, "INTLK", COLOR_PANEL_ALT);
    gBadgeContinuity = createBadge(gStatusBar, "CONT", COLOR_PANEL_ALT);
    gBadgeState = createBadge(gStatusBar, "STATE", COLOR_PANEL_ALT);

    gTitleLabel = lv_label_create(gScreen);
    lv_obj_set_style_text_font(gTitleLabel, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(gTitleLabel, c565(COLOR_WHITE), 0);
    lv_obj_set_pos(gTitleLabel, 18, 58);

    gSubtitleLabel = lv_label_create(gScreen);
    lv_obj_add_style(gSubtitleLabel, &gStyleTextMuted, 0);
    lv_obj_set_style_text_font(gSubtitleLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(gSubtitleLabel, 18, 88);

    gHeroPanel = lv_obj_create(gScreen);
    lv_obj_remove_style_all(gHeroPanel);
    lv_obj_add_style(gHeroPanel, &gStylePanel, 0);
    lv_obj_set_size(gHeroPanel, SCREEN_W - 36, 148);
    lv_obj_set_pos(gHeroPanel, 18, 114);

    gStatusLabel = lv_label_create(gHeroPanel);
    lv_obj_set_style_text_font(gStatusLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(gStatusLabel, c565(COLOR_AMBER), 0);
    lv_obj_set_pos(gStatusLabel, 10, 8);

    gActiveNameLabel = lv_label_create(gHeroPanel);
    lv_obj_set_style_text_font(gActiveNameLabel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(gActiveNameLabel, c565(COLOR_WHITE), 0);
    lv_obj_set_pos(gActiveNameLabel, 10, 52);

    gSequenceExpectedLabel = lv_label_create(gHeroPanel);
    lv_obj_set_style_text_font(gSequenceExpectedLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(gSequenceExpectedLabel, 10, 86);

    gSequenceEnteredLabel = lv_label_create(gHeroPanel);
    lv_obj_set_style_text_font(gSequenceEnteredLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(gSequenceEnteredLabel, c565(COLOR_CYAN), 0);
    lv_obj_set_pos(gSequenceEnteredLabel, 10, 112);

    gFooterLabel = lv_label_create(gScreen);
    lv_obj_add_style(gFooterLabel, &gStyleTextMuted, 0);
    lv_obj_set_style_text_font(gFooterLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_width(gFooterLabel, SCREEN_W - 40);
    lv_obj_set_pos(gFooterLabel, 18, 270);

    gHomeActions = lv_obj_create(gScreen);
    lv_obj_remove_style_all(gHomeActions);
    lv_obj_set_size(gHomeActions, SCREEN_W - 36, 42);
    lv_obj_set_pos(gHomeActions, 18, 238);
    lv_obj_set_flex_flow(gHomeActions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gHomeActions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(gHomeActions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gHomeActions, 0, 0);

    gArmButton = createActionButton(gHomeActions, "ARM / DISARM", &gStyleButton, LvglUiAction::ARM, 180, 42);
    gActivateButton = createActionButton(gHomeActions, "ACTIVATE STRATAGEM", &gStyleButtonPrimary, LvglUiAction::ACTIVATE, 250, 42);
    lv_obj_add_style(gActivateButton, &gStyleButtonDisabled, LV_STATE_DISABLED);

    gEntryPad = lv_obj_create(gScreen);
    lv_obj_remove_style_all(gEntryPad);
    lv_obj_set_size(gEntryPad, SCREEN_W - 36, 120);
    lv_obj_set_pos(gEntryPad, 18, 190);
    lv_obj_set_style_bg_opa(gEntryPad, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gEntryPad, 0, 0);
    lv_obj_add_flag(gEntryPad, LV_OBJ_FLAG_HIDDEN);

    gArrowUpButton = createActionButton(gEntryPad, LV_SYMBOL_UP, &gStyleArrowButton, LvglUiAction::DIR_UP, 104, 46);
    lv_obj_set_pos(gArrowUpButton, 188, 0);
    lv_obj_add_style(gArrowUpButton, &gStyleButtonDisabled, LV_STATE_DISABLED);
    gArrowLeftButton = createActionButton(gEntryPad, LV_SYMBOL_LEFT, &gStyleArrowButton, LvglUiAction::DIR_LEFT, 104, 46);
    lv_obj_set_pos(gArrowLeftButton, 70, 54);
    lv_obj_add_style(gArrowLeftButton, &gStyleButtonDisabled, LV_STATE_DISABLED);
    gArrowDownButton = createActionButton(gEntryPad, LV_SYMBOL_DOWN, &gStyleArrowButton, LvglUiAction::DIR_DOWN, 104, 46);
    lv_obj_set_pos(gArrowDownButton, 188, 54);
    lv_obj_add_style(gArrowDownButton, &gStyleButtonDisabled, LV_STATE_DISABLED);
    gArrowRightButton = createActionButton(gEntryPad, LV_SYMBOL_RIGHT, &gStyleArrowButton, LvglUiAction::DIR_RIGHT, 104, 46);
    lv_obj_set_pos(gArrowRightButton, 306, 54);
    lv_obj_add_style(gArrowRightButton, &gStyleButtonDisabled, LV_STATE_DISABLED);
    gCancelButton = createActionButton(gEntryPad, "CANCEL", &gStyleButtonDanger, LvglUiAction::CANCEL, 104, 46);
    lv_obj_set_pos(gCancelButton, 0, 54);

    gConfirmActions = lv_obj_create(gScreen);
    lv_obj_remove_style_all(gConfirmActions);
    lv_obj_set_size(gConfirmActions, SCREEN_W - 36, 54);
    lv_obj_set_pos(gConfirmActions, 18, 246);
    lv_obj_set_style_bg_opa(gConfirmActions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gConfirmActions, 0, 0);
    lv_obj_add_flag(gConfirmActions, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* abort = createActionButton(gConfirmActions, "ABORT", &gStyleButtonDanger, LvglUiAction::CANCEL, 180, 52);
    lv_obj_set_pos(abort, 0, 0);
    gFireButton = createActionButton(gConfirmActions, "FIRE", &gStyleButtonPrimary, LvglUiAction::FIRE, 250, 52);
    lv_obj_set_pos(gFireButton, 194, 0);
    lv_obj_add_style(gFireButton, &gStyleButtonDisabled, LV_STATE_DISABLED);

    gLaunchOverlay = lv_obj_create(gScreen);
    lv_obj_remove_style_all(gLaunchOverlay);
    lv_obj_add_style(gLaunchOverlay, &gStyleLaunchOverlay, 0);
    lv_obj_set_size(gLaunchOverlay, SCREEN_W - 96, 90);
    lv_obj_center(gLaunchOverlay);
    lv_obj_add_flag(gLaunchOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* launchLabel = lv_label_create(gLaunchOverlay);
    lv_label_set_text(launchLabel, "MISSILE AWAY");
    lv_obj_set_style_text_font(launchLabel, &lv_font_montserrat_28, 0);
    lv_obj_center(launchLabel);

    lv_scr_load(gScreen);
}

static void renderModel(const UiRenderModel& model) {
    gLastModel = model;

    lv_label_set_text_fmt(gBadgeLink, "LINK %s", model.online ? "ONLINE" : "OFFLINE");
    setBadgeStyle(gBadgeLink, model.online ? COLOR_GREEN : COLOR_RED);

    lv_label_set_text_fmt(gBadgeInterlock, "KEY %s", model.keySwitchOn ? "ARM" : "SAFE");
    setBadgeStyle(gBadgeInterlock, model.keySwitchOn ? COLOR_AMBER : COLOR_PANEL_ALT);

    lv_label_set_text_fmt(gBadgeContinuity, "CONT %s", continuityName(model.continuityState));
    setBadgeStyle(gBadgeContinuity, model.continuityOk ? COLOR_GREEN : COLOR_PANEL_ALT);

    lv_label_set_text_fmt(gBadgeState, "%s", stateName(model.remoteState));
    setBadgeStyle(gBadgeState, model.remoteState == LauncherSafetyState::FAULT ? COLOR_RED : COLOR_PANEL_ALT,
                  model.remoteState == LauncherSafetyState::ARMED ? COLOR_AMBER : COLOR_TEXT);

    lv_label_set_text(gTitleLabel, model.title);
    lv_label_set_text(gSubtitleLabel, model.subtitle);
    lv_label_set_text(gStatusLabel, model.statusLine);
    lv_label_set_text_fmt(gActiveNameLabel, "STRATAGEM  %s", model.activeName);
    lv_label_set_text_fmt(gSequenceExpectedLabel, "EXPECTED  %s", model.expectedSequence);
    lv_label_set_text_fmt(gSequenceEnteredLabel, "ENTERED   %s", model.enteredSequence);
    lv_label_set_text(gFooterLabel, model.footerLine);

    lv_obj_set_style_text_color(gStatusLabel,
                                c565(model.screen == UiScreen::HOME ? (model.activationAvailable ? COLOR_GREEN : COLOR_AMBER)
                                    : model.screen == UiScreen::CONFIRM ? COLOR_AMBER
                                    : model.screen == UiScreen::FIRING ? COLOR_WHITE
                                    : COLOR_CYAN),
                                0);

    lv_label_set_text(lv_obj_get_child(gArmButton, 0), model.armed ? "DISARM" : "ARM");
    setButtonEnabled(gActivateButton, model.activationAvailable);
    setButtonEnabled(gFireButton, model.confirmAvailable);
    setButtonEnabled(gArrowUpButton, model.inputEnabled);
    setButtonEnabled(gArrowLeftButton, model.inputEnabled);
    setButtonEnabled(gArrowDownButton, model.inputEnabled);
    setButtonEnabled(gArrowRightButton, model.inputEnabled);

    const bool showHome = model.screen == UiScreen::HOME;
    const bool showEntry = model.screen == UiScreen::ENTRY || model.screen == UiScreen::WAITING_FOR_ARM;
    const bool showConfirm = model.screen == UiScreen::CONFIRM;
    const bool showLaunch = model.screen == UiScreen::FIRING;

    if (showHome) lv_obj_clear_flag(gHomeActions, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(gHomeActions, LV_OBJ_FLAG_HIDDEN);
    if (showEntry) lv_obj_clear_flag(gEntryPad, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(gEntryPad, LV_OBJ_FLAG_HIDDEN);
    if (showConfirm) lv_obj_clear_flag(gConfirmActions, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(gConfirmActions, LV_OBJ_FLAG_HIDDEN);
    if (showLaunch) lv_obj_clear_flag(gLaunchOverlay, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(gLaunchOverlay, LV_OBJ_FLAG_HIDDEN);

    if (model.screen == UiScreen::LINK_WAIT) {
        lv_label_set_text(gActiveNameLabel, "SYSTEM  LINK SEARCH");
        lv_label_set_text(gSequenceExpectedLabel, "EXPECTED  --");
        lv_label_set_text(gSequenceEnteredLabel, "ENTERED   --");
    } else if (model.screen == UiScreen::WAITING_FOR_ARM) {
        lv_label_set_text(gActiveNameLabel, "STRATAGEM  STANDBY");
        lv_label_set_text(gSequenceEnteredLabel, "ENTERED   INPUT LOCKED UNTIL ARM");
    }
}
} // namespace

void lvgl_ui_init(const uint8_t*) {
#if CROWPANEL_REV_V22
    Serial.println("[WRIST/UI] CrowPanel pin profile: v2.2");
#else
    Serial.println("[WRIST/UI] CrowPanel pin profile: pre-v2.2");
#endif

    gDisplay.init();
    gDisplay.setRotation(SCREEN_ROTATION);
    gDisplay.setBrightness(255);
    gDisplay.fillScreen(COLOR_BG1);

    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
    gTouch.begin();
    gTouch.setRotation(SCREEN_ROTATION);
    gTouchReady = true;

    lv_init();
    lv_disp_draw_buf_init(&gDrawBuf, gDrawBufMem, nullptr, SCREEN_W * DRAW_BUFFER_LINES);

    lv_disp_drv_init(&gDispDrv);
    gDispDrv.hor_res = SCREEN_W;
    gDispDrv.ver_res = SCREEN_H;
    gDispDrv.flush_cb = displayFlush;
    gDispDrv.draw_buf = &gDrawBuf;
    lv_disp_drv_register(&gDispDrv);

    lv_indev_drv_init(&gIndevDrv);
    gIndevDrv.type = LV_INDEV_TYPE_POINTER;
    gIndevDrv.read_cb = touchRead;
    lv_indev_drv_register(&gIndevDrv);

    initStyles();
    buildUi();

    gLastLvTickMs = millis();
    Serial.println("[WRIST/UI] LVGL init OK");
}

void lvgl_ui_tick(const LauncherLinkState& link,
                  const StratagemEngineState& engine,
                  bool stratagemModeRequested,
                  bool fireCommandInFlight,
                  uint32_t now) {
    if (gLastLvTickMs == 0) {
        gLastLvTickMs = now;
    }
    uint32_t delta = now - gLastLvTickMs;
    gLastLvTickMs = now;
    lv_tick_inc(delta);

    UiRenderModel model = buildModel(link, engine, stratagemModeRequested, fireCommandInFlight);
    renderModel(model);
    lv_timer_handler();
}

LvglUiAction lvgl_ui_takeAction() {
    LvglUiAction action = gPendingAction;
    gPendingAction = LvglUiAction::NONE;
    return action;
}
