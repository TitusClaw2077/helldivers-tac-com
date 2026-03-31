# Helldivers Tac-Com Bracelet — Bill of Materials (BOM)

*Prepared from `PROJECT_SPEC.md` on 2026-03-30*

## Scope
This BOM covers the complete **wrist unit** and **launcher unit** needed for the current project definition, plus the small parts, wiring, prototyping supplies, and recommended dev/setup tools needed to actually build, test, and field the system.

**Important note:** this project controls a **wireless rocket ignition system**. The BOM below follows the project spec, but live use still requires range-safety review, local club/range approval, proper igniter handling, and staged testing with resistive dummy loads before any live igniter tests.

---

## Assumptions used for this BOM
- Qty values are for **one complete system**: **1 wrist unit + 1 launcher unit**.
- Pricing is **estimated street price in USD** and will move around.
- Where a direct Amazon product was identifiable, the BOM includes an **ASIN or direct product link**.
- Where a specific ASIN was not reliably confirmed, I included a **purchase/search link** to a known source or Amazon search page.
- Some line items are bought in packs; unit prices below are estimated per usable item, with notes calling out when the real purchase comes as a multi-pack.

---

## 1) Wrist Unit BOM

| Item | Qty | Recommended Part / Model | Link / ASIN | Est. Unit Price | Why this part was chosen |
|---|---:|---|---|---:|---|
| ESP32 touchscreen HMI board | 1 | **ELECROW CrowPanel ESP32 3.5" 480x320 resistive touch display** | ASIN **B0FXLB5CFL** — <https://www.amazon.com/ELECROW-ESP32-Display-3-5-Touchscreen/dp/B0FXLB5CFL> | $35-$45 | This is the exact display platform called out in the spec. It combines ESP32, screen, touch, battery support, speaker, and GPIO in one board, which keeps the wrist build sane. |
| Wrist battery | 1 | **1S LiPo 3.7V 1000mAh** with JST-PH 2.0 plug | Amazon search: <https://www.amazon.com/s?k=1S+1000mAh+LiPo+JST+PH+2.0> | $8-$14 | Matches the spec target for about 3-4 hours runtime and fits typical wearable power needs without getting too bulky. |
| LiPo extension / adapter lead | 1 | **JST-PH 2.0 extension lead**, 22-26 AWG | Amazon search: <https://www.amazon.com/s?k=JST+PH+2.0+extension+cable> | $2-$4 | Gives flexibility in enclosure layout so the battery does not have to sit awkwardly right beside the panel PCB. |
| USB-C cable for charging/programming | 1 | **USB-C data cable**, 1-3 ft | Amazon search: <https://www.amazon.com/s?k=usb-c+data+cable+short> | $4-$8 | Required for firmware upload, serial logging, and charging. A short cable is nicer at the bench. |
| Wrist power switch (optional but strongly recommended) | 1 | **SPST inline slide switch** or panel mini rocker | Amazon search: <https://www.amazon.com/s?k=small+inline+slide+switch+22awg> | $1-$3 | The board can run directly from battery, but a dedicated power disconnect makes the wearable less annoying to store and safer to transport. |
| Wrist speaker / buzzer (optional depending on CrowPanel accessory support) | 1 | Small **8Ω speaker** or active buzzer if onboard audio is insufficient | Amazon search: <https://www.amazon.com/s?k=8ohm+mini+speaker+arduino> | $2-$5 | For Helldivers-style UI feedback. Only needed if the onboard speaker path is absent or not loud enough in the final enclosure. |
| Hookup wire, flexible | 1 set | **26 AWG silicone wire assortment** | Amazon search: <https://www.amazon.com/s?k=26awg+silicone+wire+kit> | $8-$12 | Flexible silicone wire is much better than stiff PVC wire for a wearable enclosure. |
| Small JST / Dupont jumper assortment | 1 set | JST-PH / Dupont mixed jumper kit | Amazon search: <https://www.amazon.com/s?k=dupont+jumper+wire+kit> | $6-$10 | Useful for initial breadboard validation and any accessory wiring off the CrowPanel GPIO breakout. |
| Fasteners for enclosure | 1 set | **M2/M2.5 machine screws + heat-set inserts** | Amazon search: <https://www.amazon.com/s?k=m2.5+heat+set+inserts+kit> | $8-$15 | Needed if the 3D printed wrist enclosure uses screw closure instead of glue. Heat-set inserts make rework much easier. |
| Wrist strap / mounting hardware | 1 | **Velcro strap**, nylon watch-style strap, or elastic band solution | Amazon search: <https://www.amazon.com/s?k=hook+and+loop+strap+1+inch> | $5-$10 | Not glamorous, but necessary. A wearable build that does not actually mount comfortably is dead on arrival. |
| 3D printing filament for enclosure | ~0.25 kg | **PETG** or **PLA+** | Amazon search: <https://www.amazon.com/s?k=petg+filament> | $5-$8 used from spool | PETG is preferable for a tougher wearable shell; PLA+ is fine for early fitment prototypes. |
| Screen protection (optional) | 1 | Universal **3.5" resistive touchscreen protector film** | Amazon search: <https://www.amazon.com/s?k=3.5+inch+touch+screen+protector> | $2-$5 | Helps preserve the touchscreen if the wrist unit is used outdoors or transported in a gear bag. |

