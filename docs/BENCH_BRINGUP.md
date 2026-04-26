# Bench Bring-Up Guide

This document is the practical bring-up and re-validation sequence for the **current validated simplified Helldivers Tac-Com bench rig**.

It reflects the project state **after**:
- ESP-NOW comms validation
- remote ARM / DISARM and SAFE interlock validation
- continuity OPEN / PRESENT validation on the current rig
- wrist diagnostics UI checkpoint
- LED checkpoint on `GPIO25`
- MOSFET dummy-load switching checkpoint on `GPIO26`
- continuity suppression during the active pulse
- launcher pending-command race fix
- buck-converter-powered controller retest on the same safe dummy-load rig

If this doc conflicts with older thread history, trust the current repo docs.

Related docs:
- `docs/LAUNCHER_WIRING.md`
- `projects/helldivers-tac-com/CONTINUITY_BENCH_SETUP.md`
- `projects/helldivers-tac-com/MOSFET_DUMMY_LOAD_BENCH_SETUP.md`
- `projects/helldivers-tac-com/BUCK_CONVERTER_BENCH_PREP.md`

---

## What is verified right now

Both PlatformIO environments currently build on the VM:

- `pio run -e wrist` ✅
- `pio run -e launcher` ✅

The current repo is already proven far beyond first radio bring-up.

### Current verified launcher-side behavior
- ESP-NOW wrist/launcher comms work
- launcher/wrist peer MAC mismatch was found and fixed earlier
- SAFE blocks remote ARM
- physical ARM allows remote ARM
- remote DISARM works
- returning the switch to SAFE forces disarm
- continuity classification on the current bench rig is calibrated and working
- accepted FIRE uses the real `GPIO26` -> MOSFET path on a safe dummy load
- continuity sampling is suppressed during the active fire pulse
- launcher command handling was hardened after the post-boot ARM/FIRE race issue
- launcher controller operation was revalidated on buck power without USB as the authoritative result

### Current verified wrist-side behavior
- diagnostics UI scaffold is active in the current repo
- on-screen ARM / DISARM controls work on hardware
- temporary FIRE control exists for bench work
- extra FIRE taps are ignored while a fire command is already in flight
- current CrowPanel unit works with the **v2.2** touch/read pin profile

---

## Current rig definition

This guide assumes the **current validated simplified bench rig**, not a final launcher build.

### In circuit now
- launcher ESP32
- wrist ESP32/CrowPanel
- physical ARM/SAFE switch
- arm-sense divider into `GPIO27`
- continuity network around:
  - `ARMED_POS`
  - `IGN_NEG`
  - `CONT_NODE`
  - `ADC_SENSE`
  - `GPIO34`
- `IRLZ44N` low-side MOSFET
- `GPIO26 -> 330 ohm -> gate`
- `10k` gate pulldown
- `10 ohm` dummy load across `ARMED_POS` and `IGN_NEG`
- `2S` launcher battery / load rail
- buck converter feeding ESP32 `5V/VIN` for the current controller-power checkpoint

### Not in circuit now
- real igniter/e-match
- active banana-clip live launch wiring
- final launcher harness / field packaging

---

## Safety setup for this bench session

For this bench rig:
- **do not attach a real igniter**
- keep the dummy load as the only switched load
- start with the physical ARM/SAFE switch in **SAFE**
- verify the MOSFET gate pulldown is present before power-up
- verify common ground between:
  - battery negative
  - MOSFET source
  - buck ground
  - ESP32 ground
- keep the inline fuse on the launcher positive rail
- treat the rig as electrically live whenever the battery is connected, even though this is still the safe dummy-load stage

---

## Build and upload sequence

```bash
cd ~/.openclaw/workspace/helldivers-tac-com
```

### Clean build confirmation

```bash
pio run -e wrist
pio run -e launcher
```

### Upload launcher

```bash
pio run -e launcher --target upload
```

### Upload wrist

```bash
pio run -e wrist --target upload
```

If PlatformIO selects the wrong serial port, choose the correct port explicitly before upload.

---

## Serial monitor workflow

Use `115200` baud.

```bash
pio device monitor -b 115200
```

Recommended observation order:
1. launcher by itself
2. wrist by itself
3. both powered together
4. launcher during SAFE/ARM and accepted FIRE bench checks

---

## Expected current bring-up checkpoints

## 1. Boot and peer registration

### Launcher should show the equivalent of
```text
[LAUNCHER] Boot
[LAUNCHER/radio] Wrist peer registered: <wrist-mac>
[LAUNCHER] Ready
```

### Wrist should show the equivalent of
```text
[WRIST] Boot
[WRIST/link] Launcher peer registered: <launcher-mac>
[WRIST] Ready
```

Once both are online, heartbeats/status packets should resume normally.

