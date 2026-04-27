#pragma once

#include <stdint.h>
#include "esp_err.h"

// Waveshare ESP32-S3-Matrix: GPIO39, 8x8 WS2812B
#define LED_MATRIX_GPIO   39
#define LED_MATRIX_COLS   8
#define LED_MATRIX_ROWS   8
#define LED_MATRIX_LEN    (LED_MATRIX_COLS * LED_MATRIX_ROWS)

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

void      led_driver_init(void);
esp_err_t led_driver_set_pixel(uint8_t index, rgb_t color);
esp_err_t led_driver_flush(void);
void      led_driver_clear(void);
