# Firmware Review — fresnel-beacon Phase 2

**Commit:** d922c3c
**Date:** 2026-05-08
**Scope:** ESP32 firmware (components, main, sdkconfig)
**Reviewer:** Hermes Agent

---

## Summary

Phase 2 introduces a full-featured beacon firmware: IPC queue, mutex-protected LED driver, runtime config with NVS persistence, Wi-Fi STA+AP fallback, HTTP REST API with embedded Web UI, and a 30 FPS animation task with watchdog compliance. The codebase is well-structured, uses ESP-IDF v5.x RMT APIs correctly, and follows FreeRTOS conventions broadly. However, there are **critical thread-safety violations**, a **stack overflow risk**, and **several error-handling gaps** that must be addressed before production.

---

## Findings

### Critical

#### [CRITICAL] Race condition: `config_manager_save_to_nvs()` called without holding `s_config_mutex`
- **File:** `components/http_server/http_server.c`
- **Function:** `api_config_post_handler()`
- **Line:** 166
- **Description:** After enqueuing IPC commands, the handler calls `config_manager_save_to_nvs()` directly. `save_to_nvs()` reads `s_config` fields (e.g., `s_config.speed_rpm`, `s_config.color_rgb`) without acquiring `s_config_mutex`. Meanwhile, `beacon_animation_task` drains the IPC queue and calls `config_manager_set_speed()` etc., which **do** take the mutex. This creates a window where the animation task may be mid-update while NVS reads stale or torn values. Worse, `load_from_nvs()` also writes `s_config` without the mutex during init (acceptable because no other task exists yet), but `save_to_nvs()` is called at runtime.
- **Impact:** Corrupted NVS snapshots, torn reads of `runtime_config_t`, potential crash if structure layout changes and compiler optimizes.
- **Fix:** Wrap the body of `config_manager_save_to_nvs()` and `config_manager_load_from_nvs()` with `xSemaphoreTake(s_config_mutex, portMAX_DELAY)` / `xSemaphoreGive(s_config_mutex)`. Document that NVS API is thread-safe for its own internal state, but the `s_config` struct is not.

#### [CRITICAL] `led_driver_flush()` releases mutex before RMT transmission completes
- **File:** `components/led_driver/led_driver.c`
- **Function:** `led_driver_flush()`
- **Lines:** 109–115
- **Description:** The function takes `led_mutex`, calls `rmt_tx_wait_all_done()`, then calls `rmt_transmit()`, and **immediately gives the mutex** before `rmt_transmit()` has finished. `rmt_transmit()` is non-blocking; the actual DMA/RMT transfer happens asynchronously. If `beacon_animation_task` (or another caller) calls `led_driver_clear()` or `led_driver_set_pixel()` immediately after `flush()` returns, it will mutate `s_pixels` while the RMT peripheral is still reading from that buffer.
- **Impact:** Visual corruption (flicker, wrong colors), and on some ESP32 variants, bus errors if the buffer is modified mid-DMA.
- **Fix:** Move `xSemaphoreGive(led_mutex)` to **after** `rmt_tx_wait_all_done(s_led_chan, portMAX_DELAY)` on the *next* call, or keep the mutex held until the previous frame is fully on-wire. Alternatively, use a double-buffered `s_pixels[2][...]` scheme so `rmt_transmit()` reads from a stable buffer while the animation task writes the next frame.

