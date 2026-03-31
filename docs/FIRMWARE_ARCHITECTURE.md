# Helldivers Tac-Com Bracelet Firmware Architecture

_Last updated: 2026-03-30_

## Purpose

This document defines a firmware architecture for the **Helldivers Tac-Com Bracelet** system:

- **Wrist unit**: Elecrow CrowPanel ESP32 3.5" touchscreen device running the LVGL-based tac-com UI
- **Launcher unit**: ESP32-WROOM-32-based wireless launch controller with hardware interlock, continuity check, and MOSFET ignition output

The goal is to provide enough structure that development can begin immediately in PlatformIO using the Arduino framework.

---

# 1. Recommended Project Structure

Two viable layouts make sense:

## Option A — Recommended: PlatformIO monorepo

This keeps shared protocol/types in one place and makes it easier to evolve both firmwares together.

```text
tac-com/
├─ platformio.ini
├─ PROJECT_SPEC.md
├─ FIRMWARE_ARCHITECTURE.md
├─ boards/
│  └─ custom_notes.md
├─ include/
│  └─ common/
│     ├─ protocol.h
│     ├─ protocol_types.h
│     ├─ stratagems.h
│     ├─ config_shared.h
│     └─ version.h
├─ lib/
│  ├─ common/
│  │  ├─ crc32.cpp
│  │  ├─ crc32.h
│  │  └─ time_utils.h
│  ├─ wrist_ui/
│  │  ├─ ui_theme.cpp
│  │  ├─ ui_theme.h
│  │  ├─ ui_screens.cpp
│  │  ├─ ui_screens.h
│  │  ├─ ui_widgets.cpp
│  │  └─ ui_widgets.h
│  ├─ wrist_core/
│  │  ├─ stratagem_engine.cpp
│  │  ├─ stratagem_engine.h
│  │  ├─ launcher_link.cpp
│  │  ├─ launcher_link.h
│  │  ├─ power_manager.cpp
│  │  ├─ power_manager.h
│  │  ├─ battery_monitor.cpp
│  │  └─ battery_monitor.h
│  └─ launcher_core/
│     ├─ launcher_state.cpp
│     ├─ launcher_state.h
│     ├─ igniter_driver.cpp
│     ├─ igniter_driver.h
│     ├─ continuity.cpp
│     ├─ continuity.h
│     ├─ safety_interlock.cpp
│     ├─ safety_interlock.h
│     ├─ radio_link.cpp
│     └─ radio_link.h
├─ src/
│  ├─ wrist/
│  │  └─ main.cpp
│  └─ launcher/
│     └─ main.cpp
├─ assets/
│  ├─ fonts/
│  ├─ sounds/
│  └─ icons/
├─ test/
│  ├─ test_protocol/
│  ├─ test_stratagem_engine/
│  ├─ test_launcher_state/
│  └─ test_continuity/
└─ docs/
   ├─ pinout_wrist.md
   ├─ pinout_launcher.md
   ├─ espnow_pairing.md
   └─ safety_validation_checklist.md
```

### Example `platformio.ini`

```ini
[platformio]
default_envs = wrist

[env:wrist]
platform = espressif32
board = esp32dev
framework = arduino
build_src_filter = +<src/wrist/> -<src/launcher/>
lib_deps =
  lvgl/lvgl @ ^9
build_flags =
  -D TACCOM_WRIST
  -I include

[env:launcher]
platform = espressif32
board = esp32dev
framework = arduino
build_src_filter = +<src/launcher/> -<src/wrist/>
build_flags =
  -D TACCOM_LAUNCHER
  -I include
```

## Option B — Two separate PlatformIO projects

Use this if board bring-up becomes very different and the UI project starts carrying a lot of display-specific baggage.

```text
tac-com/
├─ common/
│  ├─ include/
│  └─ lib/
├─ wrist-fw/
│  ├─ platformio.ini
│  ├─ src/
│  ├─ lib/
│  └─ include/
└─ launcher-fw/
   ├─ platformio.ini
   ├─ src/
   ├─ lib/
   └─ include/
```

### Recommendation

Use **Option A** first. The shared packet definitions, stratagem database, and common constants are important enough that a monorepo is the cleanest fit.

---

# 2. System-Wide Firmware Design Principles

1. **State machines over ad-hoc flags**  
   Both units control safety-relevant behavior. Explicit states reduce hidden interactions.

2. **Non-blocking loops**  
   No long `delay()` calls in normal operation. Timers should be millisecond-based using `millis()`.

3. **Message protocol versioning**  
   Every ESP-NOW packet should include protocol version, message type, sequence number, and CRC/check byte.

4. **Fail-safe defaults**  
   - Launcher boots **DISARMED**
   - Wrist blocks input when launcher is offline or disarmed
   - Loss of comms must never imply permission to fire

5. **Separation of concerns**  
   UI, comms, state logic, and hardware drivers should stay in separate modules.

6. **Deterministic fire path**  
   The launcher must own the final fire decision after validating:
   - sender MAC
   - current state == ARMED
   - key switch still ON
   - continuity check passes
   - not in cooldown or fault

---

# 3. Wrist Unit Firmware Architecture

## 3.1 Responsibilities

The wrist unit is responsible for:

- UI rendering via LVGL
- Touch/swipe input handling
- Stratagem recognition
- Choosing the active launch stratagem when the launcher arms
- Displaying launcher status and connectivity
- Sending arm/disarm/fire commands
- Blocking dangerous interactions when the launcher is offline or unsafe
- Battery and idle power management

## 3.2 Wrist runtime model

The wrist firmware can run as a **cooperative scheduler** in `loop()` or with lightweight FreeRTOS tasks. Because LVGL, touch, and comms are all periodic and latency-sensitive, a split-task design is useful, but it should remain simple.

### Recommended runtime layout

- `setup()` initializes hardware, LVGL, touch, battery ADC, ESP-NOW, and app state
- `loop()` runs a fast coordinator and calls service functions
- Optional pinned tasks:
  - `uiTask` on one core for LVGL tick/handler
  - `radioTask` on one core for periodic heartbeat/send queue handling

For first implementation, a **single-threaded non-blocking loop** is enough and simpler to debug.

---

## 3.3 Wrist main loop structure

### High-level flow

```cpp
void loop() {
    uint32_t now = millis();

    serviceUi(now);
    serviceTouchInput(now);
    serviceStratagemEngine(now);
    serviceLauncherComms(now);
    serviceStatusTimeouts(now);
    serviceBatteryMonitor(now);
    servicePowerManager(now);
    serviceAudioFeedback(now);
}
```

### Scheduling targets

- `serviceUi()` every loop, LVGL handler every 5-10 ms
- touch sampling every 10-20 ms
- heartbeat transmit every 2000 ms
- stale launcher timeout check every 100-250 ms
- battery read every 2-5 s
- idle/sleep evaluation every 1 s

### Setup sequence

1. initialize serial logging
2. initialize display driver
3. initialize touch driver
4. initialize LVGL and create screens
5. initialize battery ADC
6. initialize buzzer/speaker if used
7. initialize Wi-Fi in STA mode
8. initialize ESP-NOW
9. register launcher peer MAC
10. set wrist app state to default:
    - launcher offline until heartbeat reply received
    - input state = IDLE
    - active stratagem = none
    - last event = READY / UNKNOWN

---

## 3.4 LVGL task / display update loop

## LVGL architecture goals

The LVGL layer should not contain business logic beyond rendering state and forwarding UI events. The core logic should live in app modules.

### UI layers

1. **Top status bar**
   - battery icon/percent
   - launcher online/offline
   - armed/disarmed indicator
   - RSSI or link quality approximation

2. **Main stratagem panel**
   - active stratagem name
   - expected arrow sequence
   - player-entered sequence
   - prompt text / state banner

3. **Bottom action zone**
   - ARM / DISARM button
   - CONFIRM button when eligible
   - RESET / CLEAR input button
   - tab buttons or screen nav controls

### Core LVGL functions

```cpp
void ui_init();
void ui_build_main_screen();
void ui_build_status_screen();
void ui_build_settings_screen();
void ui_tick();
void ui_render_app_state(const WristAppState& state);
void ui_show_feedback(UiFeedbackType type, const char* message);
```

### LVGL timing

- A periodic LVGL tick should occur every 1-5 ms if using a timer callback
- `lv_timer_handler()` should run roughly every 5-10 ms
- Avoid heavy packet parsing or stratagem matching inside LVGL callbacks

### Display update strategy

The UI should update from a central immutable-ish state snapshot:

```cpp
struct WristAppState {
    LauncherLinkState launcher;
    StratagemInputState inputState;
    ActiveStratagem activeStratagem;
    BatteryState battery;
    UiScreen currentScreen;
    bool confirmVisible;
    bool inputEnabled;
    char statusText[64];
};
```

A common pattern:

- core modules modify app state
- UI layer checks dirty flags
- only changed widgets are refreshed

### Dirty flag categories

- `DIRTY_LINK`
- `DIRTY_ARM_STATE`
- `DIRTY_INPUT`
- `DIRTY_ACTIVE_STRATAGEM`
- `DIRTY_BATTERY`
- `DIRTY_LAST_EVENT`
- `DIRTY_SCREEN`

---

## 3.5 ESP-NOW comms task

## Wrist comms responsibilities

- initialize peer with known launcher MAC
- send heartbeat every 2 s
- send arm/disarm command on user request
- send fire command only after local conditions pass
- track last receive timestamp
- maintain message sequence numbers
- handle acks/timeouts

### Wrist-side communication state

```cpp
struct LauncherLinkState {
    bool peerConfigured;
    bool online;
    bool armed;
    bool continuityOk;
    bool keySwitchOn;
    bool firePermitted;
    uint8_t batteryPercent;
    int8_t linkQuality;
    uint32_t lastHeartbeatSentMs;
    uint32_t lastStatusRxMs;
    uint32_t lastAckRxMs;
    LauncherSafetyState remoteState;
    LauncherEvent lastEvent;
    uint16_t txSeq;
    uint16_t lastRxSeq;
};
```

### Periodic comms flow

Every 2000 ms:

1. send `HEARTBEAT`
2. include current wrist battery if desired for debugging
3. expect `STATUS` response from launcher
4. if no valid status for >5000 ms, mark launcher `OFFLINE`

### Command send rules

**ARM**:
- send only if launcher online
- user tapped ARM
- launcher reported key switch ON or system can still try arm and let launcher reject

