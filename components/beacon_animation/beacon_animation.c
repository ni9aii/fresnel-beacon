#include "beacon_animation.h"
#include "led_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define FRAME_MS        33      // ~30 fps
#define RPM             8.0f   // beacon rotation speed

// Beam trail: how many radians behind the leading edge stay lit
#define TRAIL_RADIANS   (M_PI / 2.5f)

// Beam color: warm amber, classic lighthouse
#define BEAM_R  255
#define BEAM_G  160
#define BEAM_B   40

// Waveshare ESP32-S3-Matrix wires rows in serpentine order
static inline uint8_t pixel_index(int x, int y)
{
    return (y % 2 == 0)
        ? (uint8_t)(y * LED_MATRIX_COLS + x)
        : (uint8_t)(y * LED_MATRIX_COLS + (LED_MATRIX_COLS - 1 - x));
}

void beacon_animation_task(void *arg)
{
    const float cx    = (LED_MATRIX_COLS - 1) / 2.0f;
    const float cy    = (LED_MATRIX_ROWS - 1) / 2.0f;
    const float omega = 2.0f * (float)M_PI * RPM / 60.0f; // rad/s
    const float dt    = FRAME_MS / 1000.0f;

    float angle = 0.0f;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        led_driver_clear();

        for (int y = 0; y < LED_MATRIX_ROWS; y++) {
            for (int x = 0; x < LED_MATRIX_COLS; x++) {
                float dx = x - cx;
                float dy = y - cy;
                if (dx == 0.0f && dy == 0.0f) continue;

                float pixel_angle = atan2f(dy, dx);

                // Angular distance behind the beam leading edge
                float diff = angle - pixel_angle;
                while (diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
                while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;

                if (diff < 0.0f || diff > TRAIL_RADIANS) continue;

                // Quadratic falloff from leading edge → trail tip
                float t = 1.0f - (diff / TRAIL_RADIANS);
                float brightness = t * t;

                rgb_t color = {
                    .r = (uint8_t)(BEAM_R * brightness),
                    .g = (uint8_t)(BEAM_G * brightness),
                    .b = (uint8_t)(BEAM_B * brightness),
                };
                led_driver_set_pixel(pixel_index(x, y), color);
            }
        }

        led_driver_flush();

        angle += omega * dt;
        if (angle > (float)M_PI) angle -= 2.0f * (float)M_PI;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FRAME_MS));
    }
}
