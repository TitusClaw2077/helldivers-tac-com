# Helldivers Tac-Com Bracelet — Project Specification
*Last updated: 2026-03-30*

---

## Overview

A wearable ESP32-based bracelet that replicates the Helldivers 2 stratagem input experience on a physical touchscreen. The wrist unit communicates with a secondary ESP32 launcher controller that ignites standard low-power hobby rocket igniters for A-E motors. Instead of one fixed launch code, the active launch stratagem can be randomized from an approved pool each cycle to make the tac-com interaction feel more like the game.

---

## Hardware Summary

### Wrist Unit
| Component | Detail |
|-----------|--------|
| MCU / Display | Elecrow CrowPanel ESP32 3.5" (ASIN: B0FXLB5CFL) |
| Chip | ESP32-WROVER-B, dual-core LX6 @ 240MHz |
| Display | 3.5" TFT LCD, 480×320, resistive touch (ILI9488) |
| Wireless | WiFi + BT (ESP-NOW for launcher comms) |
| Ports | USB-C, TF card, battery, speaker, GPIO breakout |
| Frameworks | PlatformIO + Arduino framework (primary); LVGL for UI |
| Housing | 3D printed wrist enclosure (reference images available; CAD optional for now) |
| Power | 1S LiPo battery, 1000mAh target for ~3-4 hours runtime |

### Launcher / Igniter Unit
| Component | Detail |
|-----------|--------|
| MCU | ESP32-WROOM-32 (bare module) |
| Wireless | WiFi + BT (ESP-NOW, receives from wrist unit) |
| Trigger GPIO | GPIO26 recommended (no boot constraints, PWM capable) |
| Igniter circuit | N-channel MOSFET switching standard hobby igniters via banana clip leads |
| Power | Dedicated 2S LiPo (7.4V nominal, 8.4V full) for igniter rail |
| Igniter target | Standard low-power hobby rocket igniters included with A-E motors |

---

## Comms Architecture

- **Protocol:** ESP-NOW (peer-to-peer, no router, ~1ms latency, ~200m range open air)
- **Direction:** Bidirectional
  - Wrist → Launcher: arm command, fire command
  - Launcher → Wrist: status (online/armed/disarmed/fault/fired)
- **Pairing:** MAC address hardcoded at build time (one wrist unit : one launcher)

---

## Safety System (NON-NEGOTIABLE)

This is a wireless rocket ignition system. The following safety layers are required before any live ignition testing.

### Arm/Disarm Flow
1. Launcher powers on → **DISARMED** state by default
2. Wrist unit sends ARM request
3. Launcher ACKs: transitions to **ARMED** state
4. Wrist UI reflects ARM status with clear visual indicator
5. DaierTek missile-cover toggle switch on launcher required to enable ARM acceptance (hardware interlock — launcher cannot be armed remotely without physical switch flipped to ARM position)

### Fire Sequence (3-step, no shortcuts)
1. User completes valid stratagem input on wrist
2. Wrist UI shows "CONFIRM LAUNCH?" — user must explicitly confirm (separate tap)
3. Wrist sends fire command → Launcher ACKs → Ignition fires
4. Launcher reports "FIRED" or "FAULT" back to wrist

### Accidental Input Protection
- Stratagem input panel is only active when launcher status shows ARMED
- Minimum 2-second lockout between stratagem completion and confirmation prompt
- Misfire protection: if comms lost mid-sequence, launcher stays in current state (does NOT fire)

### Range Safety Compliance
- Review club/NAR/TRA requirements for remote ignition systems before field use
- Physical key switch on launcher = range officer control point
- DaierTek covered toggle's illuminated LED = visual "hot/cold" range indicator when armed

---

## Wrist Unit — Software Deliverables

### 1. Stratagem Input UI (LVGL)
The core game interface. Replicates the Helldivers 2 tac-com experience.