**DISARM**:
- send whenever user taps DISARM
- also send when leaving main screen if desired
- also send on wrist-side emergency UI action

**FIRE**:
- send only if:
  - launcher online
  - launcher state == ARMED
  - continuity OK
  - key switch ON
  - active stratagem matched
  - confirm gate open
  - explicit confirm received

### Timeout handling

If no launcher status for >5 s:

- set `online = false`
- set `armed = false` locally for UI purposes
- hide confirm button
- disable stratagem input
- show OFFLINE / LINK LOST banner

The launcher itself retains its real state; the wrist must assume nothing safe can proceed.

---

## 3.6 Stratagem input state machine

This is the core game-like behavior. The input state machine should be explicit and separate from the launcher link state.

## State enum

```cpp
enum class StratagemInputState : uint8_t {
    IDLE,
    INPUTTING,
    MATCHED,
    CONFIRMING,
    FIRING
};
```

## State meanings

### `IDLE`
No active player input yet.

**Entry conditions:**
- boot
- launcher disarmed/offline
- after successful fire cycle reset
- after timeout/reset
- after wrong input reset

**Behavior:**
- show active stratagem and expected sequence
- input only enabled if launcher ARMED and online
- empty entered sequence buffer

### `INPUTTING`
User has started entering arrows.

**Entry conditions:**
- first valid arrow received from IDLE

**Behavior:**
- append arrow to buffer
- update on-screen entered sequence
- restart inactivity timeout timer (~3000 ms)
- compare prefix against active stratagem sequence

**Transitions:**
- valid prefix continues -> remain INPUTTING
- full exact match -> MATCHED
- invalid prefix -> reset to IDLE with error feedback
- inactivity timeout -> reset to IDLE

### `MATCHED`
The full active sequence has been entered correctly.

**Entry conditions:**
- entered buffer exactly equals active stratagem sequence

**Behavior:**
- play match feedback
- flash success UI
- show stratagem name large
- start mandatory lockout timer of at least 2000 ms
- disable further directional input

**Transitions:**
- after lockout expires -> CONFIRMING
- if launcher becomes disarmed/offline -> IDLE

### `CONFIRMING`
User is allowed to explicitly confirm launch.

**Entry conditions:**
- 2-second minimum delay after MATCHED
- launcher still ARMED and online

**Behavior:**
- show `CONFIRM LAUNCH?`
- display dedicated confirm button
- may require one tap only, or optional double-confirm later
- allow cancel/reset back to IDLE

**Transitions:**
- user confirms -> FIRING
- launcher disarmed/offline -> IDLE
- timeout or cancel -> IDLE

### `FIRING`
Fire command has been sent and wrist is awaiting acknowledgment / final status.

**Entry conditions:**
- fire command transmitted successfully from CONFIRMING

**Behavior:**
- lock input completely
- show `LAUNCHING...`
- wait for `FIRE_ACK` / updated status packet

**Transitions:**
- launcher reports FIRED -> IDLE after post-fire reset
- launcher reports FAULT -> IDLE with error banner
- timeout waiting for fire ack -> IDLE with communication fault notice

## State transition summary

```text
IDLE
  -> INPUTTING        on first valid arrow while armed/online

INPUTTING
  -> INPUTTING        on valid partial prefix
  -> MATCHED          on complete exact match
  -> IDLE             on invalid arrow or inactivity timeout

MATCHED
  -> CONFIRMING       after 2-second lockout
  -> IDLE             if link lost or launcher disarmed

CONFIRMING
  -> FIRING           on confirm tap
  -> IDLE             on cancel / timeout / link lost / disarm

FIRING
  -> IDLE             on FIRED / FAULT / ack timeout
```

## Input buffer design

```cpp
constexpr size_t MAX_INPUT_LEN = 8;

enum class Direction : uint8_t { UP, DOWN, LEFT, RIGHT };

struct InputBuffer {
    Direction values[MAX_INPUT_LEN];
    uint8_t length;
    uint32_t lastInputMs;
};
```

## Prefix-match algorithm

Each new input should be checked incrementally:

```cpp
bool isPrefixMatch(const InputBuffer& input, const StratagemDef& active);
bool isFullMatch(const InputBuffer& input, const StratagemDef& active);
```

This avoids searching the whole database during normal use because only the active stratagem matters for launch.

---

## 3.7 Random stratagem selection logic

The active launch code should be chosen when the launcher transitions into ARMED.

## Approved pool

Initial pool from the spec:

- Eagle Airstrike — `↑ → ↓ →`
- Orbital Precision Strike — `→ → ↓ →`
- Resupply — `↓ ↓ → ↑`
- Eagle 110mm Rocket Pods — `↑ → ↑ ←`
- Reinforce — `↑ ↓ → ← ↑`

## Selection timing

### Recommended rule

Choose a new active stratagem when:

- launcher transitions `DISARMED -> ARMED`, or
- after a completed fire/disarm cycle and re-arm

Do **not** re-roll during a single armed session unless explicitly requested.

## Anti-repeat behavior

Avoid selecting the same stratagem twice in a row unless the pool size is 1.

```cpp
int chooseRandomStratagem(int previousIndex, int poolSize);
```

Pseudo-logic:

1. if pool size == 1, return index 0
2. pick random index in `[0, poolSize)`
3. if equal to previous index, reroll once or loop until different

