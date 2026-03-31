#pragma once
#include <stdint.h>
#include "protocol_types.h"
#include "config_shared.h"

// ─── Stratagem definition ────────────────────────────────────────────────────
struct StratagemDef {
    uint8_t         id;
    const char*     name;
    Direction       sequence[MAX_STRATAGEM_LEN];
    uint8_t         length;
    bool            inLaunchPool;   // eligible for random launch selection
};

// ─── Active stratagem selection ──────────────────────────────────────────────
struct ActiveStratagem {
    int                     poolIndex;  // index into LAUNCH_POOL (or -1 if none)
    const StratagemDef*     def;
    uint32_t                selectedAtMs;
};

// ─── Stratagem IDs ───────────────────────────────────────────────────────────
namespace StratagemID {
    // Eagle
    constexpr uint8_t EAGLE_AIRSTRIKE       = 1;
    constexpr uint8_t EAGLE_500KG           = 2;
    constexpr uint8_t EAGLE_STRAFING_RUN    = 3;
    constexpr uint8_t EAGLE_110MM           = 4;
    constexpr uint8_t EAGLE_NAPALM          = 5;
    constexpr uint8_t EAGLE_CLUSTER         = 6;
    constexpr uint8_t EAGLE_SMOKE           = 7;
    // Orbital
    constexpr uint8_t ORBITAL_PRECISION     = 10;
    constexpr uint8_t ORBITAL_AIRBURST      = 11;
    constexpr uint8_t ORBITAL_RAILCANNON    = 12;
    constexpr uint8_t ORBITAL_LASER         = 13;
    constexpr uint8_t ORBITAL_GATLING       = 14;
    constexpr uint8_t ORBITAL_380HE         = 15;
    constexpr uint8_t ORBITAL_120HE         = 16;
    constexpr uint8_t ORBITAL_EMS           = 17;
    constexpr uint8_t ORBITAL_GAS           = 18;
    constexpr uint8_t ORBITAL_NAPALM        = 19;
    constexpr uint8_t ORBITAL_WALKING       = 20;
    constexpr uint8_t ORBITAL_SMOKE         = 21;
    // Mission
    constexpr uint8_t REINFORCE             = 30;
    constexpr uint8_t RESUPPLY              = 31;
    constexpr uint8_t SOS_BEACON            = 32;
    constexpr uint8_t EAGLE_REARM           = 33;
    constexpr uint8_t HELLBOMB              = 34;
    constexpr uint8_t SUPER_EARTH_FLAG      = 35;
    constexpr uint8_t SSSD_DELIVERY         = 36;
    constexpr uint8_t UPLOAD_DATA           = 37;
    constexpr uint8_t SEAF_ARTILLERY        = 38;
}

// ─── Full stratagem database (defined in lib/common/stratagems.cpp) ───────────
extern const StratagemDef STRATAGEM_DB[];
extern const int STRATAGEM_DB_SIZE;

// ─── Launch pool (random selection from this set) ────────────────────────────
extern const StratagemDef* LAUNCH_POOL[LAUNCH_POOL_SIZE];

// ─── Helpers ─────────────────────────────────────────────────────────────────
const StratagemDef* getStratagemById(uint8_t id);
bool isPrefixMatch(const Direction* input, uint8_t inputLen, const StratagemDef& def);
bool isFullMatch(const Direction* input, uint8_t inputLen, const StratagemDef& def);
const char* directionToArrow(Direction d);  // returns "↑" "↓" "←" "→"