---

## 2) Launcher Unit BOM

| Item | Qty | Recommended Part / Model | Link / ASIN | Est. Unit Price | Why this part was chosen |
|---|---:|---|---|---:|---|
| Launcher MCU | 1 | **ESP32 ESP-WROOM-32 dev board** (CP2102 or CH340 USB-UART is fine) | Example ASIN **B07WCG1PLV** — <https://www.amazon.com/ESP-WROOM-32-Development-Dual-Mode-Microcontroller-Integrated/dp/B07WCG1PLV> | $7-$12 | Matches the spec and gives a simple breadboard-friendly platform for ESP-NOW, GPIO26 control, continuity ADC, and status handling. |
| Launcher battery | 1 | **2S LiPo 7.4V nominal**, ideally **1000-2200mAh**, XT60-equipped | MotionRC example: <https://www.motionrc.com/products/admiral-1000mah-2s-7-4v-30c-lipo-battery-with-xt60-connector-epr10002x6> or Amazon search: <https://www.amazon.com/s?k=2S+LiPo+XT60> | $15-$28 | The spec calls for a dedicated 2S rail for igniter current. 1000mAh is plenty; 1500-2200mAh gives more margin and less voltage sag. |
| LiPo charger / balance charger | 1 | **2S-capable smart LiPo charger** (SkyRC, HTRC, Tenergy, etc.) | Amazon search: <https://www.amazon.com/s?k=2s+lipo+battery+charger+balance> | $20-$45 | Mandatory. A proper balance charger is not optional with LiPo packs. |
| Main switching MOSFET | 1 | **IRLZ44N** logic-level N-channel MOSFET (TO-220) | Amazon search/product family: <https://www.amazon.com/s?k=IRLZ44N+logic+level+MOSFET> | $1-$3 each | Specifically called out in the spec. Logic-level gate drive works from ESP32 GPIO, and the part has far more current capacity than needed here. |
| MOSFET spare | 1 | Second **IRLZ44N** spare transistor | Same as above | $1-$3 | Worth having because MOSFETs are cheap and easy to sacrifice during early prototyping or wiring mistakes. |
| Gate resistor | 1 | **330Ω 1/4W resistor** | Amazon search: <https://www.amazon.com/s?k=330+ohm+1%2F4w+resistor+kit> | <$0.10 | Explicitly called for in the spec to protect the ESP32 GPIO and tame gate charging current. |
| Gate pulldown resistor | 1 | **10kΩ 1/4W resistor** | Amazon search: <https://www.amazon.com/s?k=10k+ohm+1%2F4w+resistor+kit> | <$0.10 | Strongly recommended even though not explicitly named in the spec. Keeps the MOSFET firmly off during boot/reset so the igniter path does not float. |
| Continuity sense resistor | 1 | **1kΩ 1/4W resistor** (or value finalized in schematic) | Amazon search: <https://www.amazon.com/s?k=1k+ohm+1%2F4w+resistor+kit> | <$0.10 | Provides a low-current continuity test path that the ESP32 ADC can read without risking accidental ignition current. Final value should be confirmed in schematic review. |
| Flyback / protection diode (general rail protection) | 1 | **1N5819 Schottky** or **1N4007** | Amazon search: <https://www.amazon.com/s?k=1N5819+diode+kit> | <$0.20 | An igniter is not a motor coil, so classic flyback needs are limited, but adding diode-based rail protection is cheap insurance against wiring mistakes and transients. |
| Inline fuse holder | 1 | **ATO/ATC inline blade fuse holder** with leads | Amazon search: <https://www.amazon.com/s?k=inline+blade+fuse+holder> | $3-$6 | The spec requires a fuse in the ignition rail. This is the cleanest field-serviceable option. |
| Main fuse | 1 | **5A automotive blade fuse** to start bench testing; adjust after validation | Amazon search: <https://www.amazon.com/s?k=5a+blade+fuse> | <$1 | Enough to protect wiring and the pack while still allowing igniter current. Actual fuse rating should be validated against measured igniter pulse current. |
| Arming / safety cover switch | 1 | **DaierTek illuminated covered toggle switch** | ASIN **B07T6XJF1T** (regional listings confirmed) | $8-$12 | This is the exact switch specified. It adds the required physical ARM interlock and provides a visible armed/hot indicator. |
| Physical key switch (recommended to satisfy spec language) | 1 | **2-position key switch**, SPST or SPDT, panel mount | Amazon search: <https://www.amazon.com/s?k=12v+panel+mount+key+switch> | $8-$15 | The safety section explicitly mentions a physical key switch as the range-officer control point. Adding one in series with the arming logic is the right move. |
| Launch lead connectors | 2 | **4 mm banana sockets/binding posts** panel mount, red + black | Amazon search: <https://www.amazon.com/s?k=4mm+banana+socket+panel+mount> | $1-$3 each | Clean and familiar interface for detachable launch leads. Better than hard-wiring pad leads into the box. |
| Launch leads | 1 pair | **18 AWG silicone banana-to-alligator leads** or custom banana plug leads ending in micro clips | Amazon search: <https://www.amazon.com/s?k=banana+plug+alligator+test+lead> | $6-$12 | Needed to connect the launcher box to the igniter at the pad. Silicone jacket holds up better outdoors. |
| Banana plugs (if custom leads are built) | 2 | 4 mm stackable banana plugs | Amazon search: <https://www.amazon.com/s?k=4mm+banana+plug+kit> | $1-$2 each | Lets you build custom-length leads instead of accepting generic test-lead lengths. |
| Alligator clips / micro clips for igniters | 2 | Small insulated clips for Estes-style igniter legs | Amazon search: <https://www.amazon.com/s?k=mini+alligator+clip+leads> | $3-$7 | Needed at the igniter end unless the launch leads already include suitable clips. |
| Launcher enclosure | 1 | ABS project box, approx. **6x4x2 in** minimum | Amazon search: <https://www.amazon.com/s?k=abs+project+box+6x4x2> | $10-$18 | Enough room for ESP32 board, battery connector, switch gear, fuse, and cable management. Weather-resistant is a plus. |
| Battery connector pigtail | 1 | **XT60 pigtail**, 14-16 AWG | Amazon search: <https://www.amazon.com/s?k=xt60+pigtail> | $2-$4 | Most 2S packs in the useful size range ship with XT60. A pigtail makes the launcher box wiring clean and replaceable. |
| Power distribution wire | 1 set | **16-18 AWG silicone wire** red/black | Amazon search: <https://www.amazon.com/s?k=18awg+silicone+wire> | $8-$12 | Appropriate for short, low-voltage high-current launch wiring. Thicker than signal wiring, less voltage drop, more durable. |
| Signal wiring | 1 set | **22-24 AWG stranded hookup wire** | Amazon search: <https://www.amazon.com/s?k=22awg+stranded+hookup+wire> | $6-$10 | Cleaner for ESP32 logic, continuity sense, switch wiring, and status LEDs. |
| Status LED (optional extra) | 1 | 5 mm red/green panel LED or dual-color LED | Amazon search: <https://www.amazon.com/s?k=panel+mount+red+green+led> | $2-$5 | The DaierTek illuminated switch already helps, but a dedicated status LED can still be useful for DISARMED/ARMED/FAULT indication during debug. |
| LED current-limiting resistor | 1 | **220Ω to 1kΩ resistor** depending on LED used | Amazon search: <https://www.amazon.com/s?k=220+ohm+resistor+kit> | <$0.10 | Standard LED support part if an external status LED is added. |
| Terminal block / lever nuts | 2-4 | Small **2-position terminal blocks** or compact Wago-style connectors | Amazon search: <https://www.amazon.com/s?k=2+pin+terminal+block+connector> | $0.50-$1 each | Makes internal box wiring and rework much less painful than soldering every heavy wire joint permanently. |
| Heat shrink tubing | 1 kit | Adhesive-lined mixed heat shrink kit | Amazon search: <https://www.amazon.com/s?k=adhesive+lined+heat+shrink+kit> | $8-$12 | Needed for field-safe wiring. This is not the place for exposed joints. |
| Breadboard / perfboard for prototype circuit | 1 | Solderable perfboard or proto PCB | Amazon search: <https://www.amazon.com/s?k=perfboard+prototype+pcb> | $3-$8 | The spec still has the schematic TBD. Breadboard first, then move to perfboard or custom PCB once validated. |
| Mounting hardware | 1 set | M3 screws, standoffs, washers, nylon spacers | Amazon search: <https://www.amazon.com/s?k=m3+standoff+kit> | $6-$12 | Needed to mechanically secure the ESP32 dev board, fuse holder, and possibly the perfboard in the launcher enclosure. |

