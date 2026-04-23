# Launcher Wiring / Igniter Unit

## Purpose

This document describes the **launcher-side wiring** for the Helldivers Tac-Com Bracelet project using:

- **ESP32-WROOM-32** as the launcher controller
- **IRLZ44N N-channel MOSFET** as the igniter low-side switch
- **2S LiPo** as the dedicated igniter power source
- **DaierTek covered toggle switch** as a physical ARM/SAFE hardware interlock
- **Continuity-sense circuit** so the ESP32 can verify an igniter/load is connected before allowing a fire pulse
- **Separate ESP32 power rail** so the controller is not directly powered from the noisy/high-current igniter path

This is written to be detailed enough to breadboard or transfer to perfboard without ambiguity.

---

## 1. System Architecture Overview

The launcher should be treated as **two power domains sharing common ground**:

1. **Igniter rail**
   - Source: **2S LiPo** (7.4 V nominal, 8.4 V full)
   - Purpose: deliver launch current to the igniter
   - Switched by: **IRLZ44N MOSFET**
   - Protected by: **inline fuse**
   - Interlocked by: **DaierTek ARM/SAFE switch**

2. **Controller rail**
   - Source: either:
     - a small separate battery, or
     - the same 2S LiPo through a **buck regulator to 5 V or 3.3 V**
   - Purpose: power the ESP32 cleanly and reliably
   - Must be electrically cleaner than the igniter path

**Important:** Even if both rails originate from the same pack, the **ESP32 must not be powered directly from raw 2S LiPo voltage**. Use a proper regulator.

The MOSFET ignition path is a **low-side switch**:

- Igniter positive lead gets battery positive through fuse/interlock
- Igniter negative lead returns to MOSFET drain
- MOSFET source goes to system ground
- ESP32 drives MOSFET gate through a resistor

This is the simplest and most common way to switch igniters from a microcontroller.

---

## 2. Recommended Parts / Electrical Assumptions

### Core parts
- **ESP32-WROOM-32** module/dev board
- **IRLZ44N** logic-level N-channel MOSFET
- **330 ohm gate resistor** between ESP32 GPIO and MOSFET gate
- **10 k ohm gate pulldown resistor** from gate to GND
- **Inline fuse** on igniter battery positive
- **DaierTek lighted covered toggle switch** (ARM/SAFE interlock)
- **Banana jack / banana lead outputs** to clip onto standard hobby igniter leads
- **Buck converter** for ESP32 power if using same 2S pack

### Continuity circuit parts
A simple continuity-sense path can be built with:
- **100 k ohm resistor** from switched igniter positive to continuity sense node
- **4.7 k ohm resistor** from continuity sense node to the igniter negative/output side
- **100 k ohm resistor** from continuity sense node to ESP32 ADC input, or direct if layout is tight and you trust the node
- Optional: **0.1 uF capacitor** from ADC input to GND for filtering

This continuity scheme keeps current very low while producing a measurable voltage difference depending on whether an igniter is connected.

### Typical igniter assumptions
Per project spec, target is standard hobby rocket igniters for A-E motors.
- Typical resistance: roughly **1-2 ohms**
- Firing current: often several amps
- Therefore the main fire path must be built as a **short, low-resistance, high-current path**

### Fuse suggestion
Use a **fast automotive blade fuse or inline fuse** sized to protect wiring while still allowing igniter firing.
A practical starting point for bench testing is:
- **5 A fuse** if wiring is short and you are testing gently
- **7.5 A fuse** if 5 A proves too conservative for your igniter type

Start conservative and validate with a resistive dummy load first.

---

## 3. Full MOSFET Ignition Circuit

## 3.1 Fire-path concept
The high-current ignition path is:

**2S LiPo + → fuse → ARM switch contact → banana positive output → igniter → banana negative return → MOSFET drain → MOSFET source → battery -**

The ESP32 does **not** carry igniter current. It only drives the gate.

## 3.2 MOSFET connections
Using the **IRLZ44N**:

- **Gate**
  - Connect to **ESP32 GPIO26** through a **330 ohm resistor**
  - Also connect gate to **GND** through a **10 k ohm pulldown resistor**
  - Purpose of pulldown: keeps MOSFET OFF during boot/reset/floating GPIO states

- **Drain**
  - Connect to the **banana negative output terminal** / igniter return side

- **Source**
  - Connect directly to **system ground / battery negative**

### Why low-side switching here?
Because the ESP32 is 3.3 V logic and the IRLZ44N is an N-channel MOSFET, low-side switching is the straightforward configuration. It allows reliable gate drive without a dedicated high-side driver.

