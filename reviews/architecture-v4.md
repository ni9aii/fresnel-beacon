# Architecture Review — fresnel-beacon Phase 2

**Commit:** d922c3c  
**Date:** 2026-05-08  
**Scope:** Component boundaries, dependency graph, init order, data flow, IPC design, task priorities, memory layout, extensibility.

---

## Summary

Phase 2 introduces six loosely-coupled ESP-IDF components with a queue-based IPC layer, a mutex-protected LED driver, and a REST/Web UI control surface. The architecture is pragmatic for an MVP but carries several structural risks: the IPC component has leaked an LED-specific mutex into a generic namespace, the animation task holds the LED mutex across an RMT transmit + wait, the HTTP server persists config via NVS without waiting for IPC commands to be consumed, and there is no component-level event bus or publish/subscribe abstraction. These issues will amplify as new animation modes, sensors, or network protocols are added in Phase 3.

---

## Findings

### [CRITICAL] IPC component violates single-responsibility: `led_mutex` lives in generic IPC namespace

**File:** `components/ipc/ipc.c` (lines 8, 21–28), `components/ipc/include/ipc.h` (lines 37–41)  
**Function:** `ipc_init()`

The IPC component is advertised as a generic FreeRTOS queue for inter-component commands, yet it also creates `led_mutex` — a semaphore whose sole consumer is `led_driver`. This conflates two unrelated concerns:

1. **Generic message passing** (`ipc_queue`, `ipc_cmd_t`).
2. **LED framebuffer mutual exclusion** (`led_mutex`).

Because `led_mutex` is declared `extern` in `ipc.h`, every component that includes `ipc.h` (currently `config_manager`, `beacon_animation`, `http_server`, `led_driver`, `main`) gains visibility to a semaphore it does not own. This is a latent coupling hazard: any future task could take `led_mutex` and deadlock the animation loop.

**Recommended fix:** Move `led_mutex` creation and the `extern` declaration into `led_driver.h` / `led_driver.c`. Remove the `REQUIRES ipc` from `led_driver` in `CMakeLists.txt` (or invert it so `ipc` depends on `led_driver` only if necessary). The IPC component should expose *only* the queue and command types.

---

### [CRITICAL] Animation task holds `led_mutex` across RMT transmit + blocking wait

**File:** `components/led_driver/led_driver.c` (lines 90–116)  
**Function:** `led_driver_flush()`

`led_driver_flush()` takes `led_mutex`, then calls `rmt_tx_wait_all_done(s_led_chan, portMAX_DELAY)` followed by `rmt_transmit()`. The animation task (`beacon_animation_task`) invokes `flush()` once per frame (every 33 ms). On an ESP32-S3 at 20 MHz RMT resolution, an 8×8 WS2812B frame (192 bytes ≈ 1.5 ms wire time plus DMA/RMT latency) can keep the mutex held for **2–5 ms**.

Because `led_mutex` is the *only* concurrency primitive protecting the framebuffer, any other task that attempts `set_pixel` or `clear` during this window will block. In Phase 3, if a second animation task or a sensor-driven overlay task is introduced, this coarse-grained lock will become a serialisation bottleneck. More importantly, if `rmt_tx_wait_all_done` ever stalls (e.g., RMT driver bug, power glitch), the watchdog in `beacon_animation_task` will reset, but the mutex remains held — a potential deadlock if the reset path does not release it.

**Recommended fix:** Decouple the framebuffer (protected by a short mutex) from the RMT transmission (which can run asynchronously). Adopt a double-buffering scheme:

- `s_pixels_front` — written by animation task under a brief mutex.
- `s_pixels_back` — handed to RMT without holding the mutex; use an RMT callback or `rmt_tx_wait_all_done` *after* release to signal completion.

---

### [WARNING] HTTP server calls `config_manager_save_to_nvs()` immediately after enqueuing IPC commands, creating a race with the animation task

**File:** `components/http_server/http_server.c` (lines 138–169)  
**Function:** `api_config_post_handler()`

The POST handler:

1. Parses JSON fields.
2. Sends up to four `ipc_cmd_t` messages to `ipc_queue` with `xQueueSend(..., 0)` (non-blocking, zero timeout).
3. Immediately calls `config_manager_save_to_nvs()`.

There is no guarantee that the animation task has drained the queue and applied the new values before NVS is written. If the queue is full (e.g., under heavy UI interaction or slow animation loop), commands are dropped with only a log warning, yet NVS still snapshots the *old* config. On the next boot, the user sees stale values despite having received a 200 OK from the API.

**Recommended fix:** Introduce a synchronous "commit" mechanism. Options:

