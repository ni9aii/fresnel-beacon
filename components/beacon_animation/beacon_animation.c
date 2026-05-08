#include "beacon_animation.h"
#include "beacon_math.h"
#include "led_driver.h"
#include "ipc.h"
#include "config_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#define FRAME_MS        33      // ~30 fps
#define STACK_LOG_MS    30000   // stack log interval (30s)

// Beam trail: how many radians behind the leading edge stay lit
#define TRAIL_RADIANS   (M_PI / 2.5f)

static inline rgb_t unpack_rgb(uint32_t rgb)
{
    return (rgb_t){
        .r = (uint8_t)((rgb >> 16) & 0xFF),
        .g = (uint8_t)((rgb >>  8) & 0xFF),
        .b = (uint8_t)( rgb        & 0xFF),
    };
}

static void process_ipc_commands(void)
{
    ipc_cmd_t cmd;
    bool commit_pending = false;
    while (xQueueReceive(ipc_queue, &cmd, 0) == pdTRUE) {
        switch (cmd.type) {
            case IPC_CMD_SET_SPEED:
                config_manager_set_speed(cmd.data.speed_rpm);
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

void beacon_animation_task(void *arg)
{
    const float cx    = (LED_MATRIX_COLS - 1) / 2.0f;
    const float cy    = (LED_MATRIX_ROWS - 1) / 2.0f;
    const float dt    = FRAME_MS / 1000.0f;
    static const char *TAG = "beacon";

    float angle = 0.0f;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t iter = 0;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI(TAG, "Task watchdog registered (current task)");

    while (1) {
        /* Drain IPC command queue (non-blocking) */
        if (ipc_queue != NULL) {
            process_ipc_commands();
        }

        /* Read current runtime config */
        runtime_config_t cfg = {
            .speed_rpm   = 8.0f,
            .mode        = 0,
            .brightness  = 1.0f,
            .color_rgb   = 0xFFA028,
        };
        if (config_manager_get(&cfg) != ESP_OK) {
            ESP_LOGW(TAG, "config_manager_get failed, using defaults");
        }

        const float omega = 2.0f * (float)M_PI * cfg.speed_rpm / 60.0f; // rad/s
        rgb_t beam_color  = unpack_rgb(cfg.color_rgb);

        led_driver_clear();

        for (int y = 0; y < LED_MATRIX_ROWS; y++) {
            for (int x = 0; x < LED_MATRIX_COLS; x++) {
                float dx = x - cx;
                float dy = y - cy;
                if (dx == 0.0f && dy == 0.0f) continue;

                float pixel_angle = atan2f(dy, dx);

                float diff = angle_diff(angle, pixel_angle);

                if (diff < 0.0f || diff > TRAIL_RADIANS) continue;

                // Quadratic falloff from leading edge → trail tip
                float t = 1.0f - (diff / TRAIL_RADIANS);
                float brightness = t * t * cfg.brightness;
                if (brightness > 1.0f) brightness = 1.0f;

                rgb_t color = {
                    .r = (uint8_t)(beam_color.r * brightness),
                    .g = (uint8_t)(beam_color.g * brightness),
                    .b = (uint8_t)(beam_color.b * brightness),
                };
                led_driver_set_pixel(pixel_index(x, y), color);
            }
        }

        esp_task_wdt_reset();

        esp_err_t flush_ret = led_driver_flush();
        if (flush_ret != ESP_OK) {
            ESP_LOGE(TAG, "led_driver_flush failed: %s", esp_err_to_name(flush_ret));
        }

        angle += omega * dt;
        if      (angle >  (float)M_PI) angle -= 2.0f * (float)M_PI;
        else if (angle < -(float)M_PI) angle += 2.0f * (float)M_PI;

        iter++;
        if (iter * FRAME_MS >= STACK_LOG_MS) {
            ESP_LOGI(TAG, "Stack high water mark: %u", uxTaskGetStackHighWaterMark(NULL));
            iter = 0;
        }

        esp_task_wdt_reset();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FRAME_MS));
    }
}
