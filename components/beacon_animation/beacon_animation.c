#include "beacon_animation.h"
#include "beacon_math.h"
#include "renderer.h"
#include "ipc.h"
#include "config_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#define FRAME_MS     33    // ~30 fps
#define STACK_LOG_MS 30000 // stack log interval (30s)

#define BEACON_SPEED_MIN_SEC     0.5f  // seconds per rotation (fastest)
#define BEACON_SPEED_MAX_SEC     20.0f // seconds per rotation (slowest)
#define BEACON_SPEED_DEFAULT_SEC 1.0f

// Beam trail: how many radians behind the leading edge stay lit
#define TRAIL_RADIANS (M_PI / 2.5f)

// Strobe timing
#define STROBE_ON_MS  50  // 50ms on
#define STROBE_OFF_MS 100 // 100ms off (200ms period)

static inline rgb_t unpack_rgb(uint32_t rgb) {
    return (rgb_t){
        .r = (uint8_t) ((rgb >> 16) & 0xFF),
        .g = (uint8_t) ((rgb >> 8) & 0xFF),
        .b = (uint8_t) (rgb & 0xFF),
    };
}

/**
 * @brief Convert speed (seconds per rotation) to RPM.
 */
static inline float sec_per_rotation_to_rpm(float sec) {
    if (sec <= 0.0f)
        return 60.0f;
    return 60.0f / sec;
}

/**
 * @brief Convert RPM to seconds per rotation.
 */
static inline float rpm_to_sec_per_rotation(float rpm) {
    if (rpm <= 0.0f)
        return 1.0f;
    return 60.0f / rpm;
}

static void process_ipc_commands(void) {
    ipc_cmd_t cmd;
    bool commit_pending = false;
    while (xQueueReceive(ipc_queue, &cmd, 0) == pdTRUE) {
        switch (cmd.type) {
        case IPC_CMD_SET_SPEED:
            config_manager_set_speed(cmd.data.speed_rpm);
            ESP_LOGI("beacon", "Speed updated: %.2f sec/rotation",
                     rpm_to_sec_per_rotation(cmd.data.speed_rpm));
            break;
        case IPC_CMD_SET_COLOR:
            config_manager_set_color(cmd.data.color_rgb);
            break;
        case IPC_CMD_SET_MODE:
            config_manager_set_mode(cmd.data.mode);
            break;
        case IPC_CMD_SET_BRIGHTNESS:
            config_manager_set_brightness(cmd.data.brightness);
            break;
        case IPC_CMD_COMMIT:
            commit_pending = true;
            break;
        default:
            break;
        }
    }
    if (commit_pending) {
        ipc_signal_commit();
    }
}

/* ---- Mode-specific render functions ---- */

/**
 * @brief Render BEACON mode: rotating beacon with trail.
 */
static void render_beacon_mode(renderer_t *renderer, float angle, const runtime_config_t *cfg) {
    const float cx = (LED_MATRIX_COLS - 1) / 2.0f;
    const float cy = (LED_MATRIX_ROWS - 1) / 2.0f;
    rgb_t beam_color = unpack_rgb(cfg->color_rgb);
    const float omega = 2.0f * (float) M_PI * cfg->speed_rpm / 60.0f; // rad/s

    for (int y = 0; y < LED_MATRIX_ROWS; y++) {
        for (int x = 0; x < LED_MATRIX_COLS; x++) {
            float dx = x - cx;
            float dy = y - cy;
            if (dx == 0.0f && dy == 0.0f)
                continue;

            float pixel_angle = atan2f(dy, dx);
            float diff = angle_diff(angle, pixel_angle);

            if (diff < 0.0f || diff > TRAIL_RADIANS)
                continue;

            float t = 1.0f - (diff / TRAIL_RADIANS);
            float brightness = t * t * cfg->brightness;
            if (brightness > 1.0f)
                brightness = 1.0f;

            rgb_t color = {
                .r = (uint8_t) (beam_color.r * brightness),
                .g = (uint8_t) (beam_color.g * brightness),
                .b = (uint8_t) (beam_color.b * brightness),
            };
            renderer_set_pixel(renderer, pixel_index(x, y), color);
        }
    }
}

/**
 * @brief Render STROBE mode: flashing strobe effect.
 */
static void render_strobe_mode(renderer_t *renderer, uint32_t frame_count,
                               const runtime_config_t *cfg) {
    static const char *TAG = "strobe";
    rgb_t color = unpack_rgb(cfg->color_rgb);

    // 200ms period: 50ms on, 100ms off
    uint32_t period_ms = STROBE_ON_MS + STROBE_OFF_MS;
    uint32_t phase = (frame_count * FRAME_MS) % period_ms;

    bool should_light = (phase < STROBE_ON_MS);

    if (should_light) {
        for (int i = 0; i < LED_MATRIX_ROWS * LED_MATRIX_COLS; i++) {
            renderer_set_pixel(renderer, (uint8_t) i, color);
        }
    } else {
        renderer_clear(renderer);
    }
}