## 3.3 Gate drive behavior
- **GPIO26 LOW** → MOSFET OFF → no fire current path
- **GPIO26 HIGH (3.3 V)** → IRLZ44N turns ON → current flows through igniter

The gate resistor reduces current spikes into the MOSFET gate capacitance and helps suppress ringing.

## 3.4 Fire pulse timing
Firmware should assert GPIO26 HIGH for the configured firing window, for example:
- **default: 1 second**

After pulse completion:
- set GPIO26 LOW immediately
- transition launcher state to **FIRED** or **FAULT**, then back to **DISARMED**

Do not leave the MOSFET latched on longer than necessary.

---

## 4. DaierTek Covered Toggle Switch as ARM/SAFE Hardware Interlock

## 4.1 What the switch should do
Per spec, the DaierTek switch is **not the primary high-current switching element** and should be treated as a **hardware permission/interlock**.

It should do two things:

1. **Physically enable the igniter positive feed only when in ARM position**
2. **Provide a digital ARM status signal to the ESP32** so firmware knows whether remote arming is even allowed

That gives both:
- a real electrical interlock
- a software-visible interlock

## 4.2 Recommended use of the switch poles
If the DaierTek model has separate switch contacts plus LED terminals, wire it so:

### Contact path (interlock)
- **Common input of switch contact**: from fused 2S LiPo positive
- **Switched output of contact**: to banana positive output

This means when the switch is SAFE/OFF, the positive launch lead is physically disconnected from the battery.

### ESP32 ARM sense input
Also feed the switched side into a logic sense divider so the ESP32 knows whether the cover/switch is in ARM.

Example:
- Switched ARM output → **100 k / 47 k divider** → ESP32 GPIO input
- Bottom of divider to GND
- Divider midpoint to **ARM_SENSE GPIO**

Because switched ARM output may be as high as 8.4 V, it must be divided before entering the ESP32.

With a 100 k / 47 k divider:
- 8.4 V becomes about 2.69 V
- safely readable as a digital HIGH or ADC value

### LED indicator in the switch
If the switch includes an LED rated for 12 V operation:
- LED positive can be tied to the **switched ARM output**
- LED negative to **GND**

That way the switch lights only when the launcher is physically hot/armed.

**Important:** Confirm the exact DaierTek pinout because illuminated toggle switches vary. Some have separate LED pins; others share ground/common internally. Verify with the datasheet or a multimeter before final wiring.

## 4.3 Safety behavior of the interlock
Recommended firmware rule:
- If physical ARM switch is OFF, launcher state is forced to **DISARMED**
- Any remote ARM command is ignored unless switch is ON
- If switch is turned OFF while armed, immediately:
  - clear armed state
  - drive MOSFET gate LOW
  - report DISARMED

This is the correct behavior for a hard interlock.

---

## 5. Continuity Check Circuit for ESP32 ADC

## 5.1 Purpose
The continuity circuit lets the ESP32 determine whether an igniter is connected across the banana outputs **without delivering enough current to fire it**.

This must be a **very low current** measurement path.

## 5.2 Simple continuity-sense topology
Use a very small current injection from the **switched igniter positive node** to a sense node, then observe how that node behaves depending on whether the igniter path exists.

### Connections
Define nodes:
- **ARMED_POS** = positive launch rail after fuse and ARM switch
- **IGN_NEG** = negative launch output / MOSFET drain node
- **CONT_ADC** = ESP32 ADC input pin
- **CONT_NODE** = analog sense node

Wire as follows:

1. **100 k ohm resistor** from **ARMED_POS** to **CONT_NODE**
2. **4.7 k ohm resistor** from **CONT_NODE** to **IGN_NEG**
3. **CONT_NODE** to **ESP32 ADC pin** (direct or through ~1 k if desired)
4. Optional **0.1 uF capacitor** from CONT_NODE or ADC pin to GND for filtering

## 5.3 How it works
### When no igniter is connected
- IGN_NEG is mostly floating because the MOSFET is OFF
- CONT_NODE is pulled upward through the 100 k resistor
- ADC reads a relatively higher voltage

### When an igniter is connected
- There is now a resistive path from banana positive to banana negative through the igniter
- CONT_NODE is influenced through the 4.7 k link to IGN_NEG
- Because the igniter is low resistance, the negative side is referenced toward the positive side through the igniter, creating a distinct ADC reading different from open-circuit