---

## 3) Shared Build / Prototyping Consumables

These are the things people forget until they stop the build.

| Item | Qty | Recommended Part / Model | Link / ASIN | Est. Unit Price | Why this part was chosen |
|---|---:|---|---|---:|---|
| Solder | 1 spool | 63/37 rosin-core electronics solder | Amazon search: <https://www.amazon.com/s?k=63%2F37+rosin+core+solder> | $10-$20 | Easier wetting and cleaner joints than bargain no-name solder. |
| Flux pen | 1 | No-clean flux pen | Amazon search: <https://www.amazon.com/s?k=no+clean+flux+pen> | $6-$10 | Makes ESP32 and connector work cleaner and less frustrating. |
| Soldering iron / station | 1 | Pinecil, Hakko, or comparable temp-controlled iron | Amazon search: <https://www.amazon.com/s?k=pinecil+soldering+iron> | $25-$80 | Needed if the build is not already equipped. A bad iron will waste time fast. |
| Multimeter | 1 | Basic digital multimeter with continuity mode | Amazon search: <https://www.amazon.com/s?k=digital+multimeter> | $15-$40 | Mandatory for continuity testing, battery voltage checks, and validating the igniter circuit before connecting any live igniter. |
| Bench dummy load | 1 | Power resistor assortment, e.g. **1Ω-10Ω high-watt resistors** | Amazon search: <https://www.amazon.com/s?k=cement+power+resistor+5w+10w+kit> | $8-$15 | Strongly recommended for safe bench simulation of igniter loads during Phase 2 and Phase 4 testing. |
| Breadboards | 1-2 | Full-size solderless breadboard | Amazon search: <https://www.amazon.com/s?k=full+size+breadboard> | $5-$10 | Needed for early continuity and MOSFET logic prototyping, though do not run high igniter currents through a breadboard long-term. |
| Jumper leads | 1 kit | Male-male / male-female Dupont kit | Amazon search: <https://www.amazon.com/s?k=dupont+jumper+wires> | $5-$8 | Standard bench glue for quick test setups. |
| Zip ties / cable lacing | 1 pack | Small cable ties | Amazon search: <https://www.amazon.com/s?k=small+zip+ties> | $4-$7 | Cheap, boring, necessary. |
| Label maker tape / heat shrink labels | optional | Wire ID labels | Amazon search: <https://www.amazon.com/s?k=wire+label+kit> | $5-$15 | Helpful because launcher wiring gets confusing surprisingly fast once safety interlocks are added. |