- **Option A (preferred):** Add an IPC command `IPC_CMD_COMMIT` that the animation task acknowledges after applying pending commands. The HTTP handler blocks on a response semaphore or event group before calling `save_to_nvs()`.
- **Option B:** Have the HTTP handler write directly to `config_manager` (bypassing IPC) and let the animation task poll `config_manager_get()` every frame. This eliminates the queue race entirely but removes the decoupling benefit of IPC.

---

### [WARNING] `config_manager.h` includes `ipc.h` solely for `SemaphoreHandle_t` typedef, creating an artificial dependency

**File:** `components/config_manager/include/config_manager.h` (line 4)

`config_manager.h` includes `ipc.h` even though the config manager does not use the queue, the command types, or `led_mutex`. It only needs `SemaphoreHandle_t` from `freertos/semphr.h`. This forces every consumer of `config_manager` to transitively depend on `ipc`, bloating the include graph and build dependencies.

**Recommended fix:** Replace `#include "ipc.h"` with `#include "freertos/semphr.h"` in `config_manager.h`. Update `CMakeLists.txt` to remove `REQUIRES ipc` from `config_manager`.

---

### [WARNING] `wifi_manager` destroys and recreates `esp_netif` handles on AP fallback without global coordination

**File:** `components/wifi_manager/wifi_manager.c` (lines 77–128)  
**Function:** `wifi_manager_start_ap()`

When falling back to AP mode, `wifi_manager_start_ap()`:

- Calls `esp_wifi_stop()` and `esp_netif_destroy(s_sta_netif)`.
- Creates a new AP netif and restarts Wi-Fi.

There is no notification to `http_server` that the underlying network interface has changed. The HTTP server (bound to `0.0.0.0`) will momentarily lose all sockets; existing HTTP clients will see a TCP reset. In Phase 3, if an MQTT client or OTA task is added, it will not know to re-establish connections.

**Recommended fix:** Introduce a lightweight event bus or callback registry in `wifi_manager`. Emit events such as `WIFI_EVENT_STA_LOST_IP`, `WIFI_EVENT_AP_STARTED`, etc., and let `http_server` (and future network consumers) register handlers. This decouples Wi-Fi state transitions from downstream components.

---

### [WARNING] No abstraction layer between animation logic and LED hardware

**File:** `components/beacon_animation/beacon_animation.c` (lines 83–109)  
**Function:** `beacon_animation_task()`

The animation task directly calls `led_driver_clear()`, `led_driver_set_pixel()`, and `led_driver_flush()`. There is no intermediate "renderer" or "scene graph" abstraction. Adding a second animation mode (e.g., strobe, ambient) will require either:

- Branching inside the hot loop (hurting readability and cache locality), or
- Replacing the entire task (duplicating IPC command handling and watchdog logic).

**Recommended fix:** Define an `animation_mode_t` interface with a `render(float dt, const runtime_config_t *cfg, rgb_t *framebuffer)` contract. The `beacon_animation_task` becomes a generic scheduler: drain IPC, fetch config, call the active mode’s render function, then flush. New modes are registered at link time or runtime without touching the task loop.

---

### [SUGGESTION] `main.c` embeds a hard-coded 100 ms delay between Wi-Fi and HTTP server init

**File:** `main/main.c` (line 86)

```c
vTaskDelay(pdMS_TO_TICKS(100));
```

This delay is a heuristic to "let WiFi settle." It is fragile: on congested networks or after an NVS erase, 100 ms may be insufficient; on fast boots it wastes power. There is no explicit state machine or event-driven handshake.

**Recommended fix:** Replace the fixed delay with an event-group or semaphore. `wifi_manager` can signal `WIFI_READY_BIT` once the interface is up (STA started or AP active), and `http_server_init()` blocks until that bit is set. This makes init order deterministic and testable.

---

### [SUGGESTION] Stack size for `beacon_animation_task` is hard-coded and not validated against worst-case usage

**File:** `main/main.c` (lines 95–104)

```c
const size_t stack_size = 4096;
BaseType_t task_created = xTaskCreate(beacon_animation_task, "beacon", stack_size, NULL, 5, NULL);
```

The task uses `atan2f`, `snprintf` (indirectly via logging), and the RMT driver, all of which can consume significant stack. The high-water mark is logged every 30 s, but there is no runtime assertion that it stays above a safety margin. On ESP32-S3 with default config, 4096 bytes is usually adequate, but if future modes add recursion or large local arrays, this will silently overflow.

**Recommended fix:**

1. Define `BEACON_TASK_STACK_SIZE` in `Kconfig` or a component header.
2. After `configUSE_TRACE_FACILITY` is enabled, assert `uxTaskGetStackHighWaterMark(NULL) > 512` at the end of each frame in debug builds.

---

### [SUGGESTION] NVS load/save uses a flat key namespace with no versioning or schema migration

**File:** `components/config_manager/config_manager.c` (lines 131–269)