/**
 * @brief Render AMBIENT mode: static ambient color.
 */
static void render_ambient_mode(renderer_t *renderer, const runtime_config_t *cfg) {
    rgb_t color = unpack_rgb(cfg->color_rgb);

    // Apply brightness to static color
    color.r = (uint8_t) (color.r * cfg->brightness);
    color.g = (uint8_t) (color.g * cfg->brightness);
    color.b = (uint8_t) (color.b * cfg->brightness);

    for (int i = 0; i < LED_MATRIX_ROWS * LED_MATRIX_COLS; i++) {
        renderer_set_pixel(renderer, (uint8_t) i, color);
    }
}

/**
 * @brief Render OFF mode: all LEDs off.
 */
static void render_off_mode(renderer_t *renderer) {
    renderer_clear(renderer);
}

/* ---- Main animation task ---- */

esp_err_t beacon_animation_init(void) {
    runtime_config_t cfg = CONFIG_MANAGER_DEFAULTS();
    float speed_sec = BEACON_SPEED_DEFAULT_SEC;
    if (config_manager_get(&cfg) == ESP_OK) {
        speed_sec = rpm_to_sec_per_rotation(cfg.speed_rpm);
        if (speed_sec < BEACON_SPEED_MIN_SEC)
            speed_sec = BEACON_SPEED_MIN_SEC;
        if (speed_sec > BEACON_SPEED_MAX_SEC)
            speed_sec = BEACON_SPEED_MAX_SEC;
    }
    ESP_LOGI("beacon", "Beacon speed initialized: %.2f sec/rotation", speed_sec);
    return ESP_OK;
}

void beacon_animation_task(void *arg) {
    animation_config_t *anim_cfg = (animation_config_t *) arg;
    renderer_t *renderer = anim_cfg ? anim_cfg->renderer : NULL;
    if (renderer == NULL) {
        ESP_LOGE("beacon", "No renderer provided to animation task");
        vTaskDelete(NULL);
        return;
    }

    static const char *TAG = "beacon";

    float angle = 0.0f;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t iter = 0;

    /* Skip watchdog registration in Wokwi simulation */
#ifndef WOKWI_SIMULATION
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI(TAG, "Task watchdog registered (current task)");
#endif

    while (1) {
#ifndef WOKWI_SIMULATION
        esp_task_wdt_reset();
#endif

        /* Drain IPC command queue (non-blocking) */
        if (ipc_queue != NULL) {
            process_ipc_commands();
        }

        /* Read current runtime config */
        runtime_config_t cfg = CONFIG_MANAGER_DEFAULTS();
        if (config_manager_get(&cfg) != ESP_OK) {
            ESP_LOGW(TAG, "config_manager_get failed, using defaults");
        }

        /* Render based on current mode */
        animation_mode_t mode = (animation_mode_t) cfg.mode;

        switch (mode) {
        case ANIM_MODE_BEACON: {
            const float omega = 2.0f * (float) M_PI * cfg.speed_rpm / 60.0f; // rad/s
            const float dt = FRAME_MS / 1000.0f;
            render_beacon_mode(renderer, angle, &cfg);
            angle += omega * dt;
            if (angle > (float) M_PI)
                angle -= 2.0f * (float) M_PI;
            else if (angle < -(float) M_PI)
                angle += 2.0f * (float) M_PI;
            break;
        }
        case ANIM_MODE_STROBE:
            render_strobe_mode(renderer, iter, &cfg);
            break;
        case ANIM_MODE_AMBIENT:
            render_ambient_mode(renderer, &cfg);
            break;
        case ANIM_MODE_OFF:
            render_off_mode(renderer);
            break;
        default:
            ESP_LOGW(TAG, "Unknown mode %d, falling back to BEACON", mode);
            const float omega = 2.0f * (float) M_PI * cfg.speed_rpm / 60.0f;
            const float dt = FRAME_MS / 1000.0f;
            render_beacon_mode(renderer, angle, &cfg);
            angle += omega * dt;
            if (angle > (float) M_PI)
                angle -= 2.0f * (float) M_PI;
            else if (angle < -(float) M_PI)
                angle += 2.0f * (float) M_PI;
            break;
        }

        esp_err_t flush_ret = renderer_flush(renderer);
        if (flush_ret != ESP_OK) {
            ESP_LOGE(TAG, "renderer_flush failed: %s", esp_err_to_name(flush_ret));
        }

        iter++;
        if (iter * FRAME_MS >= STACK_LOG_MS) {
            ESP_LOGI(TAG, "Stack high water mark: %u", uxTaskGetStackHighWaterMark(NULL));
            iter = 0;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FRAME_MS));
    }

    /* Task exit cleanup (should never reach here in normal operation) */
#ifndef WOKWI_SIMULATION
    ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));
    ESP_LOGI(TAG, "Task watchdog unregistered");
#endif
    vTaskDelete(NULL);
}