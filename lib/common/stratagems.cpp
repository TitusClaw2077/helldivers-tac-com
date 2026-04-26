#include "stratagems.h"
#include <string.h>

using D = Direction;

// ─── Full stratagem database ─────────────────────────────────────────────────
const StratagemDef STRATAGEM_DB[] = {
    // id, name, sequence[], length, inLaunchPool

    // ── Eagle ──────────────────────────────────────────────────────────────
    { StratagemID::EAGLE_AIRSTRIKE,     "Eagle Airstrike",
      { D::UP, D::RIGHT, D::DOWN, D::RIGHT }, 4, true },

    { StratagemID::EAGLE_500KG,         "Eagle 500kg Bomb",
      { D::UP, D::RIGHT, D::DOWN, D::DOWN, D::DOWN }, 5, false },

    { StratagemID::EAGLE_STRAFING_RUN,  "Eagle Strafing Run",
      { D::UP, D::RIGHT, D::RIGHT }, 3, false },

    { StratagemID::EAGLE_110MM,         "Eagle 110mm Rocket Pods",
      { D::UP, D::RIGHT, D::UP, D::LEFT }, 4, true },

    { StratagemID::EAGLE_NAPALM,        "Eagle Napalm Airstrike",
      { D::UP, D::RIGHT, D::DOWN, D::UP }, 4, false },

    { StratagemID::EAGLE_CLUSTER,       "Eagle Cluster Bomb",
      { D::UP, D::RIGHT, D::DOWN, D::DOWN, D::RIGHT }, 5, false },

    { StratagemID::EAGLE_SMOKE,         "Eagle Smoke Strike",
      { D::UP, D::RIGHT, D::UP, D::DOWN }, 4, false },

    // ── Orbital ────────────────────────────────────────────────────────────
    { StratagemID::ORBITAL_PRECISION,   "Orbital Precision Strike",
      { D::RIGHT, D::RIGHT, D::DOWN, D::RIGHT }, 4, true },

    { StratagemID::ORBITAL_AIRBURST,    "Orbital Airburst Strike",
      { D::RIGHT, D::RIGHT, D::RIGHT }, 3, false },

    { StratagemID::ORBITAL_RAILCANNON,  "Orbital Railcannon Strike",
      { D::RIGHT, D::UP, D::DOWN, D::DOWN, D::RIGHT }, 5, false },

    { StratagemID::ORBITAL_LASER,       "Orbital Laser",
      { D::RIGHT, D::DOWN, D::UP, D::RIGHT, D::DOWN }, 5, false },

    { StratagemID::ORBITAL_GATLING,     "Orbital Gatling Barrage",
      { D::RIGHT, D::DOWN, D::LEFT, D::UP, D::UP }, 5, false },

    { StratagemID::ORBITAL_380HE,       "Orbital 380MM HE Barrage",
      { D::RIGHT, D::DOWN, D::UP, D::UP, D::LEFT, D::DOWN, D::DOWN }, 7, false },

    { StratagemID::ORBITAL_120HE,       "Orbital 120MM HE Barrage",
      { D::RIGHT, D::RIGHT, D::DOWN, D::LEFT, D::RIGHT, D::DOWN }, 6, false },

    { StratagemID::ORBITAL_EMS,         "Orbital EMS Strike",
      { D::RIGHT, D::RIGHT, D::LEFT, D::DOWN }, 4, false },

    { StratagemID::ORBITAL_GAS,         "Orbital Gas Strike",
      { D::RIGHT, D::RIGHT, D::DOWN, D::RIGHT }, 4, false },

    { StratagemID::ORBITAL_NAPALM,      "Orbital Napalm Barrage",
      { D::RIGHT, D::RIGHT, D::DOWN, D::LEFT, D::RIGHT, D::UP }, 6, false },

    { StratagemID::ORBITAL_WALKING,     "Orbital Walking Barrage",
      { D::RIGHT, D::DOWN, D::RIGHT, D::DOWN, D::RIGHT, D::DOWN }, 6, false },

    { StratagemID::ORBITAL_SMOKE,       "Orbital Smoke Strike",
      { D::RIGHT, D::RIGHT, D::DOWN, D::UP }, 4, false },

    // ── Mission ────────────────────────────────────────────────────────────
    { StratagemID::REINFORCE,           "Reinforce",
      { D::UP, D::DOWN, D::RIGHT, D::LEFT, D::UP }, 5, true },

    { StratagemID::RESUPPLY,            "Resupply",
      { D::DOWN, D::DOWN, D::RIGHT, D::UP }, 4, true },

    { StratagemID::SOS_BEACON,          "SOS Beacon",
      { D::UP, D::DOWN, D::RIGHT, D::UP }, 4, false },

    { StratagemID::EAGLE_REARM,         "Eagle Rearm",
      { D::UP, D::UP, D::LEFT, D::UP, D::RIGHT }, 5, false },

    { StratagemID::HELLBOMB,            "Hellbomb",
      { D::DOWN, D::UP, D::LEFT, D::DOWN, D::UP, D::RIGHT, D::DOWN, D::UP }, 8, false },

    { StratagemID::SUPER_EARTH_FLAG,    "Super Earth Flag",
      { D::DOWN, D::UP, D::DOWN, D::UP }, 4, false },

    { StratagemID::SSSD_DELIVERY,       "SSSD Delivery",
      { D::DOWN, D::DOWN, D::DOWN, D::UP, D::UP }, 5, false },

    { StratagemID::UPLOAD_DATA,         "Upload Data",
      { D::LEFT, D::RIGHT, D::UP, D::UP, D::UP }, 5, false },

    { StratagemID::SEAF_ARTILLERY,      "SEAF Artillery",
      { D::RIGHT, D::UP, D::UP, D::DOWN }, 4, false },
};

