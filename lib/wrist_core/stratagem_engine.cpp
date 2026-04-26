#include "stratagem_engine.h"
#include <Arduino.h>

void stratagemEngine_init(StratagemEngineState& s) {
    s.inputState        = StratagemInputState::IDLE;
    s.buffer.length     = 0;
    s.buffer.lastInputMs= 0;
    s.active.poolIndex  = -1;
    s.active.def        = nullptr;
    s.active.selectedAtMs = 0;
    s.previousPoolIndex = -1;
    s.matchedAtMs       = 0;
    s.confirmOpenAtMs   = 0;
    s.confirmVisible    = false;
}

void stratagemEngine_reset(StratagemEngineState& s) {
    s.inputState         = StratagemInputState::IDLE;
    s.buffer.length      = 0;
    s.buffer.lastInputMs = 0;
    s.matchedAtMs        = 0;
    s.confirmOpenAtMs    = 0;
    s.confirmVisible     = false;
    // Keep active stratagem assigned — don't re-roll on reset
}

void stratagemEngine_clearActive(StratagemEngineState& s) {
    stratagemEngine_reset(s);
    s.active.poolIndex    = -1;
    s.active.def          = nullptr;
    s.active.selectedAtMs = 0;
}

void stratagemEngine_selectRandom(StratagemEngineState& s) {
    ensureLaunchPoolBuilt();

    int poolSize = getLaunchPoolCount();
    if (poolSize == 0) {
        stratagemEngine_clearActive(s);
        return;
    }

    int pick;
    if (poolSize == 1) {
        pick = 0;
    } else {
        // Anti-repeat: try up to 5 times to avoid same index
        pick = random(poolSize);
        for (int attempt = 0; attempt < 5 && pick == s.previousPoolIndex; attempt++) {
            pick = random(poolSize);
        }
    }

    s.active.poolIndex    = pick;
    s.active.def          = LAUNCH_POOL[pick];
    s.active.selectedAtMs = millis();
    s.previousPoolIndex   = pick;

    stratagemEngine_reset(s);
}

void stratagemEngine_tick(StratagemEngineState& s, uint32_t now) {
    // Inactivity timeout while inputting
    if (s.inputState == StratagemInputState::INPUTTING) {
        if (s.buffer.length > 0 &&
            (now - s.buffer.lastInputMs) >= STRATAGEM_INPUT_TIMEOUT_MS) {
            stratagemEngine_reset(s);
        }
    }

    // Post-match lockout expiry
    if (s.inputState == StratagemInputState::MATCHED) {
        if ((now - s.matchedAtMs) >= POST_MATCH_LOCKOUT_MS) {
            s.inputState       = StratagemInputState::CONFIRMING;
            s.confirmOpenAtMs  = now;
            s.confirmVisible   = true;
        }
    }
}

void stratagemEngine_onDirection(StratagemEngineState& s, Direction d, uint32_t now) {
    if (s.active.def == nullptr) return;
    if (s.inputState == StratagemInputState::MATCHED  ||
        s.inputState == StratagemInputState::CONFIRMING ||
        s.inputState == StratagemInputState::FIRING) return;

    // First input: move to INPUTTING
    if (s.inputState == StratagemInputState::IDLE) {
        s.inputState = StratagemInputState::INPUTTING;
    }

    // Append direction
    if (s.buffer.length < MAX_STRATAGEM_LEN) {
        s.buffer.values[s.buffer.length++] = d;
    }
    s.buffer.lastInputMs = now;

    // Check against active stratagem
    if (!isPrefixMatch(s.buffer.values, s.buffer.length, *s.active.def)) {
        // Wrong input — reset with error feedback
        stratagemEngine_reset(s);
        return;
    }

    if (isFullMatch(s.buffer.values, s.buffer.length, *s.active.def)) {
        s.inputState    = StratagemInputState::MATCHED;
        s.matchedAtMs   = now;
        s.confirmVisible = false;
    }
}

void stratagemEngine_onConfirm(StratagemEngineState& s) {
    if (s.inputState == StratagemInputState::CONFIRMING) {
        s.inputState     = StratagemInputState::FIRING;
        s.confirmVisible = false;
    }
}

bool stratagemEngine_isInputEnabled(const StratagemEngineState& s, bool launcherArmed) {
    return launcherArmed &&
           s.active.def != nullptr &&
           (s.inputState == StratagemInputState::IDLE ||
            s.inputState == StratagemInputState::INPUTTING);
}

bool stratagemEngine_readyToFire(const StratagemEngineState& s) {
    return s.inputState == StratagemInputState::FIRING;
}
