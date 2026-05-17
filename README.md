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
- [x] **Phase 2**: Runtime configuration via REST API + Web UI
  - IPC queue for thread-safe config updates
  - NVS persistent storage (speed, mode, brightness, color)
  - WiFi STA + AP fallback (AP password derived from MAC)
  - HTTP server with embedded Web UI
- [x] Configurable rotation speed (0.5-20 sec per rotation)
- [ ] Multiple light modes (strobe, ambient)
- [ ] Brightness control

## Firmware Architecture

```
app_main
├── config_manager_init()          — load config from NVS
├── wifi_manager_init()            — STA + AP fallback
├── http_server_start()            — REST API + Web UI
└── beacon_animation_task          — main animation loop
    ├── Per-frame: clear matrix, compute beam, set pixels, flush
    ├── process_ipc_commands()     — apply config updates from queue
    ├── esp_task_wdt: subscribe + periodic reset
    └── Stack high-water mark logging (every 30 s)
```

## Project Structure

```
fresnel-beacon/
├── main/
│   ├── main.c                    — app_main: init all subsystems
│   ├── credentials.h.example     — WiFi credentials template
│   └── CMakeLists.txt
├── components/
│   ├── led_driver/               — RMT-based WS2812B driver
│   │   ├── led_driver.c
│   │   └── include/led_driver.h
│   ├── beacon_animation/         — Beacon pattern generator
│   │   ├── beacon_animation.c
│   │   └── include/
│   │       ├── beacon_animation.h
│   │       └── beacon_math.h     — Inline math (pixel_index, angle_diff)
│   ├── config_manager/           — Runtime config + NVS persistence
│   │   ├── config_manager.c
│   │   └── include/config_manager.h
│   ├── ipc/                      — Inter-process communication queue
│   │   ├── ipc.c
│   │   └── include/ipc.h
│   ├── http_server/              — REST API + embedded Web UI
│   │   ├── http_server.c
│   │   └── include/http_server.h
│   └── wifi_manager/             — WiFi STA/AP + event handling
│       ├── wifi_manager.c
│       └── include/wifi_manager.h
├── test/
│   ├── test_beacon_math.c        — Host unit tests (angle math, pixel mapping)
│   ├── test_led_driver.c         — Host unit tests (GRB order, bounds)
│   ├── test_config_manager.c     — ESP-IDF Unity tests (NVS roundtrip)
│   ├── test_ipc.c                — ESP-IDF Unity tests (queue ops)
│   ├── test_esp_http_server.c    — ESP-IDF compile-check
│   ├── test_esp_led_driver.c     — ESP-IDF compile-check
│   └── test_esp_wifi_manager.c   — ESP-IDF compile-check
├── CMakeLists.txt
├── sdkconfig.defaults
├── diagram.json                  — Wokwi simulation layout
├── wokwi.toml                    — Wokwi config
├── scenario.yaml                 — Wokwi test scenario
├── .clang-format                 — Code style (LLVM, 4-space, 100 col)
└── .github/workflows/
    ├── ci.yml                    — Test, analyze, build + simulate
    └── release.yml               — Build + publish on v* tags
```

## REST API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | Current config + WiFi status (JSON) |
| POST | `/api/config` | Update config (JSON body: `speed`, `mode`, `brightness`, `color`) |

**POST example:**
```bash
curl -X POST http://fresnel-beacon.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"speed":12.5,"mode":0,"brightness":0.8,"color":"0xFFA028"}'
```

**AP mode:** When WiFi connection fails, device starts AP with SSID `Fresnel-Beacon-XXYY` and password derived from MAC address (printed in serial log on boot).

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
gcc -Icomponents/led_driver/include -Icomponents/beacon_animation/include -I. test/test_beacon_math.c -lm -o test_beacon_math && ./test_beacon_math
gcc -Icomponents/led_driver/include -I. test/test_led_driver.c -lm -o test_led_driver && ./test_led_driver
```

## Code Style

Format all C/H files before committing:

```bash
find components main test -name "*.c" -o -name "*.h" | xargs clang-format -i
```

Style config: `.clang-format` (LLVM-based, 4-space indent, 100 column limit).

## CI

Three jobs on every push to `main`:

| Job | Description |
|-----|-------------|
| **test** | Compile and run host unit tests + formatting check |
| **analyze** | cppcheck static analysis |
| **build-and-simulate** | ESP-IDF build, size report, Wokwi simulation |

Releases are published automatically on `v*` tags.

## License

MIT — see [LICENSE](LICENSE).