---

## 2. SAFE / ARM interlock check

### Start condition
- both boards powered
- status/heartbeat traffic is alive
- launcher switch in SAFE

### SAFE expectation
- wrist status reflects `key=0`
- remote ARM should be rejected
- continuity should not be treated as active fire-permission input while SAFE on this rig

### Flip to ARM
Expected:
- launcher senses the switch transition
- wrist status reflects `key=1`
- remote ARM can now be accepted

### Flip back to SAFE
Expected:
- launcher forces disarm
- wrist status returns to SAFE indication

---

## 3. Continuity check on the current rig

This is already calibrated on the real bench setup.

### OPEN case
With nothing bridging `ARMED_POS` to `IGN_NEG`, the launcher should classify continuity as:
- `OPEN`
- raw about `242-243`

### PRESENT case
With the validated `10 ohm` dummy load between `ARMED_POS` and `IGN_NEG`, the launcher should classify continuity as:
- `PRESENT`
- raw about `2996-2999`

### Acceptable quirk
One transient enable-time `UNKNOWN` is acceptable on the current rig before it settles.

---

## 4. Accepted FIRE dummy-load check

This is the current launcher-side fire-path checkpoint.

### Preconditions
- dummy load connected
- physical switch in ARM
- launcher remotely armed
- continuity reports `PRESENT`

### Accepted case expectation
- launcher accepts `FIRE_CMD`
- `GPIO26` pulses the MOSFET gate
- dummy load is switched only during the accepted pulse
- launcher transitions through firing flow cleanly
- `FIRE_ACK` is sent
- launcher returns cleanly afterward

### Bench measurements already established
- SAFE OFF case: load measured `0V`
- ARM idle case: load measured about `9 mV`
- accepted FIRE, earlier pass: about `6.0-6.2V`
- accepted FIRE, later retest: about `5.94V`
- accepted FIRE, authoritative buck-only run: about `5.5V`

### Important continuity behavior
During the current validated firmware/runtime:
- continuity sampling is suppressed during the active fire pulse
- the launcher should no longer report the earlier misleading mid-pulse `OPEN`

---

## 5. Buck-powered controller revalidation

The current authoritative controller-power result is the **buck-only** run.

### Expected result
- ESP32 boots from buck power without USB-C attached
- no unexpected resets during ARM/FIRE activity
- radio behavior remains sane
- accepted FIRE still switches the dummy load cleanly
- post-fire heartbeats continue normally

### Important note
A mixed buck+USB check was tried later, but it is **not** the source of truth because power priority in that setup was ambiguous.

---

## What you should not expect from this rig yet

Do not treat the current bench rig as proof of:
- live igniter firing readiness
- banana-clip live launch readiness
- final field wiring quality
- final mechanical packaging
- final current capacity of the eventual launcher harness

This is still a deliberate, simplified, safe dummy-load stage.

---

## Recommended bench pass/fail summary for the current repo

A clean current-session bring-up means:
- both firmware targets build and upload
- both peers register and exchange traffic
- SAFE/ARM state propagates correctly
- remote ARM / DISARM behavior still matches the validated interlock rules
- continuity still separates cleanly into OPEN and PRESENT on the current rig
- accepted FIRE still switches only the safe `10 ohm` dummy load through `GPIO26` / `IRLZ44N`
- continuity stays sane during the pulse because mid-pulse sampling is suppressed
- buck-powered controller behavior remains stable

If all of that is still true, the launcher side is ready to move forward from documentation cleanup into the next deliberate phase.

---

## Best next step after this doc state

After this doc cleanup, the next project phase should be:

**post-cleanup launcher readiness planning for the first igniter-adjacent phase, still with risk controlled deliberately**

That means:
1. freeze the launcher node names and wiring assumptions now documented
2. decide the exact first igniter-adjacent checkpoint before touching hardware
3. define the minimum hardware delta from the current safe dummy-load rig
4. define pass/fail evidence before the next bench change
5. keep live firing itself out of scope until that checkpoint is written down clearly

---

## Handy commands

```bash
# Build both
pio run -e wrist
pio run -e launcher

# Upload launcher
pio run -e launcher --target upload

# Upload wrist
pio run -e wrist --target upload

# Serial monitor
pio device monitor -b 115200
```

---

## Reference docs

- `docs/PROJECT_SPEC.md`
- `docs/LAUNCHER_WIRING.md`
- `docs/FIRMWARE_ARCHITECTURE.md`
- `projects/helldivers-tac-com/CONTINUITY_BENCH_SETUP.md`
- `projects/helldivers-tac-com/MOSFET_DUMMY_LOAD_BENCH_SETUP.md`
- `projects/helldivers-tac-com/BUCK_CONVERTER_BENCH_PREP.md`
