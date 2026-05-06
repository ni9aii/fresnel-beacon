# Fresnel Beacon

A desktop lighthouse lamp: ESP32-S3 drives an 8×8 WS2812B LED matrix mounted inside a Fresnel lens to simulate a rotating lighthouse beam — no moving parts.

## Concept

A real lighthouse rotates its optics around a fixed light source. This project inverts that: the lens is static, and LEDs fire in sequence to synthesize the sweeping beam effect electronically.

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| MCU + LED panel | Waveshare ESP32-S3-Matrix | ESP32-S3 + 8×8 WS2812B, USB-C |
| Optics | Fresnel lens | Mounted over the LED matrix |
| Power | USB-C 5V | Via onboard connector |
| Enclosure | TBD | Lighthouse-shaped, 3D printed |

## Firmware Stack

- **Framework**: ESP-IDF v5.3 (FreeRTOS)
- **LED driver**: RMT peripheral → WS2812B
- **Watchdog**: esp_task_wdt (5 s timeout)
- **Language**: C

## Features

- [x] Core beacon animation (rotating beam sweep with quadratic falloff)
- [x] Unit tests (beacon math, LED driver logic)
- [x] CI pipeline (build, static analysis, Wokwi simulation)
- [x] Task watchdog and stack monitoring
- [ ] Configurable rotation speed
- [ ] Multiple light modes (strobe, ambient)
- [ ] Brightness control
- [ ] Web UI over WiFi for runtime configuration

## Firmware Architecture

```
app_main
└── beacon_animation_task
    ├── Per-frame: clear matrix, compute beam, set pixels, flush
    ├── esp_task_wdt: subscribe + periodic reset
    └── Stack high-water mark logging (every 30 s)

wifi_task (Phase 2)
    └── HTTP server for web UI
```

## Project Structure

```
fresnel-beacon/
├── main/
│   ├── main.c                    — app_main: init LED driver, spawn task
│   ├── credentials.h.example     — WiFi credentials template
│   └── CMakeLists.txt
├── components/
│   ├── led_driver/               — RMT-based WS2812B driver
│   │   ├── led_driver.c
│   │   └── include/led_driver.h
│   └── beacon_animation/         — Beacon pattern generator
│       ├── beacon_animation.c
│       └── include/
│           ├── beacon_animation.h
│           └── beacon_math.h     — Inline math (pixel_index, angle_diff)
├── test/
│   ├── test_beacon_math.c        — Host unit tests (angle math, pixel mapping)
│   └── test_led_driver.c         — Host unit tests (GRB order, bounds)
├── CMakeLists.txt
├── sdkconfig.defaults
├── diagram.json                  — Wokwi simulation layout
├── wokwi.toml                    — Wokwi config
├── scenario.yaml                 — Wokwi test scenario
└── .github/workflows/
    ├── ci.yml                    — Test, analyze, build + simulate
    └── release.yml               — Build + publish on v* tags
```

## Development Environment

- ESP-IDF v5.3.x
- Target: `esp32s3`
- Flash via USB-C (native USB, no UART adapter needed)

## Building

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash
```

## Testing

Host unit tests run without ESP-IDF or hardware:

```bash
gcc -I. test/test_beacon_math.c -lm -o test_beacon_math && ./test_beacon_math
gcc -I. test/test_led_driver.c -o test_led_driver && ./test_led_driver
```

## CI

Three parallel jobs on every push to `main`:

| Job | Description |
|-----|-------------|
| **test** | Compile and run host unit tests |
| **analyze** | cppcheck static analysis |
| **build-and-simulate** | ESP-IDF build, size report, Wokwi simulation |

Releases are published automatically on `v*` tags.

## License

MIT — see [LICENSE](LICENSE).