const int STRATAGEM_DB_SIZE = sizeof(STRATAGEM_DB) / sizeof(STRATAGEM_DB[0]);

// ─── Launch pool ─────────────────────────────────────────────────────────────
// Populated at runtime from stratagems with inLaunchPool == true
// The compiler can't initialize this statically from STRATAGEM_DB so we fill it lazily.
const StratagemDef* LAUNCH_POOL[LAUNCH_POOL_SIZE] = {};

static bool poolBuilt = false;

void ensureLaunchPoolBuilt() {
    if (poolBuilt) return;

    int idx = 0;
    for (int i = 0; i < STRATAGEM_DB_SIZE && idx < LAUNCH_POOL_SIZE; i++) {
        if (STRATAGEM_DB[i].inLaunchPool) {
            LAUNCH_POOL[idx++] = &STRATAGEM_DB[i];
        }
    }

    poolBuilt = true;
}

int getLaunchPoolCount() {
    ensureLaunchPoolBuilt();

    int count = 0;
    for (int i = 0; i < LAUNCH_POOL_SIZE; i++) {
        if (LAUNCH_POOL[i] != nullptr) {
            count++;
        }
    }
    return count;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
const StratagemDef* getStratagemById(uint8_t id) {
    for (int i = 0; i < STRATAGEM_DB_SIZE; i++) {
        if (STRATAGEM_DB[i].id == id) return &STRATAGEM_DB[i];
    }
    return nullptr;
}

bool isPrefixMatch(const Direction* input, uint8_t inputLen, const StratagemDef& def) {
    if (inputLen == 0 || inputLen > def.length) return false;
    for (uint8_t i = 0; i < inputLen; i++) {
        if (input[i] != def.sequence[i]) return false;
    }
    return true;
}

bool isFullMatch(const Direction* input, uint8_t inputLen, const StratagemDef& def) {
    if (inputLen != def.length) return false;
    return isPrefixMatch(input, inputLen, def);
}

const char* directionToArrow(Direction d) {
    switch (d) {
        case Direction::UP:    return "↑";
        case Direction::DOWN:  return "↓";
        case Direction::LEFT:  return "←";
        case Direction::RIGHT: return "→";
        default:               return "?";
    }
}