In practice, firmware should not rely on theoretical thresholds alone. It should:
- sample ADC with **no igniter attached** and store/open-circuit range
- sample ADC with a known good igniter or equivalent resistor and store/continuity range
- choose a threshold with margin

## 5.4 Better practical interpretation
For real build reliability, continuity detection should be treated as **calibrated analog classification**, not an absolute one-size-fits-all voltage.

Recommended firmware logic:
- Take 8-16 ADC samples and average them
- Compare against two thresholds:
  - **OPEN** range
  - **CONNECTED** range
- If reading is ambiguous, report **FAULT**

## 5.5 Continuity current estimate
At 8.4 V through 100 k ohm, maximum injected current is approximately:
- **84 microamps**

That is far below igniter firing current, which is the whole point.

## 5.6 Alternative continuity method
If bench testing shows the ADC separation is poor, a more robust version would add:
- a second divider to monitor banana positive directly
- or a transistor/comparator-based continuity detector

But the resistor/ADC method above is acceptable as a first pass and fits the project spec.

---

## 6. ESP32 Power Circuit (Separate from Igniter Rail)

## 6.1 Requirement
The ESP32 must have a **regulated supply** independent of the raw igniter path.

There are two reasonable options:

### Option A - Same 2S LiPo, separate regulated branch
Recommended for compactness.

Wiring:
- 2S LiPo positive → separate branch → **buck converter**
- 2S LiPo negative → buck converter GND
- Buck output set to either:
  - **5 V** into ESP32 dev board 5V/VIN pin, or
  - **3.3 V** into a proper 3.3 V rail if using bare module and you know what you are doing

### Option B - Separate small battery for controller
Best electrical isolation, but more parts and more charging overhead.

## 6.2 Strong recommendation
If using a dev board version of the ESP32-WROOM-32:
- use a **buck converter set to 5.0 V**
- feed the board’s **5V/VIN** pin
- let the board’s onboard regulator generate 3.3 V for the module

This is simpler and safer than trying to feed the bare 3.3 V rail directly.

## 6.3 Grounding
Even though the controller rail is separate from the high-current branch, the grounds must join at a common point:
- battery negative
- ESP32 GND
- MOSFET source
- buck converter GND

Use a **star-ground mindset** if possible:
- keep the heavy igniter return path short and thick
- keep ESP32 ground/reference wiring separate until near the battery negative/common node

This helps prevent voltage bounce and false resets during firing.

## 6.4 Decoupling
Recommended local filtering near the ESP32:
- **10 uF electrolytic or tantalum** across 3.3 V and GND
- **0.1 uF ceramic** close to ESP32 supply pins
- If using a dev board, it likely already has some bulk capacitance, but extra local decoupling is still a good idea

---

## 7. Suggested ESP32 Pin Assignments

These are practical assignments for the launcher controller.

| Function | ESP32 Pin | Type | Notes |
|----------|-----------|------|-------|
| MOSFET gate / FIRE output | **GPIO26** | Digital output | Recommended by spec; drives IRLZ44N gate through 330 ohm |
| ARM switch sense | **GPIO27** | Digital input | Reads physical ARM/SAFE interlock through voltage divider |
| Continuity ADC | **GPIO34** | ADC input only | Good choice because input-only pin suits analog sense |
| Status LED (optional) | **GPIO2** or **GPIO25** | Digital output | Use carefully if boot constraints matter; GPIO25 is a cleaner choice |
| Battery monitor for controller rail (optional) | **GPIO35** | ADC input only | For reading divided battery voltage |

### Notes on pin choices
- **GPIO26** is a good fire output because it has no major boot strap headaches
- **GPIO34/GPIO35** are input-only pins and are good ADC candidates
- Avoid using boot-sensitive pins for anything that could accidentally hold the ESP32 in the wrong boot state at power-up

---

## 8. Full Point-to-Point Wiring List

## 8.1 Igniter high-current path
1. **2S LiPo positive** → **inline fuse input**
2. **Fuse output** → **DaierTek switch common/input contact**
3. **DaierTek switched contact output** → **banana positive terminal**
4. **Banana positive terminal** → one igniter lead
5. Other igniter lead → **banana negative terminal**
6. **Banana negative terminal** → **IRLZ44N drain**
7. **IRLZ44N source** → **battery negative / common ground**

## 8.2 MOSFET gate wiring
1. **ESP32 GPIO26** → **330 ohm resistor**
2. Other side of 330 ohm resistor → **IRLZ44N gate**
3. **IRLZ44N gate** → **10 k ohm resistor** → **GND**

