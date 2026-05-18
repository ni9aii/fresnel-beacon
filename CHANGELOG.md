# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.4.0] — 2026-05-18

### Added

- **Animation modes** — 4 modes with state machine (BEACON, STROBE, AMBIENT, OFF), mode-specific render functions, HTTP API support.
- **Input validation** — Config parameters now validated and clamped (speed: [0.1, 60.0] rpm, brightness: [0.0, 1.0], color: 24-bit RGB, mode: [0, ANIM_MODE_COUNT)), warnings logged on clamping.
- **Error handling** — `led_driver_init()` returns `esp_err_t` with proper cleanup on RMT/encoder init failure, animation task checks error.
- **CI static analysis** — clang-tidy job with bugprone/cert/performance checks (non-blocking), Dependabot for weekly GitHub Actions updates.

### Changed

- **Renderer abstraction** — already implemented (mock_renderer + led_renderer with function pointers).

### Fixed

- **[MINOR]** Invalid mode values — config_manager_set_mode() now clamps invalid modes to BEACON (0) with warning.

### Security

- All config inputs validated and clamped to safe ranges.
- Invalid modes rejected and logged.

### Performance

- No performance changes.

### Architecture

- Animation modes cleanly separated into render functions.
- Better error propagation in LED driver init.

## [0.3.0] — 2026-05-18

### Added

- **Async NVS persistence** — Background task (`nvs_save_task`) with queue-based async save, HTTP handlers now non-blocking.
- **WiFi credential isolation** — Separate `wifi_credentials_t` struct with dedicated API (`config_manager_get/set_wifi_credentials()`).
- **Production configuration** — `sdkconfig.defaults.production` with WDT panic enabled and NVS encryption ready.
- **Renderer error logging** — `led_renderer_set_pixel()` now logs `ESP_LOGW()` on LED driver failures.
- **DevOps hardening** — Semgrep hard failure, cache invalidation, release tag guards.

### Fixed

- **[CRITICAL]** Global state race (`g_beacon_speed`) — Removed global, animation task now uses `config_manager_get_speed_sec()`.
- **[CRITICAL]** LED driver mutex hold during DMA — Triple-buffering (pending/ready/active), mutex hold reduced from ~100ms to ~0.1ms.
- **[CRITICAL]** Stack overflow in animation task — Stack size increased from 4096 to 8192 bytes.
- **[CRITICAL]** WiFi event handler ISR safety — AP fallback moved to EventGroup + worker task (no blocking in ISR).
- **[IMPORTANT]** IPC semaphore type — Changed from binary to counting semaphore (depth 32).
- **[IMPORTANT]** HTTP server rate limiter race — Atomic operations (`__atomic_fetch_add()`) for thread-safe counting.
- **[IMPORTANT]** HTTP JSON parser — Replaced hand-rolled `sscanf`/`strstr` with cJSON (ESP-IDF built-in).
- **[IMPORTANT]** Stack buffer overflow risk — Dynamic JSON allocation via `cJSON_PrintUnformatted()` instead of fixed stack buffer.
- **[IMPORTANT]** Encapsulation violations — `led_mutex` made static, `wifi_manager_get_ip()` now uses caller-provided buffer.
- **[IMPORTANT]** NVS write blocks HTTP handler — Async save via `config_manager_save_to_nvs_async()`.
- **[IMPORTANT]** WiFi credentials in plaintext RAM — Separate `wifi_credentials_t` struct with protected API.
- **[MINOR]** Missing watchdog reset — Added `esp_task_wdt_reset()` in animation loop.
- **[MINOR]** CI semgrep non-blocking — Removed `|| true`, now fails on ERROR severity.
- **[MINOR]** CI cache stale — Added `**/CMakeLists.txt` to hashFiles key.
- **[MINOR]** CI release misuse — Guard added: `if: github.ref_type == 'tag' && startsWith(github.ref_name, 'v')`.
- **[MINOR]** WDT panic disabled — Enabled in `sdkconfig.defaults.production`.

### Security

- WiFi credentials isolated from animation config in separate struct with mutex-protected API.
- Rate limiter now uses atomic operations for thread-safe counting across HTTPD worker tasks.
- cJSON parser replaces fragile hand-rolled JSON parsing with proper error handling.
- Dynamic JSON allocation prevents stack buffer overflows.
- Production config enables watchdog panic and NVS encryption.

### Performance

- LED driver mutex hold time reduced by 99.9% (100ms → 0.1ms) via triple-buffering.
- HTTP handlers now non-blocking (async NVS save).

### Architecture

- Improved component encapsulation (led_mutex static).
- Cleaner API surface (WiFi credentials separate from runtime config).
- Better ISR safety (EventGroup + worker task pattern).

## [0.2.0] — 2026-05-10

### Added

- Configurable rotation speed (0.5-20 seconds per rotation) — HTTP API endpoint `/api/config` now accepts `speed_sec` parameter, Web UI updated with range slider.

### Fixed

- [CRITICAL] Use-after-free in `led_driver_deinit()` — mutex deletion now occurs BEFORE `led_driver_clear()` call.
- [IMPORTANT] HTTP JSON parser — added width limits (`%31f`, `%31ld`) to `sscanf` calls for security.
- [IMPORTANT] HTTP mode validation — added sentinel check to reject `-1` parse failure value.
- [IMPORTANT] HTTP color overflow — reordered validation checks (parse errors before range check).
- [IMPORTANT] AP password entropy — now uses full MAC address (6 bytes) instead of last 2 bytes.
- [IMPORTANT] HTTP server SSN protection — added `httpd_sess_set_ctx()` context binding.
- [MINOR] CI format check — changed from `|| true` to `--Werror` for stricter enforcement.
- [MINOR] CI caching — added `sdkconfig.defaults` and `CMakeLists.txt` hash for cache invalidation.
- [MINOR] CI clang-format — enabled `--Werror` flag for fail-fast formatting violations.

### Security

- Added `CONFIG_MANAGER_DEFAULTS()` compile-time validation with `_Static_assert` for AP password buffer size.
- Temporarily disabled NVS encryption in `sdkconfig.defaults` for CI builds (no eFuse keys in simulation).

## [0.1.0] — 2026-05-09

### Added

- Core beacon animation — rotating beam sweep with quadratic falloff on 8×8 WS2812B matrix.
- RMT-based LED driver (`components/led_driver/`).
- Inline beacon math (`components/beacon_animation/include/beacon_math.h`).
- Host unit tests (`test_beacon_math.c`, `test_led_driver.c`).
- CI pipeline — test, cppcheck analysis, ESP-IDF build, Wokwi simulation.
- Task watchdog (`esp_task_wdt`) and stack high-water mark logging.
- Wokwi simulation setup (`diagram.json`, `wokwi.toml`, `scenario.yaml`).

[Unreleased]: https://github.com/ni9aii/fresnel-beacon/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/ni9aii/fresnel-beacon/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/ni9aii/fresnel-beacon/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/ni9aii/fresnel-beacon/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/ni9aii/fresnel-beacon/releases/tag/v0.1.0