## Random source

Seed once during boot using a mix of:

- `esp_random()`
- ADC noise if available
- `millis()` at boot

## Data structure

```cpp
struct StratagemDef {
    uint8_t id;
    const char* name;
    Direction sequence[8];
    uint8_t length;
    bool enabledForLaunchPool;
};

struct ActiveStratagem {
    int index;
    uint8_t id;
    const StratagemDef* def;
    uint32_t selectedAtMs;
};
```

---

## 3.8 Launcher status display logic

The wrist should always present launcher state clearly and conservatively.

## UI status priorities

Display the most safety-relevant state first.

Priority order:

1. `OFFLINE`
2. `FAULT`
3. `FIRED`
4. `ARMED`
5. `DISARMED`
6. `BOOTING/UNKNOWN`

## Status fields to show

- connectivity: ONLINE / OFFLINE
- safety state: DISARMED / ARMED / FIRING / FIRED / FAULT
- continuity: OK / OPEN / UNKNOWN
- key switch: SAFE / ARM
- last event: READY / FIRED / FAULT / DISARMED
- signal quality icon
- launcher battery percent if measured

## Rendering rules

- **Online + Disarmed**: yellow or neutral amber
- **Armed**: strong red accent and pulsing warning label
- **Firing**: animation and full lockout overlay
- **Fault**: red with explanation text
- **Offline**: gray / red and disable all action widgets

## Example display strings

```text
Launcher: ONLINE
State: ARMED
Continuity: OK
Interlock: ARM
Last Event: READY
Signal: 3/4
```

---

## 3.9 Power management

## Wrist power goals

The wrist should run from a 1S LiPo for roughly 3-4 hours. Main consumers will be:

- TFT backlight
- ESP32 radio
- display refresh activity
- optional speaker/buzzer

## Power management features

1. **Battery monitoring**
   - sample ADC every 2-5 s
   - low-pass filter readings
   - map voltage to percent

2. **UI brightness control**
   - full brightness when active or armed
   - dim after inactivity
   - brighten on touch/comms events

3. **Idle detection**
   - no touch for X minutes
   - launcher offline/disarmed
   - not in confirmation or fire flow
   - then enter display dim state or deep sleep

4. **Deep sleep policy**
   - only allowed if launcher is disarmed/offline and no active session
   - wake on touch, power button, or other supported source

5. **Low battery behavior**
   - at <=20%: show warning icon/banner
   - at <=10%: block arming/fire request and advise recharge

## Battery data structure

```cpp
struct BatteryState {
    float voltage;
    uint8_t percent;
    bool low;
    bool critical;
    uint32_t lastSampleMs;
};
```

## Suggested states

```cpp
enum class WristPowerMode : uint8_t {
    ACTIVE,
    DIMMED,
    IDLE,
    SLEEP_PENDING,
    DEEP_SLEEP
};
```

---

## 3.10 Wrist key data structures

```cpp
enum class LauncherSafetyState : uint8_t {
    BOOTING,
    DISARMED,
    ARMED,
    FIRING,
    FIRED,
    FAULT
};

enum class LauncherEvent : uint8_t {
    NONE,
    READY,
    ARMED_OK,
    DISARMED_OK,
    FIRE_SENT,
    FIRED_OK,
    CONTINUITY_FAIL,
    INTERLOCK_BLOCKED,
    COMMS_LOST,
    FAULT_GENERIC
};

struct LauncherStateView {
    bool online;
    LauncherSafetyState state;
    LauncherEvent lastEvent;
    bool continuityOk;
    bool keySwitchOn;
    bool canAcceptArm;
    bool canAcceptFire;
    uint8_t batteryPercent;
    int8_t rssiApprox;
    uint32_t lastUpdateMs;
};
```

---

# 4. Launcher Unit Firmware Architecture

## 4.1 Responsibilities

The launcher unit is the authority for all safety-critical hardware behavior. It must:

- boot into DISARMED
- reject commands from unknown MAC addresses
- require physical interlock ON before arming
- perform continuity check before fire
- drive the ignition MOSFET for a controlled pulse
- broadcast current status back to the wrist
- automatically return to a safe state after fire or fault

## 4.2 Launcher main loop structure

A non-blocking loop is again preferred.

### High-level flow

```cpp
void loop() {
    uint32_t now = millis();

    serviceInterlock(now);
    serviceContinuity(now);
    serviceLauncherState(now);
    serviceIgnitionPulse(now);
    serviceStatusBroadcast(now);
    serviceCommandTimeouts(now);
    serviceIndicators(now);
}
```

### Setup sequence

1. initialize serial logging
2. configure GPIO26 ignition output LOW
3. configure interlock switch input
4. configure continuity ADC input
5. configure status LED if present
6. initialize Wi-Fi STA mode
7. initialize ESP-NOW
8. register wrist peer MAC
9. state = DISARMED
10. lastEvent = READY
11. perform initial continuity read
12. begin listening for packets

---

## 4.3 ESP-NOW receive handler

The launcher receive callback should be kept short. It should validate and enqueue work rather than executing fire logic directly inside the callback.

### Receive path

1. callback invoked with sender MAC + payload
2. validate sender MAC against expected wrist MAC
3. validate packet size, protocol version, CRC/check field
4. reject stale or duplicate sequence numbers if needed
5. decode by `msgType`
6. push command into a small command queue or update pending command flags
7. `loop()` consumes pending command and applies state logic