## 8.3 Physical ARM sense wiring
1. **DaierTek switched ARM output** → **100 k ohm resistor** → ARM sense divider midpoint
2. ARM sense divider midpoint → **ESP32 GPIO27**
3. ARM sense divider midpoint → **47 k ohm resistor** → **GND**

This lets the ESP32 detect whether the physical ARM switch is ON.

## 8.4 Switch LED wiring
Assuming separate LED pins:
1. **DaierTek switched ARM output** → LED+
2. LED- → **GND**

If the switch’s LED wiring differs, follow its actual datasheet pinout.

## 8.5 Continuity sense wiring
1. **ARMED_POS** (same node as banana positive after switch) → **100 k ohm resistor** → **CONT_NODE**
2. **CONT_NODE** → **GPIO34 ADC**
3. **CONT_NODE** → **4.7 k ohm resistor** → **banana negative / MOSFET drain node**
4. Optional: **CONT_NODE** → **0.1 uF capacitor** → **GND**

## 8.6 ESP32 power wiring
Using same 2S LiPo through a buck converter:
1. **2S LiPo positive** → buck converter VIN+
2. **2S LiPo negative** → buck converter VIN-
3. Buck output +5 V → ESP32 board **5V/VIN**
4. Buck GND → ESP32 **GND**

If using a bare ESP32 module instead of dev board, use a proper 3.3 V regulator and follow Espressif power design guidance.

---

## 9. ASCII Wiring Diagram

```text
                           HELLDIVERS TAC-COM LAUNCHER

                  +-------------------- 2S LiPo 7.4V nominal -------------------+
                  |                                                              |
                  |                                                              |
             Battery +                                                       Battery -
                  |                                                              |
                  |                                                              +------------------+
                  |                                                                                 |
               [FUSE]                                                                               |
                  |                                                                                 |
                  +-------> to buck converter VIN+                                                  |
                  |                                                                                 |
          +-------+--------+                                                                        |
          | DaierTek ARM   |                                                                        |
          | toggle contact |                                                                        |
          |   SAFE / ARM   |                                                                        |
          +-------+--------+                                                                        |
                  |  (ARMED_POS)                                                                    |
                  |                                                                                 |
                  +------------------------------+-----------------------+                           |
                  |                              |                       |                           |
                  |                              |                       |                           |
          Banana + output                ARM sense divider       Continuity inject R1               |
                  |                      100k / 47k              100k                               |
                  |                              |                       |                           |
               [Igniter]                         +--> GPIO27             +----> CONT_NODE ---> GPIO34
                  |                              |                       |              |
                  |                             GND                    R2 4.7k         [0.1uF opt]
          Banana - output                                                 |              |
                  |                                                       |             GND
                  |                                                       |
               Drain ----------------------------------------------+------+
                  |                                               |
               IRLZ44N                                            |
                  |                                               |
               Source ---------------------------------------------+-------------------- GND/common
                  |
                 GND

ESP32 FIRE CONTROL:

   GPIO26 ---[330R]--- Gate(IRLZ44N)
                         |
                       [10k]
                         |
                        GND

ESP32 POWER:

   2S LiPo + ----> buck converter ----> +5V to ESP32 VIN/5V
   2S LiPo - --------------------------> GND to ESP32 GND

DaierTek LED (if separate pins and 12V-style internal resistor is present):

   ARMED_POS ----> LED+
   GND ----------> LED-
```

---

## 10. Breadboard / Build Notes

## 10.1 For bench-only proof of concept
You can breadboard the **logic side**:
- ESP32
- gate resistor
- gate pulldown
- continuity ADC circuit
- ARM sense divider

But the **actual igniter current path** should move off solderless breadboard quickly.

Reason:
- standard breadboards are not ideal for multi-amp pulse current
- contact resistance can be inconsistent
- they are not a good long-term fit for pyrotechnic/igniter circuits

For live-current testing, use:
- screw terminals
- ring/fork terminals
- XT30/XT60 style battery connector
- appropriately sized wire
- perfboard or terminal block mounting

## 10.2 Suggested wire sizing
For short launcher leads and battery wiring:
- **18 AWG** is a good practical starting point
- **16 AWG** if you want extra margin in the battery-to-output path

Keep the high-current path short.

## 10.3 Dummy load testing
Before connecting real igniters, test with:
- power resistor load
- automotive lamp
- other non-pyrotechnic resistive load