**Input system:**
- 4-directional swipe or tap input: ↑ ↓ ← →
- Visual arrow sequence display (inputs shown as they're entered)
- Sequence recognition engine matching against stratagem database
- Timeout/reset: sequence clears after ~3 seconds of inactivity (matches HD2 behavior)
- Visual + audio feedback on match (match flash, name display)
- Wrong input feedback (red flash, sequence reset)

**Stratagem database (stored in firmware):**
For this build, launch is tied to a **randomly selected approved stratagem** rather than one fixed code. On each arm/launch cycle, the wrist unit picks one stratagem from a curated pool and displays it as the active launch command.

**Initial approved launch pool:**
| Stratagem | Sequence | Notes |
|-----------|----------|-------|
| Eagle Airstrike | ↑ → ↓ → | Short, iconic, good baseline |
| Orbital Precision Strike | → → ↓ → | Easy alternate |
| Resupply | ↓ ↓ → ↑ | Clean 4-step input |
| Eagle 110mm Rocket Pods | ↑ → ↑ ← | Thematically strong |
| Reinforce | ↑ ↓ → ← ↑ | Longer code for optional harder mode |

**Randomization rules:**
- Choose one active launch stratagem when the launcher is armed
- Display the selected stratagem name and arrows clearly on the wrist UI
- Optional settings later: easy pool / full pool / fixed favorite

*Full stratagem list available below for future UI expansion.*

### 2. Launcher Status Panel
Always-visible status bar or sidebar:
- Launcher: ONLINE / OFFLINE
- State: ARMED / DISARMED
- Last event: FIRED / FAULT / READY
- Signal quality indicator

### 3. ESP-NOW Comms Layer
- Peer registration on boot
- Heartbeat ping to launcher every 2 seconds
- Status receive handler (updates UI in real-time)
- Arm/Disarm command transmit
- Fire command transmit (only when ARMED + confirmed)
- Timeout handling: if no heartbeat response in 5s → OFFLINE state, block fire

### 4. Power Management
- 1S LiPo via onboard battery port on Elecrow board
- 1000mAh target capacity for ~3-4 hours active runtime
- Battery level indicator in UI (ADC read from battery voltage divider)
- Low battery warning at 20%, critical at 10%
- Deep sleep option when idle > X minutes (TBD)

### 5. Expandable UI Framework
Screen/tab architecture using LVGL:
- **Screen 0:** Stratagem input (main screen)
- **Screen 1:** Launcher status / diagnostics
- **Screen 2:** [Future] GPS tracking
- **Screen 3:** [Future] Flight telemetry / altimeter data
- **Screen 4:** [Future] Settings

New screens can be added without touching core stratagem or comms logic.

---

## Launcher Unit — Software Deliverables

### 1. ESP-NOW Receive Handler
- Listen for arm/disarm/fire commands from wrist unit
- Validate source MAC address before acting on any command
- Respond to heartbeat pings with current status

### 2. State Machine
```
BOOT → DISARMED
DISARMED + arm_cmd + key_switch_ON → ARMED
ARMED + disarm_cmd OR key_switch_OFF → DISARMED
ARMED + fire_cmd → FIRING → FIRED/FAULT → DISARMED
```

### 3. Ignition Circuit Control
- GPIO26 drives MOSFET gate
- Ignition pulse: configurable duration (default 1 second)
- Continuity check: ADC read across e-match before fire to confirm circuit integrity
- Fault: if continuity check fails, report FAULT to wrist, do NOT fire

### 4. Status Broadcast
- Respond to wrist heartbeat with: state, continuity, last event, battery level

---

## Launcher Igniter Circuit (Schematic — TBD)

**Key components needed:**
- ESP32-WROOM-32
- N-channel MOSFET: IRLZ44N or similar logic-level MOSFET driven by 3.3V GPIO
- Gate resistor: 330Ω (protects GPIO from inrush)
- Ignition power rail: dedicated 2S LiPo
- Banana clip launch leads for standard hobby igniters
- Fuse: on ignition power rail (size TBD)
- Arming switch: DaierTek lighted covered toggle switch (ASIN B07T6XJF1T) used as arm/safe interlock, not as the primary high-current switching element
- Continuity test resistor: small current path for ADC continuity check

*Full KiCad/hand-drawn schematic to be produced as a deliverable.*

---

## Implementation & Testing Plan

### Phase 0 — Research & Design (Now)
- [x] Confirm wrist unit hardware (Elecrow CrowPanel)
- [x] Confirm launcher MCU (ESP32-WROOM-32)
- [x] Confirm igniter target: standard hobby igniters for A-E motors
- [x] Confirm 2S LiPo on launcher side
- [x] Confirm 1000mAh target on wrist side
- [x] Decide random active launch stratagem from approved pool
- [ ] Review 3D housing dimensions later during fitment phase
- [ ] Review club/field practices for remote ignition before live use

### Phase 1 — Bench: Comms
- [ ] Flash basic ESP-NOW sketch to both ESP32s
- [ ] Confirm bidirectional messaging (wrist → launcher, launcher → wrist)
- [ ] LED on launcher = "received fire command" (no actual ignition yet)
- [ ] Test heartbeat and status reporting loop

### Phase 2 — Bench: Igniter Circuit
- [ ] Build MOSFET circuit on breadboard
- [ ] Test with resistive load (DO NOT use e-match until Phase 4)
- [ ] Verify GPIO26 drives IRLZ44N cleanly at 3.3V
- [ ] Verify continuity ADC read works
- [ ] Verify key switch interlock behavior

### Phase 3 — Bench: Wrist UI
- [ ] LVGL dev environment set up (PlatformIO)
- [ ] Stratagem input UI: 4-direction input working
- [ ] Random active stratagem selection implemented
- [ ] Sequence recognition against approved launch pool
- [ ] Confirm sequence → comms → launcher response (LED)
- [ ] Status panel: launcher online/armed/disarmed + active stratagem displayed

### Phase 4 — Bench: Full Integration
- [ ] Combine Phase 1-3
- [ ] Full arm → stratagem → confirm → fire sequence (resistive load)
- [ ] Safety system validation: test every failure mode
  - Comms lost mid-sequence
  - Key switch OFF during arm attempt
  - Wrong stratagem input
  - Double-tap confirm required

### Phase 5 — Field: Comms Range Test
- [ ] Open field test
- [ ] Confirm ESP-NOW range is sufficient for launch distance
- [ ] Test heartbeat reliability at range

### Phase 6 — Field: Live E-Match Test (No Rocket)
- [ ] E-match wired to circuit, no motor attached
- [ ] Full sequence from wrist → confirmed ignition of e-match
- [ ] Validate safety system under real conditions

### Phase 7 — Full Integration Launch
- [ ] Rocket on pad, e-match installed
- [ ] Full tac-com sequence → real launch
- [ ] 🎉

---

## Open Questions / Decisions Needed

| # | Question | Owner | Status | Notes |
|---|----------|-------|--------|-------|
| 1 | Which e-match brand/model? | David | ✅ Decided | Standard hobby igniters included with A-E engines (e.g. Estes). Banana clip connectors on launcher wires. ~1-2Ω, fire at ~3-5A. |
| 2 | Ignition power rail voltage | David | ✅ Decided | 2S LiPo (7.4V nominal, 8.4V charged) — sufficient for standard hobby igniters used with A-E motors and lightweight/rechargeable |
| 3 | LiPo capacity for wrist unit | David | ✅ Decided | 1000mAh recommended for ~3-4 hours runtime; 500mAh likely too short |
| 4 | 3D housing file for wrist unit | David | 🟡 Optional | Not required for firmware work now; useful later for fitment and mounting details |
| 5 | Launch trigger stratagem | David | ✅ Decided | Randomized from an approved stratagem pool each launch cycle for added fun |
| 6 | Physical key switch type for launcher | David | ✅ Decided | DaierTek 12V LED lighted toggle switch with aircraft safety cover (Amazon ASIN B07T6XJF1T); adequate as arming switch since launcher current is low and MOSFET handles igniter load |
| 7 | Club range rules for remote ignition | David | ✅ Tentative | Low-power rockets should be fine, but still confirm local field/club practices before live use |
| 8 | Development framework | David | ✅ Decided | PlatformIO + Arduino framework recommended for easiest ESP32/LVGL workflow |

---

## Full Stratagem Reference (Helldivers 2)

For future UI expansion. Current plan: launch trigger is chosen randomly from the approved launch pool, not fixed to a single stratagem.

### Orbital
| Stratagem | Sequence |
|-----------|----------|
| Airburst Strike | → → → |
| Smoke Strike | → → ↓ ↑ |
| Gas Strike | → → ↓ → |
| Precision Strike | → → ↓ → |
| Railcannon Strike | → ↑ ↓ ↓ → |
| Gatling Barrage | → ↓ ← ↑ ↑ |
| Napalm Barrage | → → ↓ ← → ↑ |
| EMS Strike | → → ← ↓ |
| 120MM HE Barrage | → → ↓ ← → ↓ |
| 380MM HE Barrage | → ↓ ↑ ↑ ← ↓ ↓ |
| Walking Barrage | → ↓ → ↓ → ↓ |
| Laser | → ↓ ↑ → ↓ |

### Eagle
| Stratagem | Sequence |
|-----------|----------|
| Strafing Run | ↑ → → |
| Airstrike | ↑ → ↓ → |
| Napalm Airstrike | ↑ → ↓ ↑ |
| Smoke Strike | ↑ → ↑ ↓ |
| Cluster Bomb | ↑ → ↓ ↓ → |
| 110mm Rocket Pods | ↑ → ↑ ← |
| 500kg Bomb | ↑ → ↓ ↓ ↓ |

### Support Weapons
| Stratagem | Sequence |
|-----------|----------|
| Machine Gun | ↓ ← ↓ ↑ → |
| Laser Cannon | ↓ ← ↓ ↑ ← |
| Autocannon | ↓ ← ↓ ↑ ↑ → |
| Recoilless Rifle | ↓ ← → → ← |
| EAT-17 | ↓ ↓ ← ↑ → |
| Quasar Cannon | ↓ ↓ ↑ ← → |
| Arc Thrower | ↓ → ↓ ↑ ← ← |
| Railgun | ↓ → ↓ ↑ ← → |
| Grenade Launcher | ↓ ← ↑ ← ↓ |
| Flamethrower | ↓ ← ↑ ↓ ↑ |

### Defensive
| Stratagem | Sequence |
|-----------|----------|
| Anti-Personnel Minefield | ↓ ← ↑ → |
| Gatling Sentry | ↓ ↑ → ← |
| Mortar Sentry | ↓ ↑ → → ↓ |
| Machine Gun Sentry | ↓ ↑ → → ↑ |
| Tesla Tower | ↓ ↑ → ↑ ← → |
| Autocannon Sentry | ↓ ↑ → ↑ ← ↑ |
| Rocket Sentry | ↓ ↑ → → ← |
| EMS Mortar Sentry | ↓ ↑ → ↓ → |
| Shield Generator Relay | ↓ ↓ ← → ← → |
| Incendiary Mines | ↓ ← ← ↓ |
| Anti-Tank Mines | ↓ ← ↑ ↑ |

### Mission
| Stratagem | Sequence |
|-----------|----------|
| Reinforce | ↑ ↓ → ← ↑ |
| Resupply | ↓ ↓ → ↑ |
| SOS Beacon | ↑ ↓ → ↑ |
| Eagle Rearm | ↑ ↑ ← ↑ → |
| Hellbomb | ↓ ↑ ← ↓ ↑ → ↓ ↑ |
| Super Earth Flag | ↓ ↑ ↓ ↑ |
| SSSD Delivery | ↓ ↓ ↓ ↑ ↑ |
| Upload Data | ← → ↑ ↑ ↑ |
| SEAF Artillery | → ↑ ↑ ↓ |