### Accepted commands

- `HEARTBEAT`
- `ARM_SET` (`arm=true` or `arm=false`)
- `FIRE_CMD`

### Callback sketch

```cpp
void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (!macMatchesExpected(mac)) return;
    if (!validatePacket(data, len)) return;

    DecodedPacket pkt;
    if (!decodePacket(data, len, pkt)) return;

    enqueueReceivedPacket(pkt);
}
```

---

## 4.4 Safety state machine

This state machine must be the core of launcher logic.

## State enum

```cpp
enum class LauncherSafetyState : uint8_t {
    DISARMED,
    ARMED,
    FIRING,
    FIRED,
    FAULT
};
```

## State definitions

### `DISARMED`
Safe default state.

**Behavior:**
- ignition GPIO forced LOW
- fire commands rejected
- continuity can still be monitored
- arming only accepted if physical interlock is ON

**Transitions:**
- `ARM_SET(true)` + key switch ON -> `ARMED`
- stay DISARMED otherwise

### `ARMED`
Launcher is hot but not firing.

**Behavior:**
- ignition GPIO LOW
- status LED / switch indicator shows armed
- continuity check performed periodically
- if interlock goes OFF, immediately disarm

**Transitions:**
- `ARM_SET(false)` -> `DISARMED`
- key switch OFF -> `DISARMED`
- `FIRE_CMD` + continuity OK -> `FIRING`
- `FIRE_CMD` + continuity fail -> `FAULT`

### `FIRING`
Ignition pulse in progress.

**Behavior:**
- gate MOSFET ON
- pulse duration timer active
- no new fire or arm commands accepted

**Transitions:**
- pulse complete -> `FIRED`
- hardware fault / unexpected condition -> `FAULT`

### `FIRED`
Shot has been executed.

**Behavior:**
- ignition output LOW
- send final fired status
- optionally remain briefly in FIRED for UI visibility

**Transitions:**
- after cooldown / report window -> `DISARMED`

### `FAULT`
Safety or hardware problem detected.

**Behavior:**
- ignition output LOW
- repeated fire attempts rejected
- send fault code in status packet

**Transitions:**
- explicit disarm, or safe recovery path -> `DISARMED`

## Transition diagram

```text
DISARMED
  -> ARMED         on arm_cmd and key_switch_ON

ARMED
  -> DISARMED      on disarm_cmd or key_switch_OFF
  -> FIRING        on fire_cmd and continuity_OK
  -> FAULT         on fire_cmd and continuity_FAIL

FIRING
  -> FIRED         when ignition pulse completes
  -> FAULT         on internal fault

FIRED
  -> DISARMED      after post-fire cooldown/report window

FAULT
  -> DISARMED      on disarm or safe recovery/reset
```

---

## 4.5 Ignition pulse control

The ignition driver must be simple and deterministic.

## Pin behavior

- ignition pin: `GPIO26`
- default output: LOW
- only driven HIGH while state == `FIRING`

## Pulse timing

- configurable duration: default `1000 ms`
- implemented via timestamp comparison, not blocking `delay(1000)`

### Data structure

```cpp
struct IgnitionControl {
    uint8_t pin;
    bool active;
    uint32_t pulseStartMs;
    uint32_t pulseDurationMs;
};
```

### Fire sequence

1. verify state == ARMED
2. verify key switch ON
3. run fresh continuity check
4. if continuity fail -> FAULT
5. set state = FIRING
6. write GPIO HIGH
7. record `pulseStartMs`
8. in `serviceIgnitionPulse(now)`, once elapsed >= pulseDurationMs:
   - write GPIO LOW
   - set state = FIRED
   - set lastEvent = FIRED_OK
   - send status / fire ack

## Safety rules

- fire path may only be entered from `ARMED`
- GPIO must always be forced LOW on boot, disarm, fault, or unexpected reset path
- never fire directly inside receive callback

---

## 4.6 Continuity check routine

The continuity check should use a low-current sensing path that cannot ignite the igniter.

## Goals

- determine whether launch leads appear connected to a valid igniter/load
- provide `continuityOk` status to the wrist
- block fire if continuity is open or ambiguous

## Routine timing

- sample every 250-1000 ms while armed
- sample less frequently while disarmed if desired
- always perform a fresh continuity check immediately before entering FIRING

## Result categories

```cpp
enum class ContinuityState : uint8_t {
    UNKNOWN,
    OPEN,
    PRESENT,
    SHORT_FAULT
};
```

## Driver data structure

```cpp
struct ContinuityReading {
    ContinuityState state;
    uint16_t rawAdc;
    float sensedVoltage;
    uint32_t lastReadMs;
};
```

## Recommended logic

1. read ADC
2. convert to voltage or threshold bucket
3. classify as:
   - `PRESENT` = expected igniter resistance range equivalent
   - `OPEN` = no connected igniter
   - `SHORT_FAULT` = abnormal low-resistance / wiring fault
4. set `continuityOk = (state == PRESENT)`

### Practical note

Exact thresholds will depend on the final continuity resistor network and ADC scaling. Bench characterization with actual igniters and dummy loads should determine threshold constants.

---

## 4.7 Status broadcast

