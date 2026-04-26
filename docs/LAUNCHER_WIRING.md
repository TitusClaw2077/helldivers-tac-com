# Launcher Wiring

## Purpose

This document is the launcher-side wiring source of truth for the **current validated simplified bench rig**.

It is intentionally centered on what has already been proven on the bench:
- physical SAFE/ARM interlock behavior
- launcher arm-sense input
- continuity OPEN/PRESENT sensing on the current rig
- MOSFET-switched safe dummy-load pulse on `GPIO26`
- launcher controller operation when powered from the planned buck converter

It is **not** a claim that the launcher is already wired as a final field-ready igniter assembly.

For bench-stage detail and test notes, also read:
- `projects/helldivers-tac-com/CONTINUITY_BENCH_SETUP.md`
- `projects/helldivers-tac-com/MOSFET_DUMMY_LOAD_BENCH_SETUP.md`
- `projects/helldivers-tac-com/BUCK_CONVERTER_BENCH_PREP.md`

---

## Current Status

### Validated now
The current launcher bench rig has already validated:
- physical SAFE blocks remote arm
- physical ARM allows remote arm
- remote DISARM works
- returning physical switch to SAFE forces disarm
- continuity classification on the current rig:
  - `OPEN ~= 242-243`
  - `PRESENT ~= 2996-2999`
  - one transient enable-time `UNKNOWN` is acceptable for now
- accepted FIRE uses `GPIO26` to drive the MOSFET-switched safe dummy load
- continuity sampling is suppressed during the active fire pulse
- launcher-side pending ARM/FIRE command handling was hardened after the earlier race bug
- buck-only controller-power retest passed on the same safe dummy-load rig

### Explicitly not validated yet
These are still out of scope for this document slice:
- real igniter/e-match hookup
- banana-clip live firing path
- final launcher enclosure/perfboard layout
- near-real-current live launch validation
- field-ready wiring approval or range suitability

---

## Current Bench Hardware In Circuit

### Controller and control path
- ESP32-WROOM-32 launcher controller
- physical ARM/SAFE switch
- arm-sense divider feeding `GPIO27`
- ESP-NOW command path from wrist unit

### Continuity path
- continuity network built around:
  - `ARMED_POS`
  - `IGN_NEG`
  - `CONT_NODE`
  - `ADC_SENSE`
  - `GPIO34`
- bench-only divider/reference arrangement validated on the current rig

### Switched load path
- `IRLZ44N` low-side MOSFET
- `GPIO26 -> 330 ohm -> gate`
- `10k` gate pulldown to ground
- safe `10 ohm` dummy load between `ARMED_POS` and `IGN_NEG`
- `2S` launcher load rail

### Controller power path
- same `2S` battery feeding a buck converter
- buck output feeding launcher ESP32 `5V/VIN`
- common ground shared with launcher ground / MOSFET source / battery negative

---

## Current Bench Hardware Not In Circuit

These should be treated as future work, not implied current wiring:
- real igniter or e-match
- final banana-clip launch leads in active use
- final high-current launcher harness
- final enclosure wiring / terminal-block layout
- any claim that the current breadboard-style rig is ready for live firing

---

## Node Map

Use these names consistently across docs and bench notes.

| Node | Meaning | Current role |
|------|---------|--------------|
| `ARMED_POS` | Positive launcher rail after fuse and ARM switch | Positive side of the validated dummy-load and continuity rig |
| `IGN_NEG` | Negative launcher output node / MOSFET drain-side node | Negative side of the validated dummy-load and continuity rig |
| `CONT_NODE` | Raw continuity sense node before the safer ADC divider | Internal continuity network node |
| `ADC_SENSE` | Divided continuity node sent toward ADC | Safe ADC feed for `GPIO34` |
| `GPIO26` | Launcher accepted-fire output | Drives MOSFET gate through `330 ohm` |
| `GPIO27` | Physical ARM sense input | Reads physical ARM/SAFE state through divider |
| `GPIO34` | Continuity ADC input | Reads `ADC_SENSE` |
| `VIN+ / VIN-` | Buck converter input from launcher battery | Controller power input branch |
| `5V/VIN` | ESP32 dev-board power input | Buck-regulated controller supply |

---

## Wiring By Function

## 1. Safety / interlock path

This is the physical permission path.

### Current validated path
```text
2S battery positive -> fuse -> ARM/SAFE switch common
ARM/SAFE switched output -> ARMED_POS
```

### Current behavior
- switch in SAFE: positive launch rail is not enabled, and launcher must reject remote arm
- switch in ARM: launcher may accept remote arm if other logic conditions are satisfied
- switching back to SAFE must force disarm

### Arm-sense input
```text
ARM/SAFE switched output -> divider -> GPIO27
```

This digital sense path is already bench-validated as the firmware-visible copy of the physical interlock state.

---

## 2. Continuity path

This is the **bench-validated continuity topology** for the current rig.
It is more specific than the older first-pass continuity description and should take priority.

### Current validated continuity wiring
```text
ARMED_POS -> 100k -> CONT_NODE -> 4.7k -> IGN_NEG
IGN_NEG   -> 10k  -> GND
CONT_NODE -> 100k -> ADC_SENSE -> GPIO34
ADC_SENSE -> 47k  -> GND
ADC_SENSE -> 0.1uF -> GND      (optional)
ADC_SENSE -> 1k -> GPIO34      (optional series resistor)
```

### Dummy continuity element used for PRESENT validation
```text
ARMED_POS -> 10 ohm dummy load -> IGN_NEG
```

