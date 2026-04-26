#pragma once

#include <stdint.h>
#include "launcher_link.h"
#include "stratagem_engine.h"

enum class DiagUiAction : uint8_t {
    NONE = 0,
    ARM,
    DISARM,
    ACTIVATE,
    CANCEL,
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT,
    FIRE
};

void diag_ui_init(const uint8_t launcherMac[6]);
void diag_ui_tick(const LauncherLinkState& link,
                  const StratagemEngineState& engine,
                  bool stratagemModeRequested,
                  bool fireCommandInFlight,
                  uint32_t now);
DiagUiAction diag_ui_takeAction();