The launcher should proactively respond to wrist activity and optionally push periodic status updates while online.

## Broadcast triggers

Send `STATUS` packet when:

- heartbeat received
- arm/disarm command processed
- fire command accepted/rejected
- continuity state changes
- interlock state changes
- launcher state changes
- periodic interval elapses (for example every 1000-2000 ms while peer is active)

## Status fields

- current safety state
- continuity state
- key switch/interlock state
- last event code
- fault code
- launcher battery percent if available
- uptime or monotonic counter optional

## Internal launcher state snapshot

```cpp
struct LauncherRuntimeState {
    LauncherSafetyState state;
    LauncherEvent lastEvent;
    ContinuityState continuity;
    bool keySwitchOn;
    bool onlinePeerSeen;
    bool ignitionActive;
    uint8_t batteryPercent;
    uint32_t lastCommandRxMs;
    uint32_t lastStatusTxMs;
    uint16_t lastRxSeq;
    uint16_t txSeq;
};
```

---

## 4.8 Launcher key data structures

```cpp
enum class FaultCode : uint8_t {
    NONE,
    INVALID_MAC,
    BAD_PACKET,
    INTERLOCK_OFF,
    CONTINUITY_OPEN,
    CONTINUITY_SHORT,
    FIRE_WHILE_DISARMED,
    FIRE_TIMEOUT,
    INTERNAL_ERROR
};

struct PendingCommand {
    bool valid;
    uint8_t msgType;
    uint16_t seq;
    uint32_t receivedAtMs;
    union {
        bool arm;
        uint32_t fireToken;
    } data;
};
```

---

# 5. ESP-NOW Message Protocol

## 5.1 Protocol goals

The protocol should be compact, versioned, explicit, and easy to decode with fixed-size structs.

## Design notes

- use packed structs
- all packets include a shared header
- sender/receiver may ignore packets with wrong version or wrong size
- include sequence numbers for duplicate detection and ack correlation

## 5.2 Common packet header

```cpp
#pragma pack(push, 1)

struct PacketHeader {
    uint8_t magic;          // 0xA7
    uint8_t version;        // protocol version, start at 1
    uint8_t msgType;        // enum MessageType
    uint8_t flags;          // reserved / ack bits / error bits
    uint16_t seq;           // sender sequence number
    uint16_t payloadLen;    // bytes after header
    uint32_t sessionId;     // boot session or random nonce
};

#pragma pack(pop)
```

### Suggested message types

```cpp
enum class MessageType : uint8_t {
    HEARTBEAT   = 1,
    STATUS      = 2,
    ARM_SET     = 3,
    FIRE_CMD    = 4,
    FIRE_ACK    = 5
};
```

## 5.3 HEARTBEAT packet

Sent from wrist to launcher every 2 seconds.

### Purpose

- keep presence alive
- request fresh status
- provide wrist session/health info if useful

```cpp
struct HeartbeatPayload {
    uint32_t wristUptimeMs;
    uint8_t wristBatteryPercent;
    uint8_t uiScreen;
    uint8_t inputState;
    uint8_t reserved;
};
```

### Packet summary

- **Direction:** Wrist -> Launcher
- **Rate:** every 2000 ms
- **Expected response:** `STATUS`

## 5.4 STATUS packet

Sent from launcher to wrist in response to heartbeat and on state changes.

```cpp
struct StatusPayload {
    uint8_t launcherState;      // LauncherSafetyState
    uint8_t continuityState;    // ContinuityState
    uint8_t lastEvent;          // LauncherEvent
    uint8_t faultCode;          // FaultCode
    uint8_t keySwitchOn;        // 0/1
    uint8_t canArm;             // 0/1
    uint8_t canFire;            // 0/1
    uint8_t batteryPercent;
    int8_t  linkQuality;
    uint8_t reserved[3];
    uint32_t launcherUptimeMs;
};
```

### Packet summary

- **Direction:** Launcher -> Wrist
- **Trigger:** heartbeat response + event-driven pushes

## 5.5 ARM/DISARM packet (`ARM_SET`)

Sent from wrist to launcher when the user requests an arm state change.

```cpp
struct ArmSetPayload {
    uint8_t arm;                // 1 = arm, 0 = disarm
    uint8_t requestedByUi;      // always 1 for now
    uint16_t reserved;
    uint32_t requestToken;
};
```

### Behavior

- launcher receives `arm=1`
  - if key switch ON -> transitions to ARMED and returns STATUS
  - if key switch OFF -> remains DISARMED, sets lastEvent/fault accordingly, returns STATUS
- launcher receives `arm=0`
  - transitions to DISARMED, returns STATUS

## 5.6 FIRE command packet

Sent only when wrist is in `CONFIRMING` and user taps confirm.

```cpp
struct FireCmdPayload {
    uint8_t stratagemId;
    uint8_t inputLength;
    uint8_t reserved[2];
    uint32_t requestToken;
    uint32_t matchedAtMs;
};
```

### Behavior

Launcher validates:

- sender MAC valid
- state == ARMED
- key switch ON
- continuity OK

Then either:

- accepts and enters FIRING, or
- rejects and returns `FIRE_ACK` / `STATUS` with fault reason

## 5.7 FIRE acknowledgment packet

Returned by launcher when a fire command is processed.

