Wrist UI module for the CrowPanel build.

Current reality:
- this is no longer just the old temporary diagnostics bring-up
- the module now contains the LVGL wrist shell used for the launcher home / stratagem entry / confirm / fire flow
- `diag_ui.*` remains the older diagnostics-oriented path
- `lvgl_ui.*` is the current main wrist UI path under active testing

Important dependency notes:
- use the real LVGL package header form: `#include <lvgl/lvgl.h>`
- keep the wrist build pinned to the known-good combo documented in `platformio.ini`
- `LovyanGFX@1.2.20` was observed failing against `lvgl/lvgl@8.4.0`; `LovyanGFX@1.2.19` is the verified working baseline

Immediate testing goal:
- re-run CrowPanel hardware validation on the current compile-fixed branch
- confirm the home / activate / entry / confirm / fire / return flow still behaves correctly on hardware
