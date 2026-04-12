# Bench Bring-Up Guide

*Created: 2026-04-11*

This document is the practical bring-up sequence for the current `helldivers-tac-com` repo state.

## What is verified right now

As of 2026-04-11, both PlatformIO environments build successfully on the VM:

- `pio run -e wrist` ✅
- `pio run -e launcher` ✅

That means the repo is ready for **firmware upload and serial bring-up**.

It does **not** yet mean the full launcher workflow is complete end to end.

## Important reality check

The current firmware is ready for:
- boot verification
- ESP-NOW peer registration
- heartbeat / status exchange
- launcher interlock switch sense verification

The current firmware is **not yet ready for full arm-and-fire bench testing** because:
- the wrist UI/touch path is still TODO
- the wrist firmware currently has no user input path that sends `ARM_SET`
- launcher continuity sampling is still TODO, so continuity stays `UNKNOWN`
- the wrist blocks fire if continuity is not good

So the practical bench target **today** is:
1. upload both boards
2. confirm both boot cleanly
3. confirm peer registration
4. confirm launcher heartbeat/status loop works
5. confirm the launcher ARM/SAFE switch is sensed correctly

That is still useful bring-up work. It proves the radio path and safety interlock foundation before dummy-load or igniter work.

---

## Safety setup for this bench session

For this stage:
- **Do not attach a real igniter**
- keep the launcher output unloaded, or use only a harmless indicator/dummy setup later
- start with the physical ARM/SAFE switch in **SAFE**
- keep the MOSFET gate/output wiring conservative and double-check polarity before power-up
- use the inline fuse on the igniter rail if that rail is connected at all
- share ground correctly between controller-side electronics and the launcher circuit

Because continuity logic is not active yet, there is no reason to connect a real match for this pass.

---

## Upload sequence

Open the repo in VS Code / PlatformIO:

```bash
cd ~/.openclaw/workspace/helldivers-tac-com
```

### Build again if you want a clean confirmation

```bash
pio run -e wrist
pio run -e launcher
```

### Upload launcher firmware

Connect only the launcher board first.

```bash
pio run -e launcher --target upload
```

### Upload wrist firmware

Then connect the wrist board.

```bash
pio run -e wrist --target upload
```

If PlatformIO picks the wrong port, select the correct serial port in VS Code before uploading.

---

## Serial monitor workflow

Use 115200 baud.

```bash
pio device monitor -b 115200
```

If you want to monitor one specific board at a time, do it in this order:

1. launcher by itself
2. wrist by itself
3. both powered together, switching between ports as needed

---

## Expected launcher boot output

On a clean launcher boot, you should see the equivalent of:

```text
[LAUNCHER] Boot
[LAUNCHER/radio] Wrist peer registered: <wrist-mac>
[LAUNCHER] Ready — state: DISARMED
```

Once the wrist is online and sending heartbeats, you should also start seeing:

```text
[LAUNCHER/radio] HEARTBEAT rx seq=<n>
```

That confirms:
- ESP-NOW initialized
- the paired wrist MAC was accepted as a peer
- the launcher is receiving traffic from the wrist

---

## Expected wrist boot output

On a clean wrist boot, you should see the equivalent of:

```text
[WRIST] Boot
[WRIST/link] Launcher peer registered: <launcher-mac>
[WRIST] Ready
```

Once the launcher begins broadcasting status, the wrist should log status packets like:

```text
[WRIST/link] STATUS rx — state=<n> armed=0 cont=0 key=0
```

Notes:
- `state=<n>` is expected because the current log prints the enum numerically
- `armed=0` is expected at boot
- `cont=0` is expected right now because continuity is not implemented yet
- `key=0` means the physical ARM switch is still in SAFE

---

## ARM/SAFE switch check

This is the most useful live bench check available in the current firmware.

### Start condition
- launcher powered on
- wrist powered on
- both exchanging packets
- launcher switch in **SAFE**

### Flip the launcher switch to ARM

On the launcher monitor, expect:

```text
[LAUNCHER] Key switch: ARM
```

On the wrist side, subsequent status logs should change from `key=0` to `key=1`.

Example:

```text
[WRIST/link] STATUS rx — state=<n> armed=0 cont=0 key=1
```

### Flip the launcher switch back to SAFE

On the launcher monitor, expect:

```text
[LAUNCHER] Key switch: SAFE
```

On the wrist side, status should return to `key=0`.

This confirms the physical interlock sense input is working through the radio link.

---

## What you should **not** expect yet

Do not expect the current firmware to do these yet:

### 1. Wrist-driven arming
There is no active UI/button/input path in `src/wrist/main.cpp` that calls:

- `launcher_link_sendArmSet(...)`

So the launcher will not enter ARMED from normal wrist use yet.

### 2. Successful fire permission
The launcher state machine requires:
- `state == ARMED`
- interlock on
- `continuity == PRESENT`

But continuity sampling is still TODO in `src/launcher/main.cpp`, so the continuity state does not become `PRESENT` in the current build.

### 3. Real stratagem input flow
Display, touch, and LVGL setup are still TODO in the wrist main loop, so there is no real human input path yet.

---

## Recommended bench conclusion for this firmware revision

If the following all work, this firmware revision has passed the right bring-up goal:

- both targets upload successfully
- launcher boots DISARMED
- both peers register the expected MAC address
- launcher receives wrist heartbeats
- wrist receives launcher status packets
- flipping the physical ARM/SAFE switch changes `key=0/1` on the wrist status logs
- no unexpected boot-time ignition activity occurs

That is a solid foundation check before moving to dummy-load fire-path testing.

---

## Best next firmware tasks

To reach true bench fire-path testing with a dummy load, the next missing pieces are:

1. **Add a temporary arm/disarm test control on the wrist**
   - serial command, hardcoded timer, or simple button input
   - enough to exercise `launcher_link_sendArmSet(...)`

2. **Implement or stub continuity handling on the launcher**
   - either real ADC-based continuity sensing
   - or a clearly temporary bench stub for dummy-load development only

3. **Add a temporary test trigger for fire**
   - only after arm path and continuity behavior are verified
   - still using LED or resistive dummy load first, never a real igniter during code-debug bring-up

4. **Then wire in the real wrist UI**
   - display init
   - touch init
   - LVGL screens
   - stratagem input path

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

