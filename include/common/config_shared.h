#pragma once
#include <stdint.h>

// ─── Protocol ────────────────────────────────────────────────────────────────
#define PROTOCOL_VERSION        1
#define PACKET_MAGIC            0xA7

// ─── Timing ──────────────────────────────────────────────────────────────────
#define HEARTBEAT_INTERVAL_MS       2000    // wrist → launcher ping rate
#define LAUNCHER_TIMEOUT_MS         5000    // wrist marks launcher offline after this
#define STRATAGEM_INPUT_TIMEOUT_MS  3000    // clears entered sequence on inactivity
#define POST_MATCH_LOCKOUT_MS       2000    // delay between match and confirm prompt
#define CONTINUITY_CHECK_INTERVAL_MS 500   // how often launcher samples continuity
#define STATUS_BROADCAST_INTERVAL_MS 1000  // launcher periodic status push

// ─── Ignition ────────────────────────────────────────────────────────────────
#define IGNITION_PULSE_DURATION_MS  1000    // default MOSFET on-time
#define POST_FIRE_COOLDOWN_MS       2000    // time in FIRED state before DISARMED

// ─── Stratagem input ─────────────────────────────────────────────────────────
#define MAX_STRATAGEM_LEN           8       // maximum arrow sequence length
#define LAUNCH_POOL_SIZE            5       // number of stratagems in default launch pool

// ─── Battery ─────────────────────────────────────────────────────────────────
#define BATTERY_LOW_PCT             20
#define BATTERY_CRITICAL_PCT        10
#define BATTERY_SAMPLE_INTERVAL_MS  3000

// ─── Hardware — Launcher GPIO ─────────────────────────────────────────────────
#define PIN_IGNITION_GATE       26      // MOSFET gate output
#define PIN_ARM_SENSE           27      // DaierTek switch sense input (divided)
#define PIN_CONTINUITY_ADC      34      // ADC-only input for continuity check
#define PIN_BATT_ADC            35      // ADC-only input for battery voltage divider

// ─── Safety ──────────────────────────────────────────────────────────────────
// MAC address of the paired wrist unit — fill in at build time
// Example: { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
#define WRIST_MAC   { 0x40, 0xf5, 0x20, 0x8f, 0x32, 0x40 }   // real MAC set
#define LAUNCHER_MAC   { 0xf8, 0xb3, 0xb7, 0x44, 0x6e, 0x7c }   // actual launcher STA MAC