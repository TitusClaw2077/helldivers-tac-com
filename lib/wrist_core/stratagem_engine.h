#pragma once
#include "protocol_types.h"
#include "stratagems.h"
#include "config_shared.h"
#include <stdint.h>

struct InputBuffer {
    Direction values[MAX_STRATAGEM_LEN];
    uint8_t   length;
    uint32_t  lastInputMs;
};

struct StratagemEngineState {
    StratagemInputState inputState;
    InputBuffer         buffer;
    ActiveStratagem     active;
    int                 previousPoolIndex;  // for anti-repeat
    uint32_t            matchedAtMs;
    uint32_t            confirmOpenAtMs;
    bool                confirmVisible;
};

void    stratagemEngine_init(StratagemEngineState& s);
void    stratagemEngine_tick(StratagemEngineState& s, uint32_t now);
void    stratagemEngine_onDirection(StratagemEngineState& s, Direction d, uint32_t now);
void    stratagemEngine_onConfirm(StratagemEngineState& s);
void    stratagemEngine_reset(StratagemEngineState& s);

// Call when launcher transitions to ARMED to assign a random active stratagem
void    stratagemEngine_selectRandom(StratagemEngineState& s);

bool    stratagemEngine_isInputEnabled(const StratagemEngineState& s, bool launcherArmed);
bool    stratagemEngine_readyToFire(const StratagemEngineState& s);
