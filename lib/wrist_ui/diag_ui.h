#pragma once

#include <stdint.h>
#include "launcher_link.h"

enum class DiagUiAction : uint8_t {
    NONE = 0,
    ARM,
    DISARM
};

void diag_ui_init(const uint8_t launcherMac[6]);
void diag_ui_tick(const LauncherLinkState& link, uint32_t now);
DiagUiAction diag_ui_takeAction();