```cpp
struct FireAckPayload {
    uint32_t requestToken;
    uint8_t accepted;           // 1 yes, 0 no
    uint8_t launcherState;      // post-processing state
    uint8_t lastEvent;
    uint8_t faultCode;
    uint32_t firedAtMs;
};
```

### Use cases

- immediate acceptance/rejection of fire command
- wrist correlates with `requestToken`
- final state should still also arrive in `STATUS`

## 5.8 Packet wrapper examples

```cpp
struct HeartbeatPacket {
    PacketHeader header;
    HeartbeatPayload payload;
    uint32_t crc32;
};

struct StatusPacket {
    PacketHeader header;
    StatusPayload payload;
    uint32_t crc32;
};
```

## 5.9 Validation rules

Every receiver should reject packets if:

- `magic` mismatch
- unsupported `version`
- `payloadLen` unexpected for `msgType`
- CRC/check fails
- sender MAC mismatch
- duplicate `seq` already processed recently

## 5.10 Recommended protocol behavior notes

- `STATUS` is the source of truth for launcher state
- `FIRE_ACK` is immediate command result feedback
- the wrist UI should not assume success merely because `esp_now_send()` returned OK
- command delivery success and command acceptance are separate things

---

# 6. LVGL Screen Layout Notes

The UI should be modular so future tabs like GPS and telemetry can be added without touching the core launch flow.

## Screen 0 — Main stratagem input screen

This is the default and primary interaction screen.

### Widgets

- top status bar container
  - launcher online/offline icon
  - armed/disarmed badge
  - continuity badge
  - battery icon/percent
  - signal indicator
- main title label
  - `TACTICAL COMMUNICATIONS`
- active stratagem card
  - stratagem name label
  - expected sequence rendered as arrow icons
- entered sequence card
  - entered arrows row
  - progress indicator like `3 / 4`
- prompt/status banner
  - `WAITING FOR ARM`
  - `ENTER STRATAGEM`
  - `MATCH CONFIRMED`
  - `CONFIRM LAUNCH?`
  - `LAUNCHING...`
- touch directional pad area
  - large 4-zone swipe/tap target or full-screen swipe recognizer
- buttons
  - ARM / DISARM
  - CLEAR / RESET
  - CONFIRM LAUNCH (hidden except in CONFIRMING)

### Notes

- this screen should be optimized for glove-like or outdoor use: large text, strong contrast, minimal clutter
- success/failure flashes should be obvious but brief

## Screen 1 — Launcher diagnostics screen

### Widgets

- state summary tile
  - ONLINE/OFFLINE
  - current launcher state
- continuity tile
  - PRESENT / OPEN / SHORT
  - raw ADC value optional
- interlock tile
  - switch SAFE / ARM
- last event / fault tile
- battery tile
- comms stats tile
  - packets sent/received
  - last heartbeat age
  - last RSSI estimate
- manual controls
  - ARM
  - DISARM
  - Ping/Test button

### Notes

This screen is for debugging and field checks before live launch.

## Screen 2 — GPS tracking placeholder

### Widgets

- placeholder label: `GPS MODULE NOT INSTALLED`
- coordinate labels
- map or compass placeholder

No core logic dependency.

## Screen 3 — Flight telemetry placeholder

### Widgets

- placeholder label
- altimeter data fields
- vertical speed fields
- event log list

No dependency on launcher state machine.

## Screen 4 — Settings screen

### Widgets

- launch pool mode dropdown
  - approved pool
  - fixed favorite
  - harder pool later
- audio toggle
- screen brightness slider
- sleep timeout dropdown
- protocol/version/build info
- paired launcher MAC display
- optional hidden calibration submenu

## Shared modal overlays

Useful LVGL overlays/dialogs:

- `OFFLINE` modal
- `CONFIRM LAUNCH?` modal
- `FAULT` modal with reason
- `FIRED` success overlay
- low battery warning banner

---

# 7. Suggested File Breakdown

This section maps functionality into concrete files a developer can start creating.

## 7.1 Shared files

### `include/common/protocol_types.h`
Contains shared enums and structs:

- `MessageType`
- `LauncherSafetyState`
- `LauncherEvent`
- `FaultCode`
- `ContinuityState`
- `Direction`

### `include/common/protocol.h`
Contains packed packet structs and encode/decode helpers:

- `PacketHeader`
- payload structs
- `validatePacket()`
- `buildHeartbeatPacket()`
- `buildStatusPacket()`

### `include/common/stratagems.h`
Contains stratagem database definitions:

- `StratagemDef`
- approved launch pool array
- helpers for rendering arrow names/icons

### `include/common/config_shared.h`
Contains cross-firmware constants:

- protocol version
- heartbeat interval
- timeout thresholds
- default fire pulse duration
- max input length

### `include/common/version.h`
Firmware version strings/build numbers.

---

## 7.2 Wrist files

### `src/wrist/main.cpp`
Top-level firmware entry point.

Should do:
- boot setup
- initialize all subsystems
- main service loop dispatch

### `lib/wrist_core/launcher_link.h/.cpp`
Handles ESP-NOW on the wrist side.

Responsibilities:
- peer init
- send heartbeat
- send arm/disarm
- send fire command
- receive and decode status/fire-ack
- update `LauncherLinkState`

### `lib/wrist_core/stratagem_engine.h/.cpp`
Implements stratagem state machine and input matching.