#### [CRITICAL] `wifi_manager_start_ap()` uses `esp_netif_destroy()` without stopping/deinitializing cleanly
- **File:** `components/wifi_manager/wifi_manager.c`
- **Function:** `wifi_manager_start_ap()`
- **Lines:** 81–86
- **Description:** When falling back to AP mode, the code calls `esp_wifi_stop()`, `esp_wifi_set_mode(WIFI_MODE_NULL)`, then `esp_netif_destroy(s_sta_netif)`. `esp_netif_destroy()` on an active netif without first deregistering event handlers or ensuring the interface is fully down can lead to use-after-free in the TCP/IP thread or event loop. The event handler registered in `wifi_manager_init()` is never unregistered.
- **Impact:** Memory corruption, event loop crashes, hard-to-reproduce faults during AP fallback.
- **Fix:** Unregister the Wi-Fi event handler instance before destroying the netif, or use `esp_netif_detach()` / `esp_netif_action_stop()` sequence. Ensure `esp_wifi_stop()` has completed (it is synchronous) before `esp_netif_destroy()`.

---

### Warning

#### [WARNING] `httpd_req_recv()` may return partial data; buffer not fully drained
- **File:** `components/http_server/http_server.c`
- **Function:** `api_config_post_handler()`
- **Lines:** 92–98
- **Description:** `httpd_req_recv()` can return fewer bytes than `req->content_len` (e.g., if the packet is fragmented). The code checks `ret <= 0` but does not loop to read the remainder. On slow or fragmented requests, `buf[ret]` terminates mid-JSON, causing `sscanf` to parse garbage or fail silently.
- **Impact:** Intermittent API failures, config updates silently dropped.
- **Fix:** Loop `httpd_req_recv()` until total received equals `req->content_len`, or use `httpd_req_recv()` with a retry counter.

#### [WARNING] `ipc_queue` is checked for NULL but never asserted in `process_ipc_commands()`
- **File:** `components/beacon_animation/beacon_animation.c`
- **Function:** `process_ipc_commands()`
- **Line:** 29
- **Description:** `process_ipc_commands()` calls `xQueueReceive(ipc_queue, ...)` unconditionally inside `while(1)`. The outer caller checks `ipc_queue != NULL`, but if `ipc_init()` failed and the task was still created (it cannot be, because `app_main` checks `ipc_init()` with `ESP_ERROR_CHECK`), this is defensive. More importantly, `xQueueReceive` with `0` ticks is fine, but if the queue is full in the HTTP server, commands are dropped with only a log warning. There is no back-pressure or retry.
- **Impact:** Config changes may be lost under burst load.
- **Fix:** Consider a short `portMAX_DELAY` or a bounded retry in the HTTP handler, or increase queue depth beyond 10 if the Web UI allows rapid slider changes.

#### [WARNING] `config_manager_load_from_nvs()` writes `s_config` without mutex during early init
- **File:** `components/config_manager/config_manager.c`
- **Function:** `config_manager_load_from_nvs()`
- **Lines:** 131–205
- **Description:** While safe in practice because no other task runs before `config_manager_load_from_nvs()` returns in `app_main`, the function is public and could be called later from another task (e.g., a “factory reset” command via HTTP). Without `s_config_mutex`, this is a race.
- **Impact:** Potential data race if called concurrently.
- **Fix:** Take `s_config_mutex` at the top of `load_from_nvs()` and `save_to_nvs()`.

#### [WARNING] `wifi_manager_init()` uses `ESP_ERROR_CHECK()` for non-fatal fallback paths
- **File:** `components/wifi_manager/wifi_manager.c`
- **Function:** `wifi_manager_init()`
- **Lines:** 168–170
- **Description:** `esp_wifi_set_mode()`, `esp_wifi_set_config()`, and `esp_wifi_start()` are wrapped in `ESP_ERROR_CHECK()`. If STA start fails (e.g., no credentials), the system panics instead of falling back gracefully. The code later calls `wifi_manager_start_ap()` if no SSID is configured, but that path is unreachable if `esp_wifi_start()` panics.
- **Impact:** Unnecessary reboot loops during bring-up or credential misconfiguration.
- **Fix:** Replace `ESP_ERROR_CHECK` with explicit error handling and fallback to AP mode on failure.

