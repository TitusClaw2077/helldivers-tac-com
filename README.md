# Helldivers Tac-Com Bracelet 🦂

> A wearable ESP32 stratagem pad that calls in a real rocket airstrike.

Enter a Helldivers 2 stratagem sequence on the wrist unit touchscreen. The correct code wirelessly arms and triggers a rocket igniter on the launcher unit via ESP-NOW — turning a model rocket launch into a real airstrike call.

---

## Hardware

### Wrist Unit
- **Elecrow CrowPanel ESP32 3.5"** — ESP32-WROVER-B, 480×320 resistive touchscreen, LVGL-compatible
- 1S 1000mAh LiPo
- 3D printed wrist enclosure

### Launcher Unit
- **ESP32-WROOM-32** bare module
- IRLZ44N N-channel MOSFET switching standard hobby igniters
- 2S LiPo (7.4V) dedicated igniter rail
- DaierTek missile-cover toggle switch as ARM/SAFE interlock
- Banana clip output leads for Estes-style igniters

## Comms
**ESP-NOW** — peer-to-peer, no router required, ~1ms latency, ~200m range

## Development

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension)
- ESP32 Arduino framework (installed automatically via PlatformIO)

### Build targets

```bash
# Build wrist firmware
pio run -e wrist

# Build launcher firmware
pio run -e launcher

# Upload to connected device
pio run -e launcher --target upload
```

### Repo structure
```
include/common/     # Shared headers: protocol, stratagems, config
lib/common/         # Shared implementation: CRC, stratagem DB
lib/wrist_core/     # Wrist logic: stratagem engine, comms, power
lib/launcher_core/  # Launcher logic: state machine, igniter, continuity
lib/wrist_ui/       # LVGL screen and widget code
src/wrist/          # Wrist main.cpp
src/launcher/       # Launcher main.cpp
docs/               # Wiring diagrams, pinouts, project spec
```

## Safety
This project controls a wireless rocket ignition system. See `docs/` for the full safety system design. Key rules:
- Launcher boots **DISARMED** by default
- Physical ARM interlock required before any remote arm command is accepted
- 3-step fire sequence: stratagem match → 2-second lockout → explicit confirm
- Comms loss never equals fire

## Docs
- [Project Spec](docs/PROJECT_SPEC.md)
- [BOM](docs/BOM.md)
- [Launcher Wiring](docs/LAUNCHER_WIRING.md)
- [Firmware Architecture](docs/FIRMWARE_ARCHITECTURE.md)
- [Bench Bring-Up Guide](docs/BENCH_BRINGUP.md)

## Contributors
- **Defcon88** — project lead, hardware, rocketry
- **TitusClaw2077** — firmware architecture, code
