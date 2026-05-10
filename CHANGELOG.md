# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] — 2026-05-10

### Added

- **IPC queue** (`components/ipc/`) — thread-safe inter-process communication for config updates between HTTP server and animation task.
- **Config manager** (`components/config_manager/`) — runtime configuration with NVS persistence. Supports speed, mode, brightness, color.
- **WiFi manager** (`components/wifi_manager/`) — STA mode with AP fallback. AP SSID includes MAC suffix; password derived from MAC address (printed on boot).
- **HTTP server + Web UI** (`components/http_server/`) — REST API (`GET /api/status`, `POST /api/config`) with embedded HTML/JS control panel.
- **Host unit tests** for `test_led_driver.c` (requires `-lm` for `roundf`).
- **ESP-IDF Unity tests** — `test_config_manager.c`, `test_ipc.c`, `test_esp_http_server.c`, `test_esp_led_driver.c`, `test_esp_wifi_manager.c`.
- **`.clang-format`** — LLVM-based style config (4-space indent, 100 column limit).
- **Formatting check** in CI (non-blocking, `|| true`).

### Changed

- `app_main` architecture — now initializes config_manager, wifi_manager, http_server before spawning animation task.
- `beacon_animation_task` — per-frame `process_ipc_commands()` call to apply queued config updates.
- CI `build-and-simulate` timeout increased from 30s to 120s for Wokwi simulation.

### Fixed

- CI action pinning reverted from SHA hashes to tag-based references (`actions/cache@v4.2.0`, `espressif/esp-idf-ci-action@v1.1.0`) to resolve "Set up job" failures.

## [0.1.0] — 2026-05-09

### Added

- Core beacon animation — rotating beam sweep with quadratic falloff on 8×8 WS2812B matrix.
- RMT-based LED driver (`components/led_driver/`).
- Inline beacon math (`components/beacon_animation/include/beacon_math.h`).
- Host unit tests (`test_beacon_math.c`, `test_led_driver.c`).
- CI pipeline — test, cppcheck analysis, ESP-IDF build, Wokwi simulation.
- Task watchdog (`esp_task_wdt`) and stack high-water mark logging.
- Wokwi simulation setup (`diagram.json`, `wokwi.toml`, `scenario.yaml`).

[Unreleased]: https://github.com/ni9aii/fresnel-beacon/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/ni9aii/fresnel-beacon/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/ni9aii/fresnel-beacon/releases/tag/v0.1.0