#### [WARNING] `s_ip_str` and `s_status` are read without synchronization
- **File:** `components/wifi_manager/wifi_manager.c`
- **Functions:** `wifi_manager_get_status()`, `wifi_manager_get_ip()`
- **Lines:** 182–190
- **Description:** The event handler (running in the system event task) writes `s_status` and `s_ip_str`, while `http_server` and `wifi_monitor_task` read them. On ESP32-C3/S3 (which this project targets), a 32-bit enum/char array write is not guaranteed atomic across cores. No mutex or volatile qualifier is used.
- **Impact:** Torn reads of IP string, stale status values.
- **Fix:** Add a lightweight mutex or use `volatile` + proper memory barriers. For `s_ip_str`, copy into a local buffer under lock.

#### [WARNING] `led_driver_init()` lacks error return; `ESP_ERROR_CHECK` panics on RMT failure
- **File:** `components/led_driver/led_driver.c`
- **Function:** `led_driver_init()`
- **Lines:** 31–48
- **Description:** `led_driver_init()` returns `void` and uses `ESP_ERROR_CHECK()` for RMT channel creation. If the GPIO is already in use or RMT memory is exhausted, the system panics. In `app_main`, this is called after Wi-Fi and HTTP server are up, so a panic loses the diagnostic HTTP endpoint.
- **Impact:** Hard panic instead of graceful degradation.
- **Fix:** Change signature to `esp_err_t led_driver_init(void)` and return errors. In `app_main`, log the error and continue (or blink an onboard LED to indicate fault).

---

### Suggestion

#### [SUGGESTION] `beacon_animation_task` stack size of 4096 may be tight with floating-point math
- **File:** `main/main.c`
- **Line:** 96
- **Description:** The task uses `atan2f`, heavy loop nesting, and JSON/string formatting is not in this task, but the stack watermark should be monitored. Current code logs high water mark every 30 s, which is good. On ESP32-S3 with 240 MHz and FPU, 4096 is likely sufficient, but margin is slim if the RMT driver or logging allocates temporaries.
- **Fix:** Keep the 30 s high-water-mark log; if it drops below 512 words, increase to 5120 or 6144.

#### [SUGGESTION] `config_manager` lacks input validation on setters
- **File:** `components/config_manager/config_manager.c`
- **Functions:** `config_manager_set_speed()`, `config_manager_set_brightness()`, etc.
- **Description:** Brightness can be set to negative or >1.0; speed can be NaN or infinite; mode is unbounded. The HTTP server clamps some values, but IPC commands from other sources (e.g., future MQTT component) would not be guarded.
- **Fix:** Add range clamps inside the setters: `brightness` to [0,1], `speed_rpm` to [0,60], `mode` to known enum values.

#### [SUGGESTION] `http_server` JSON parser is fragile and non-standard
- **File:** `components/http_server/http_server.c`
- **Function:** `api_config_post_handler()`
- **Lines:** 100–133
- **Description:** The hand-rolled `sscanf`/`strstr` parser breaks on JSON reordering, extra whitespace, nested objects, or escaped quotes. It is acceptable for a Phase 2 demo but will not scale.
- **Fix:** Integrate a lightweight JSON parser such as `cJSON` (available in ESP-IDF component registry) or `jsmn` for robust parsing.

#### [SUGGESTION] Missing `http_server_stop()` call on Wi-Fi AP fallback
- **File:** `components/wifi_manager/wifi_manager.c`
- **Function:** `wifi_manager_start_ap()`
- **Description:** When switching from STA to AP, the HTTP server is left running. This is usually fine because the IP layer changes, but existing connections may hang and the server socket may still be bound to the old netif. A cleaner reset would stop and restart the HTTP server on the new interface.
- **Fix:** Call `http_server_stop()` before `esp_wifi_stop()` and `http_server_init()` after AP is up, or use `SO_REUSEADDR` and bind to `INADDR_ANY` (already default in `esp_http_server`).