### Important note
Older wording that implied a simpler direct continuity feed into `GPIO34` should be treated as stale.
The current authoritative bench rig uses the safer divider/reference arrangement documented above and in `projects/helldivers-tac-com/CONTINUITY_BENCH_SETUP.md`.

### Current observed readings
- `OPEN ~= 242-243`
- `PRESENT ~= 2996-2999`
- one transient enable-time `UNKNOWN` is acceptable for now

### Current firmware interpretation
- continuity monitoring is only meaningful while physically armed
- low readings classify `OPEN`
- high readings classify `PRESENT`
- mid-band readings classify `UNKNOWN`
- continuity sampling is suppressed during the active accepted fire pulse so the launcher does not falsely report `OPEN` mid-pulse

---

## 3. Switched load path

This is the currently validated **safe dummy-load** path, not a live igniter path.

### Current validated switched path
```text
GPIO26 -> 330 ohm -> IRLZ44N gate
IRLZ44N gate -> 10k -> GND
IRLZ44N source -> battery negative / common ground
IRLZ44N drain -> IGN_NEG
ARMED_POS -> 10 ohm dummy load -> IGN_NEG
```

### Current accepted-fire behavior
- launcher accepts `FIRE_CMD` only after the normal interlock/state checks
- `GPIO26` pulses HIGH for the configured fire window
- MOSFET switches the safe dummy load
- launcher transitions through accepted firing flow and returns cleanly afterward

### Bench evidence already captured
- SAFE switch OFF: load measured `0V`
- ARM switch ON, idle: load measured about `9 mV`
- accepted FIRE on earlier USB-powered pass: about `6.0-6.2V` across the `10 ohm` dummy load
- accepted FIRE on repeated retest: about `5.94V`
- accepted FIRE on authoritative buck-only run: about `5.5V`

### Important scope note
This proves the current launcher can switch a **safe resistive load** through the intended architecture.
It does **not** yet prove a live igniter path.

---

## 4. Controller power path

This is the currently validated controller-power branch.

### Current validated path
```text
2S battery -> buck converter VIN+/VIN-
buck output -> ESP32 dev board 5V/VIN
common ground shared with battery negative / MOSFET source / launcher ground
```

### Authoritative result
The authoritative controller-power checkpoint is the **buck-only run** with USB-C removed from the ESP32.

That run passed with:
- stable boot
- sane continuity settle behavior
- accepted ARM
- accepted FIRE
- pulse complete
- accepted `FIRE_ACK`
- continued post-fire heartbeats

### Non-authoritative mixed-power note
A later mixed buck+USB check was tried, but it should not be used as the source of truth because power-source priority in that setup was ambiguous.

---

## Point-to-Point Summary For The Current Validated Rig

## Keep as currently validated
- `PIN_ARM_SENSE` path to `GPIO27`
- continuity network around `ARMED_POS`, `IGN_NEG`, `CONT_NODE`, `ADC_SENSE`, `GPIO34`
- accepted-fire output on `GPIO26`
- `IRLZ44N` low-side switch
- `10 ohm` dummy load across `ARMED_POS` and `IGN_NEG`
- buck-powered ESP32 controller supply through `5V/VIN`

## Current point-to-point summary
1. `2S positive -> fuse -> ARM/SAFE switch common`
2. `ARM/SAFE switch output -> ARMED_POS`
3. `ARM/SAFE switch output -> arm-sense divider -> GPIO27`
4. `ARMED_POS -> 100k -> CONT_NODE`
5. `CONT_NODE -> 4.7k -> IGN_NEG`
6. `IGN_NEG -> 10k -> GND`
7. `CONT_NODE -> 100k -> ADC_SENSE`
8. `ADC_SENSE -> 47k -> GND`
9. `ADC_SENSE -> GPIO34` (optional `1k` series, optional `0.1uF` filter to ground)
10. `GPIO26 -> 330 ohm -> IRLZ44N gate`
11. `IRLZ44N gate -> 10k -> GND`
12. `IRLZ44N source -> common ground / battery negative`
13. `IRLZ44N drain -> IGN_NEG`
14. `10 ohm dummy load -> between ARMED_POS and IGN_NEG`
15. `2S battery -> buck converter -> ESP32 5V/VIN`
16. `buck ground / ESP32 ground / battery negative / MOSFET source all tied to common ground`

---

## What This Document Deliberately Avoids

To keep project truth clean, this document does **not** assume:
- active banana-clip live launch wiring
- final perfboard or enclosure packaging
- final fuse sizing for real igniter use
- final field harness current ratings
- live igniter continuity/current behavior

Those belong in a later document revision after a deliberate higher-risk slice is chosen.

---

## Bench Reality vs Later Build

### Bench reality now
The current launcher is best understood as:
- a validated simplified bench controller
- with real physical interlock behavior
- with calibrated continuity sensing on the current rig
- with a real MOSFET-switched safe dummy load
- with a validated buck-powered controller supply

### Later build work
Later launcher documentation can add:
- final igniter output connector scheme
- final launch harness layout
- live igniter/e-match procedure and controls
- final mechanical packaging
- any additional protective circuitry justified by later testing

---

## Bottom Line

The current source of truth is:
- physical interlock path is real and validated
- continuity path is the **bench-validated divided topology**, not the older simpler wording
- accepted fire currently means **safe dummy-load switching on `GPIO26` through the `IRLZ44N`**
- controller power currently means **buck-regulated ESP32 power**, with the **buck-only run** as the authoritative checkpoint
- igniter/live firing work remains out of scope until a later deliberate slice says otherwise
