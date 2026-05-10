# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Phase 2**: Runtime configuration system
  - `config_manager` component — NVS-backed persistent config (speed, mode, brightness, color)
  - `ipc` component — thread-safe command queue between HTTP server and animation task
  - `http_server` component — REST API (`GET /api/status`, `POST /api/config`) + embedded Web UI
  - `wifi_manager` component — WiFi STA with AP fallback, WPA2_PSK password derived from MAC
- `.clang-format` — LLVM style, 4-space indent, 100 column limit
- `sdkconfig.defaults` expanded with log level, stack size, RMT, and NVS encryption settings
- Content-Type validation on POST `/api/config`
- `strtoul` + `errno` + range check for color parser in HTTP handler
- `CONFIG_MANAGER_DEFAULTS()` macro to centralize default values
- `extern "C"` guards in all 8 public headers
- `ipc_wait_commit()` documentation for timeout semantics

### Changed
- Wokwi simulation timeout increased from 30s to 120s in CI
- `led_driver_deinit()` now calls `led_driver_clear()` **before** deleting mutex (use-after-free fix)
- HTTP error message changed from `"nvs save failed"` to `"internal error"` (information leak fix)
- `config_manager.c` spelling unified to American English ("initialized")
- `wifi_manager.c` indentation fixed in retry loop

### Fixed
- **CRITICAL**: WiFi event handler legacy API compatibility (`esp_event_handler_register` / `unregister`)
- **CRITICAL**: LED DMA race condition — mutex now held through `rmt_transmit`
- **CRITICAL**: HTTP JSON parser buffer overflow — capped content length + width-limited `sscanf`
- **CRITICAL**: `release.yml` missing top-level permissions (`contents: read`)
- **IMPORTANT**: `config_manager_save_to_nvs()` now propagates NVS commit errors
- **IMPORTANT**: `led_driver_deinit()` now deletes mutex (resource leak fix)
- **IMPORTANT**: CI action pins (`actions/cache`, `esp-idf-ci-action`) pinned to SHA instead of floating tags
- **IMPORTANT**: AP mode now uses `WIFI_AUTH_WPA2_PSK` with MAC-derived password instead of open network
- **IMPORTANT**: `http_server.c` returns HTTP 500 on NVS save failure instead of silent success
- **LOW**: `.gitignore` expanded with editor/OS artifacts (`*.swp`, `.DS_Store`, `__pycache__`)
- **LOW**: `release.yml` test job now includes `-lm` for `test_led_driver.c`

### Security
- AP mode password derived from device MAC address (deterministic, readable from SSID)
- WiFi credentials stored in NVS but **not** exposed via REST API (by design)
- HTTP API intentionally unauthenticated for trusted LAN use (documented known risk)

## [0.1.0] — 2026-04-20

### Added
- Initial firmware: rotating lighthouse beam animation on 8×8 WS2812B LED matrix
- `led_driver` — RMT-based WS2812B driver with GRB order, brightness scaling
- `beacon_animation` — beam sweep with quadratic falloff, `angle_diff` wrap-around
- `beacon_math` — inline pixel index mapping (serpentine layout), angle difference
- Host unit tests: `test_beacon_math.c`, `test_led_driver.c`
- CI pipeline: test, cppcheck static analysis, ESP-IDF build, Wokwi simulation
- GitHub Release workflow on `v*` tags
- Task watchdog (5s) + stack high-water mark logging

[Unreleased]: https://github.com/ni9aii/fresnel-beacon/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/ni9aii/fresnel-beacon/releases/tag/v0.1.0