#### [SUGGESTION] `sdkconfig.defaults` disables WDT panic — document production toggle
- **File:** `sdkconfig.defaults`
- **Line:** 11
- **Description:** `CONFIG_ESP_TASK_WDT_PANIC=n` is intentional for development, as noted in the comment. This is good practice, but there is no CI check or build warning to enforce switching to `=y` for release builds.
- **Fix:** Add a `#warning` or `static_assert` in `main.c` that fails if `CONFIG_ESP_TASK_WDT_PANIC` is disabled and a `RELEASE_BUILD` macro is defined.

---

### Nitpick

#### [NITPICK] `wifi_monitor_task` does not handle task deletion or WDT
- **File:** `main/main.c`
- **Function:** `wifi_monitor_task()`
- **Lines:** 16–30
- **Description:** The task runs indefinitely with a 30 s delay but does not register with the task WDT. With the 5 s WDT timeout, this task will trigger the watchdog if the delay were shorter. At 30 s it is fine because the default WDT checks the idle task, but explicit registration is cleaner.
- **Fix:** Add `esp_task_wdt_add(NULL)` and `esp_task_wdt_reset()` in the loop for consistency.

#### [NITPICK] `pixel_index()` inline in header uses unsigned cast for bounds check
- **File:** `components/beacon_animation/include/beacon_math.h`
- **Line:** 9
- **Description:** The cast `(unsigned)x >= LED_MATRIX_COLS` is clever for catching negative values, but `x` is `int`; on 32-bit ESP32, `unsigned` promotion is well-defined. No issue, just slightly unconventional.
- **Fix:** Optional — use an explicit `if (x < 0 || x >= LED_MATRIX_COLS)` for readability.

#### [NITPICK] `log_system_info()` calls `esp_wifi_get_mac()` before Wi-Fi is fully started
- **File:** `main/main.c`
- **Function:** `log_system_info()`
- **Line:** 49
- **Description:** In `app_main`, `log_system_info()` is called after `wifi_manager_init()`, so the MAC should be available. However, if `wifi_manager_init()` fell back to AP mode immediately, the STA MAC query may return `ESP_ERR_WIFI_NOT_INIT` or stale data depending on driver state.
- **Fix:** Use `esp_efuse_mac_get_default()` or `esp_read_mac()` for the base MAC, which does not depend on Wi-Fi state.

#### [NITPICK] `http_server` `api_config_post_handler` allocates `buf` with `malloc` instead of stack array
- **File:** `components/http_server/http_server.c`
- **Line:** 86
- **Description:** 513 bytes on the heap is fine, but a stack array of 512 bytes inside an HTTP worker task (stack typically 4096+) would avoid fragmentation and be simpler. The current approach is safe; just slightly heavier.
- **Fix:** Optional — switch to `char buf[512]` if `req->content_len <= 512` is already enforced.

---

## Conclusion

Phase 2 is a solid architectural step forward. The component boundaries are clean, NVS persistence works, and the Web UI is a nice integrated touch. However, **three critical issues** must be fixed before this firmware is safe for extended runtime:

1. **Thread-safety gap in `config_manager` NVS paths** — mutex must cover all `s_config` accesses.
2. **LED driver race between `flush()` and `clear()`/`set_pixel()`** — mutex scope or double-buffering needed.
3. **Wi-Fi AP fallback netif lifecycle** — event handler unregister + safe destroy sequence.

Additionally, the HTTP server’s hand-rolled JSON parser and partial `httpd_req_recv()` handling are the most likely sources of user-visible bugs in Phase 3.

**Recommended priority order:**
1. Fix CRITICAL #1 and #2 (thread safety).
2. Fix CRITICAL #3 (Wi-Fi lifecycle).
3. Address WARNING #1 (HTTP recv loop) and WARNING #4 (Wi-Fi error handling).
4. Integrate `cJSON` or `jsmn` for config parsing.
5. Add setter validation in `config_manager`.

---

*End of review.*