---

## 4) Rocket / Igniter Interface Items

These are outside the electronics core, but they are required for full system use.

| Item | Qty | Recommended Part / Model | Link / ASIN | Est. Unit Price | Why this part was chosen |
|---|---:|---|---|---:|---|
| Standard model rocket igniters | as needed | **Estes-style standard igniters** included with A-E motors | Buy from normal hobby source / motor packs | included with motors or ~$1-$3 each equivalent | Matches the project spec. The launcher should be tuned and tested against the actual igniters intended for field use. |
| Spare igniters for test firing | 5-10 | Estes or equivalent spare igniters | Amazon/hobby search varies | $5-$15 pack | Useful for non-flight live-fire testing once the electrical system is proven safe with dummy loads. |
| Launch pad / rod system | 1 | Existing low-power rocket launch pad setup | Existing gear or hobby vendor | varies | Not part of the electronics box itself, but obviously required for actual flights. |
| Range safety key / remove-before-flight tag (recommended) | 1 | Key lanyard / safety flag | Amazon search: <https://www.amazon.com/s?k=remove+before+flight+tag> | $3-$6 | Makes the launcher’s physical safe/arm control harder to forget in the field. |

---

## 5) Recommended Software / Development Tools

These are not hardware BOM line items in the strict sense, but they are part of the real build.

| Tool | Cost | Why it is recommended |
|---|---:|---|
| **PlatformIO** (VS Code extension) | Free | Best fit for this project. Clean ESP32 library management, board configs, serial monitor, and easier long-term maintainability than juggling Arduino IDE sketches manually. |
| **Arduino framework for ESP32** | Free | Matches the project spec and is the fastest route to a working ESP-NOW + display stack. |
| **LVGL** | Free | Right choice for the stratagem UI, status panels, and future multi-screen expansion. |
| **ESP32 board support packages** | Free | Required for build/upload/debug. |
| **KiCad** | Free | Strongly recommended for the launcher schematic and any later custom PCB. The spec explicitly says the schematic is still TBD. |
| **Serial monitor / logic logging** | Free | Use PlatformIO serial monitor or equivalent for message traces and state-machine debugging. |
| **Git** | Free | Worth using from the start since there are two firmware targets and safety logic that should be versioned carefully. |