Responsibilities:
- direction input buffer
- timeout/reset logic
- match detection
- 2-second post-match lockout
- confirm gate handling
- random active stratagem selection

### `lib/wrist_core/power_manager.h/.cpp`
Handles dimming and sleep policy.

Responsibilities:
- inactivity tracking
- brightness mode changes
- deep sleep decision logic

### `lib/wrist_core/battery_monitor.h/.cpp`
Battery ADC sampling and percent conversion.

Responsibilities:
- raw ADC read
- filtering
- percent thresholds
- low/critical flags

### `lib/wrist_ui/ui_screens.h/.cpp`
Creates and stores LVGL screen objects.

Responsibilities:
- main screen build
- diagnostics screen build
- settings screen build
- screen switching helpers

### `lib/wrist_ui/ui_widgets.h/.cpp`
Owns widget references and small update helpers.

Responsibilities:
- labels, bars, icons, buttons
- `updateLauncherBadge()`
- `updateStratagemCard()`
- `showConfirmOverlay()`

### `lib/wrist_ui/ui_theme.h/.cpp`
Theme/colors/fonts styling.

Responsibilities:
- armed/disarmed palettes
- warning/success styles
- common widget styles

---

## 7.3 Launcher files

### `src/launcher/main.cpp`
Top-level launcher firmware entry point.

Should do:
- GPIO init
- ESP-NOW init
- subsystem setup
- non-blocking service loop

### `lib/launcher_core/radio_link.h/.cpp`
Launcher-side ESP-NOW stack.

Responsibilities:
- peer init
- receive callback
- packet validation
- command queueing
- status/fire-ack send helpers

### `lib/launcher_core/launcher_state.h/.cpp`
Owns the main launcher safety state machine.

Responsibilities:
- process arm/disarm/fire commands
- manage transitions DISARMED/ARMED/FIRING/FIRED/FAULT
- update `lastEvent` and `faultCode`

### `lib/launcher_core/igniter_driver.h/.cpp`
Controls the MOSFET ignition output.

Responsibilities:
- configure GPIO26
- start pulse
- end pulse
- emergency force-low method

### `lib/launcher_core/continuity.h/.cpp`
Continuity sensing and classification.

Responsibilities:
- ADC read
- threshold classify
- expose `ContinuityReading`
- provide `bool continuityOk()` helper

### `lib/launcher_core/safety_interlock.h/.cpp`
Reads the DaierTek arm/safe switch.

Responsibilities:
- debounce switch input
- expose `keySwitchOn()`
- detect change events for immediate disarm/status update

---

## 7.4 Test files

### `test/test_protocol/`
Unit tests for packet sizes, enum values, encode/decode.

### `test/test_stratagem_engine/`
Unit tests for:
- prefix matches
- full matches
- invalid input reset
- timeout behavior
- anti-repeat random selection

### `test/test_launcher_state/`
Unit tests for launcher transitions:
- arm allowed vs rejected
- fire rejected while disarmed
- continuity fault path
- post-fire return to DISARMED

### `test/test_continuity/`
Threshold classification tests using recorded ADC values.

---

# 8. Recommended Implementation Order

To reduce thrash, development should proceed in this order:

1. **Shared protocol and enums**
2. **Launcher state machine without live ignition**
3. **Wrist comms + status UI**
4. **Stratagem engine and active code selection**
5. **Confirm flow and fire command path with LED/dummy load**
6. **Continuity sensing**
7. **Actual MOSFET ignition pulse path**
8. **Power management and polish**

---

# 9. Non-Negotiable Safety Behavior Summary

These should be enforced in code comments and test cases.

## Wrist unit

- must not allow stratagem input unless launcher is online and ARMED
- must not show fire confirm unless active stratagem matched and 2-second delay elapsed
- must block fire if launcher status is stale (>5 s)
- must reset to IDLE on link loss or disarm

## Launcher unit

- must boot DISARMED
- must reject all unknown MACs
- must reject arm if hardware interlock is OFF
- must reject fire unless state == ARMED
- must perform continuity validation immediately before firing
- must force ignition GPIO LOW on any fault or disarm
- must never fire from a callback or malformed packet path

---

# 10. Minimal First-Pass Class/Module Map

If the team wants the simplest possible initial build, these are the core modules to implement first:

## Wrist
- `main.cpp`
- `launcher_link.*`
- `stratagem_engine.*`
- `ui_screens.*`
- `battery_monitor.*`

## Launcher
- `main.cpp`
- `radio_link.*`
- `launcher_state.*`
- `igniter_driver.*`
- `continuity.*`
- `safety_interlock.*`

## Shared
- `protocol.h`
- `protocol_types.h`
- `stratagems.h`
- `config_shared.h`

That set is enough to stand up a working arm/status/stratagem/fire architecture without overengineering the first revision.

---

# 11. Final Recommendation

Build this as a **shared PlatformIO monorepo with explicit state machines on both devices**.

The most important architectural decisions are:

- **shared protocol definitions** between wrist and launcher
- **strict launcher-owned safety state machine**
- **separate wrist stratagem state machine**
- **event-driven UI rendering from a central app state**
- **non-blocking timers everywhere**

If those are held, the project can grow cleanly into diagnostics, telemetry, GPS, and broader stratagem features without rewriting the launch path.