**Current recommended first dummy-load checkpoint for this repo:**
- `10 ohm` power resistor
- `10W minimum`
- `20W preferred`
- connect it across the launcher output nodes (`ARMED_POS` to `IGN_NEG`)
- use the existing `2S` launcher rail with the `IRLZ44N` low-side switch on `GPIO26`

At `8.4V` full charge, this is about `0.84A` and roughly `7W`, which keeps the first MOSFET bench slice controlled without pretending it is a live igniter-current validation.

Verify:
- ARM switch behavior
- ADC continuity classification
- MOSFET switching cleanly
- no unexpected reset of ESP32 during pulse

---

## 11. Firmware Interaction Expectations

Hardware and firmware should work together like this:

### Power-up
- ESP32 boots
- GPIO26 initialized LOW
- system state = DISARMED
- continuity checked but not trusted for fire until armed

### ARM attempt
- Wrist sends ARM command
- ESP32 first checks **GPIO27 ARM_SENSE**
- If physical switch OFF: reject ARM command
- If physical switch ON: set logical state to ARMED

### Fire attempt
Before driving MOSFET:
1. confirm logical ARMED state
2. confirm ARM_SENSE still active
3. confirm continuity ADC indicates valid igniter
4. then pulse GPIO26 HIGH

### Forced disarm
If ARM switch opens at any time:
- GPIO26 LOW immediately
- state returns to DISARMED
- report DISARMED/FAULT depending on timing

---

## 12. Safety Notes Specific to This Circuit

1. **Treat the launcher as live whenever a battery is connected.**
   Even when software says DISARMED, the safest assumption is that a wiring fault could exist.

2. **Default-safe gate behavior is mandatory.**
   The 10 k gate pulldown is not optional. Without it, the MOSFET gate may float during boot and could partially turn on.

3. **Use the physical ARM interlock as the real go/no-go control.**
   Remote arming alone is not enough. The switch must be physically present and required for launch enable.

4. **Fuse the battery positive lead close to the pack.**
   If wiring shorts downstream, the fuse is what stops the battery from dumping dangerous current into the fault.

5. **Do not use a solderless breadboard for field firing.**
   Bench logic testing only. Final launch hardware should use secure mechanical connections.

6. **Keep igniter leads disconnected until final pad setup.**
   Do continuity and logic tests with dummy loads whenever possible.

7. **Do not trust continuity alone as proof of safe status.**
   Continuity circuits can be fooled by corrosion, partial contact, or clip issues. Use it as a diagnostic, not an excuse to skip range discipline.

8. **Expect ground noise when firing.**
   Keep high-current wiring physically separate from ESP32 signal wiring. Shared ground is necessary; shared routing is not.

9. **Verify actual DaierTek pinout before applying battery power.**
   Illuminated switches are notorious for non-obvious pinouts. Meter it first.

10. **Bench with no igniter first, then dummy resistor, then real igniter.**
    Do not jump straight to live pyrotechnic testing.

11. **Comms loss must never equal fire.**
    Firmware should fail passive: stay disarmed or stay in current safe state, never auto-trigger.

12. **Follow club/range rules.**
    This design may be technically workable and still not be acceptable at a given field without range officer approval.

---

## 13. Suggested First Bench Bring-Up Sequence

1. Build ESP32 power rail only
2. Confirm ESP32 boots reliably from buck converter
3. Add ARM sense divider and verify switch readings on GPIO27
4. Add MOSFET gate wiring only, with no igniter battery attached
5. Confirm GPIO26 toggles gate as expected
6. Add high-current path with a dummy resistive load instead of igniter
   - recommended first pass: `10 ohm`, `10W+`, across `ARMED_POS` and `IGN_NEG`
7. Confirm MOSFET switches load correctly
8. Add continuity circuit and log ADC values for:
   - open clips
   - dummy resistor
   - actual igniter type
9. Tune firmware thresholds
10. Only after all of that, move toward controlled live igniter testing

---

## 14. Summary

This launcher circuit should be built around:

- **IRLZ44N low-side MOSFET switching** for the igniter
- **GPIO26** as the fire output through a **330 ohm gate resistor**
- **10 k pulldown** on the MOSFET gate for safe boot behavior
- **Fused 2S LiPo igniter rail**
- **DaierTek covered toggle** as a physical ARM/SAFE interlock on the positive launch lead plus ARM sense input to the ESP32
- **Very-low-current ADC continuity network** on the banana outputs
- **Separate regulated ESP32 power branch** using a buck converter rather than raw battery voltage

That layout matches the project spec and is realistic to prototype on the bench before moving to a more permanent launcher assembly.