---

## 6) Recommended Purchase Strategy

If I were buying this for a first build, I would split it like this:

### Buy first (core hardware)
1. ELECROW CrowPanel ESP32 3.5" (**B0FXLB5CFL**)
2. ESP32-WROOM-32 dev board
3. 1S 1000mAh LiPo for wrist
4. 2S LiPo + proper balance charger for launcher
5. IRLZ44N MOSFET pack
6. DaierTek covered arming switch (**B07T6XJF1T**)
7. Fuse holder + blade fuses
8. Banana sockets + launch lead clips
9. Basic resistor assortment, wire, perfboard, heat shrink
10. Project box

### Buy second (nice-to-have / cleanup)
1. Key switch
2. Better enclosure hardware
3. Screen protector
4. Extra status LEDs
5. Spare MOSFETs / spare ESP32 board
6. Better custom leads and nicer connectorization

---

## 7) Suggested Spares

These will save time later:
- **2x ESP32 dev boards total** instead of 1
- **5-10x IRLZ44N MOSFETs** instead of buying single pieces
- Mixed resistor kit (330Ω, 1kΩ, 10kΩ, 220Ω covered)
- Extra XT60 pigtails
- Extra launch lead clips
- Spare 5A and 7.5A blade fuses
- One extra 1S LiPo and one extra 2S LiPo if the project becomes a regular field toy

---

## 8) Estimated Total Cost

### Core build estimate
- **Wrist unit:** about **$70-$115** depending on battery, strap, and enclosure choices
- **Launcher unit:** about **$85-$170** depending heavily on battery/charger choice and enclosure hardware
- **Shared consumables/tools:** anywhere from **$30** if already stocked to **$150+** if starting from scratch

### Realistic total
For someone who already owns soldering tools and a charger, the project should land around:
- **~$160-$260 total** for one complete working system

If charger/tools also need to be bought:
- **~$230-$400 total** is a more honest all-in range

---

## 9) Notes / Engineering Recommendations

1. **Use a gate pulldown resistor.** The spec mentions the 330Ω gate resistor, but the **10k pulldown** is just as important so the launcher powers up in a known-safe OFF state.
2. **Add the key switch even though the DaierTek covered toggle is already specified.** The safety section explicitly calls for a physical key switch as the range-control point. I would wire both into the arming logic path.
3. **Do continuity checks with a deliberately current-limited path only.** The continuity circuit should be designed so it cannot accidentally preheat or fire a real igniter.
4. **Do not rely on breadboards for final igniter current.** Fine for bench logic validation; not fine for final field-current wiring.
5. **Choose 18 AWG or thicker for the launch-current path.** It is overkill in a good way.
6. **Prefer PETG for outdoor printed parts.** PLA will work for prototypes, but a field box left in a car or sun can get sketchy fast.
7. **Do a dummy-load test phase before any igniter is ever connected.** The spec already says this; it is the right call.
8. **Label ARM / SAFE / KEY positions clearly on the enclosure.** This build should be impossible to misread when people are excited at the pad.

---

## 10) Short Shopping Checklist

### Wrist
- [ ] CrowPanel ESP32 3.5" (**B0FXLB5CFL**)
- [ ] 1S 1000mAh LiPo
- [ ] USB-C cable
- [ ] Wire / small connectors
- [ ] Wrist enclosure + strap hardware

### Launcher
- [ ] ESP32-WROOM-32 dev board
- [ ] 2S LiPo
- [ ] LiPo charger
- [ ] IRLZ44N MOSFETs
- [ ] 330Ω, 1kΩ, 10kΩ resistors
- [ ] Diodes
- [ ] Fuse holder + fuses
- [ ] DaierTek safety-cover toggle (**B07T6XJF1T**)
- [ ] Key switch
- [ ] Banana sockets / launch leads / clips
- [ ] Project box
- [ ] XT60 pigtail, 18 AWG wire, heat shrink, terminal blocks

### Software
- [ ] VS Code
- [ ] PlatformIO
- [ ] Arduino framework for ESP32
- [ ] LVGL
- [ ] KiCad

---

## File status
This BOM was prepared for the Tac-Com project and saved as:

`/Users/titus/.openclaw/workspace/tac-com/BOM.md`