Keys such as `speed_rpm`, `mode`, `brightness`, `color_r`, `color_g`, `color_b`, `wifi_ssid`, `wifi_pass` are read and written individually. If the schema changes in Phase 3 (e.g., adding `trail_length`, `palette_idx`), there is no version key to detect old NVS blobs and migrate them. The current code silently falls back to defaults for missing keys, which is user-friendly but makes debugging configuration drift difficult.

**Recommended fix:** Add a `uint8_t config_version` key to NVS. On `load_from_nvs()`, read it first. If the version mismatches the firmware expectation, run a migration table (e.g., rename old keys, compute new defaults) before loading. This is a one-time ~20-line addition that prevents future support burden.

---

### [SUGGESTION] `http_server` embeds ~12 KB of minified HTML/JS/CSS as a C string literal

**File:** `components/http_server/http_server.c` (lines 178–328)

The single `s_index_html[]` array is convenient for a self-contained binary but has drawbacks:

- Any UI change requires a firmware rebuild.
- The string is stored in `.rodata`, consuming flash even if the user never opens the web page.
- There is no gzip compression; the ESP-IDF HTTP server will send the full ~12 KB on every `GET /`.

**Recommended fix:** For Phase 3, consider storing the web UI in a SPIFFS or LittleFS partition. This allows over-the-air UI updates independent of firmware, enables `gzip` pre-compression, and frees `.rodata` for code. If flash partitioning is not yet available, at least wrap the string in `#ifdef CONFIG_FRENEL_EMBED_WEB_UI` so it can be excluded for headless builds.

---

### [NITPICK] `config_manager` stores Wi-Fi credentials in the same NVS namespace as animation settings

**File:** `components/config_manager/config_manager.c` (line 19)

```c
#define NVS_NAMESPACE "fresnel"
```

While the header comments note that credentials are "NOT exposed via IPC," they are still co-located in the same NVS namespace. A future factory-reset feature that erases `"fresnel"` would also wipe Wi-Fi credentials, forcing the user to re-provision the device.

**Recommended fix:** Use a separate NVS namespace (e.g., `"fresnel_wifi"`) for credentials. This allows independent erase policies and aligns with ESP-IDF security best practices.

---

### [NITPICK] `wifi_monitor_task` in `main.c` duplicates RSSI retrieval logic already present in `http_server.c`

**File:** `main/main.c` (lines 16–30), `components/http_server/http_server.c` (lines 20–29)

Both `wifi_monitor_task` and `get_rssi()` call `esp_wifi_sta_get_ap_info()`. This is minor duplication, but if the logic evolves (e.g., smoothing RSSI over time, or returning `INT_MIN` when disconnected), both sites must be updated.

**Recommended fix:** Move `get_rssi()` into `wifi_manager.c` and expose `wifi_manager_get_rssi()` in `wifi_manager.h`. Let `http_server` and `wifi_monitor_task` consume the unified API.

---

### [NITPICK] `ipc_cmd_t` uses an anonymous union without `__attribute__((packed))` or explicit padding rules

**File:** `components/ipc/include/ipc.h` (lines 21–29)

```c
typedef struct {
    ipc_cmd_type_t type;
    union {
        float    speed_rpm;
        uint32_t color_rgb;
        int32_t  mode;
        float    brightness;
    } data;
} ipc_cmd_t;
```

On ESP32 (Xtensa) with default `-mlongcalls`, `float` and `int32_t` are both 4 bytes, so the union size is 4 bytes and the struct is likely 8 bytes with natural alignment. However, `ipc_cmd_type_t` is an `enum` (typically 4 bytes on ESP-IDF). If the enum width ever changes (e.g., compiler flag `-fshort-enums`), the struct layout may shift, breaking binary compatibility between sender and receiver.

**Recommended fix:** Add `__attribute__((packed))` to `ipc_cmd_t` and assert `sizeof(ipc_cmd_t) == 8` at compile time or in `ipc_init()`. Alternatively, switch to a fixed-width type for `type` (`uint32_t` instead of `enum`) to guarantee layout.

---

## Conclusion

Phase 2 establishes a solid component skeleton, but three architectural debts should be addressed before Phase 3 scaling:

1. **Refactor IPC/LED coupling** — move `led_mutex` out of the generic IPC component and adopt double-buffering in the LED driver to shrink critical sections.
2. **Fix the config persistence race** — either synchronise NVS writes with IPC command consumption, or bypass IPC for config updates and let the animation task poll.
3. **Introduce an event/callback abstraction in `wifi_manager`** — so that `http_server` and future network consumers react cleanly to STA↔AP transitions without hard-coded delays or silent socket drops.

Addressing these items will improve cohesion, reduce coupling, and provide headroom for new animation modes, sensor inputs, and cloud connectivity in subsequent phases.
